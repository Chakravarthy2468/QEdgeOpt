/// @file QIEA.cpp
/// @brief Quantum-Inspired Evolutionary Algorithm implementation.
///
/// ═══════════════════════════════════════════════════════════
/// ACADEMIC NOTE — QUANTUM-INSPIRED, NOT QUANTUM
/// ═══════════════════════════════════════════════════════════
/// This algorithm runs entirely on a classical CPU.
/// The "quantum" aspect is a PROBABILISTIC REPRESENTATION inspired
/// by quantum computing concepts:
///
///   Q-bit representation:
///     q = (alpha, beta)  where  alpha² + beta² = 1
///     P(|0⟩) = alpha²,   P(|1⟩) = beta²
///
///   This is equivalent to a point on the unit circle parameterised
///   by angle θ:  alpha = cos(θ),  beta = sin(θ).
///
///   Measurement:
///     Generate a random number r ∈ [0,1).
///     If r < beta², observe state 1; else observe state 0.
///
///   Rotation gate (update):
///     θ_new = θ_old + Δθ · sign
///     where sign is determined by comparing the current observed
///     solution to the best-known solution. This drives the
///     probability distribution toward better solutions.
///
///   This is analogous to quantum annealing but uses classical
///   floating-point arithmetic exclusively.
///
/// ═══════════════════════════════════════════════════════════
/// ENCODING
/// ═══════════════════════════════════════════════════════════
///   - Each node index is encoded in ceil(log2(N)) bits.
///   - For S services, the chromosome has S × bitsPerNode Q-bits.
///   - Measurement collapses each Q-bit to 0 or 1.
///   - The binary string is decoded to node indices.
///   - If a decoded index ≥ N, we take modulo N.
///   - A repair step fixes capacity violations.
///
/// ═══════════════════════════════════════════════════════════
/// ALGORITHM FLOW
/// ═══════════════════════════════════════════════════════════
///   1. Initialise Q-bit population (uniform superposition)
///   2. FOR each generation:
///      a. Measure Q-bit states → binary strings → placements
///      b. Repair infeasible placements
///      c. Evaluate fitness
///      d. Update best-known solution (elitism)
///      e. Rotate Q-bits toward best solution (rotation gate)
///      f. Apply random mutation for diversity
///      g. Check diversity; re-initialise if stagnant
///   3. Return global best placement
///
/// Time complexity: O(G · P · (L + S·N + S·E))
///   G = generations, P = population, L = chromosome length,
///   S = services, N = nodes, E = communication edges

#include "algorithms/QIEA.h"
#include "evaluation/Fitness.h"
#include "evaluation/ConstraintChecker.h"
#include "utils/Timer.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_set>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

namespace qedgeopt {

// ═══════════════════════════════════════════════════════════
// ENCODING HELPERS
// ═══════════════════════════════════════════════════════════

int QIEA::bits_per_node(int num_nodes) const
{
    if (num_nodes <= 1) return 1;
    int bits = 0;
    int val = num_nodes - 1;
    while (val > 0) { bits++; val >>= 1; }
    return bits;
}

int QIEA::chromosome_length(int num_services, int num_nodes) const
{
    return num_services * bits_per_node(num_nodes);
}

// ═══════════════════════════════════════════════════════════
// INITIALISATION
// ═══════════════════════════════════════════════════════════

void QIEA::initialise_population(std::vector<QIndividual>& pop,
                                 int pop_size, int chrom_len,
                                 Random& rng)
{
    (void)rng;
    pop.resize(pop_size);
    for (auto& ind : pop) {
        ind.qbits.resize(chrom_len);
        // Start in uniform superposition: alpha = beta = 1/√2
        // This gives P(0) = P(1) = 0.5 — maximum uncertainty
        for (auto& qb : ind.qbits) {
            qb.alpha = M_SQRT1_2;
            qb.beta  = M_SQRT1_2;
        }
        ind.fitness = 1e18;
    }
}

// ═══════════════════════════════════════════════════════════
// MEASUREMENT (Q-bit collapse)
// ═══════════════════════════════════════════════════════════

Placement QIEA::measure(const QIndividual& ind, int num_services,
                         int num_nodes, int bpn, Random& rng)
{
    int L = static_cast<int>(ind.qbits.size());
    std::vector<int> bits(L);

    // Collapse each Q-bit probabilistically
    for (int b = 0; b < L; ++b) {
        double p1 = ind.qbits[b].prob_one();  // P(|1⟩) = beta²
        bits[b] = rng.uniform() < p1 ? 1 : 0;
    }

    return decode_binary(bits, num_services, num_nodes, bpn);
}

Placement QIEA::decode_binary(const std::vector<int>& bits,
                               int num_services, int num_nodes, int bpn)
{
    Placement placement(num_services);
    for (int i = 0; i < num_services; ++i) {
        int value = 0;
        int offset = i * bpn;
        for (int b = 0; b < bpn; ++b) {
            value = (value << 1) | bits[offset + b];
        }
        // Modulo to handle cases where 2^bpn > num_nodes
        placement[i] = value % num_nodes;
    }
    return placement;
}

// ═══════════════════════════════════════════════════════════
// ROTATION GATE (Q-bit update)
// ═══════════════════════════════════════════════════════════
///
/// The rotation gate is the core "quantum-inspired" operation.
/// For each Q-bit position, we compare the observed bit to the
/// corresponding bit in the best-known solution.
///
/// If they differ, we rotate the Q-bit angle toward the best
/// solution's bit value. The rotation direction is determined
/// by the sign convention from Han & Kim (2002).
///
/// Rotation matrix:
///   [cos(Δθ)  -sin(Δθ)] [alpha]
///   [sin(Δθ)   cos(Δθ)] [beta ]
///

void QIEA::rotate_qbits(QIndividual& ind,
                         const Placement& best_placement,
                         int num_nodes, int bpn,
                         double delta_theta)
{
    (void)num_nodes;
    int num_services = static_cast<int>(best_placement.size());

    // Encode best placement into binary
    std::vector<int> best_bits(ind.qbits.size(), 0);
    for (int i = 0; i < num_services; ++i) {
        int val = best_placement[i];
        int offset = i * bpn;
        for (int b = bpn - 1; b >= 0; --b) {
            best_bits[offset + b] = val & 1;
            val >>= 1;
        }
    }

    // Encode current observed placement into binary
    std::vector<int> cur_bits(ind.qbits.size(), 0);
    for (int i = 0; i < num_services; ++i) {
        int val = ind.observed[i];
        int offset = i * bpn;
        for (int b = bpn - 1; b >= 0; --b) {
            cur_bits[offset + b] = val & 1;
            val >>= 1;
        }
    }

    // Rotate each Q-bit toward the best solution
    for (int b = 0; b < static_cast<int>(ind.qbits.size()); ++b) {
        int x_i = cur_bits[b];   // current observed bit
        int b_i = best_bits[b];  // best-known bit

        if (x_i == b_i) continue;  // already aligned — no rotation needed

        // Determine rotation direction using the Han & Kim lookup table
        double alpha = ind.qbits[b].alpha;
        double beta  = ind.qbits[b].beta;
        double sign = 0;

        // Direction lookup (simplified from Han & Kim 2002):
        // Goal: rotate toward P(b_i) increasing
        if (b_i == 1) {
            // We want beta² to increase
            if (alpha * beta > 0) sign = +1;
            else if (alpha * beta < 0) sign = -1;
            else sign = (beta < 0) ? +1 : ((alpha > 0) ? +1 : -1);
        } else {
            // We want alpha² to increase
            if (alpha * beta > 0) sign = -1;
            else if (alpha * beta < 0) sign = +1;
            else sign = (alpha < 0) ? +1 : ((beta > 0) ? -1 : +1);
        }

        double dtheta = sign * delta_theta;

        // Apply rotation matrix
        double cos_dt = std::cos(dtheta);
        double sin_dt = std::sin(dtheta);
        double new_alpha = cos_dt * alpha - sin_dt * beta;
        double new_beta  = sin_dt * alpha + cos_dt * beta;

        ind.qbits[b].alpha = new_alpha;
        ind.qbits[b].beta  = new_beta;
        ind.qbits[b].normalise();
    }
}

// ═══════════════════════════════════════════════════════════
// MUTATION (diversity maintenance)
// ═══════════════════════════════════════════════════════════

void QIEA::mutate_qbits(QIndividual& ind, double mut_prob, Random& rng)
{
    for (auto& qb : ind.qbits) {
        if (rng.uniform() < mut_prob) {
            // Random perturbation of the angle
            double theta = qb.theta();
            theta += rng.uniform(-0.1 * M_PI, 0.1 * M_PI);
            qb.set_theta(theta);
            qb.normalise();
        }
    }
}

// ═══════════════════════════════════════════════════════════
// DIVERSITY MEASUREMENT
// ═══════════════════════════════════════════════════════════

double QIEA::measure_diversity(const std::vector<QIndividual>& pop)
{
    if (pop.size() <= 1) return 1.0;

    // Count distinct placements
    std::set<Placement> unique_placements;
    for (const auto& ind : pop)
        unique_placements.insert(ind.observed);

    return static_cast<double>(unique_placements.size()) / pop.size();
}

void QIEA::enforce_diversity(std::vector<QIndividual>& pop,
                             int chrom_len, double threshold,
                             Random& rng)
{
    (void)chrom_len;
    if (measure_diversity(pop) >= threshold) return;

    // Re-initialise the bottom half of the population
    int half = static_cast<int>(pop.size()) / 2;
    for (int p = half; p < static_cast<int>(pop.size()); ++p) {
        for (auto& qb : pop[p].qbits) {
            double theta = rng.uniform(0.0, M_PI / 2.0);
            qb.set_theta(theta);
        }
    }
}

// ═══════════════════════════════════════════════════════════
// REPAIR
// ═══════════════════════════════════════════════════════════

Placement QIEA::repair(const Dataset& ds, Placement placement, Random& rng)
{
    (void)rng;
    int S = ds.num_services();
    int N = ds.num_nodes();

    // Clamp out-of-range node indices
    for (int i = 0; i < S; ++i) {
        if (placement[i] < 0 || placement[i] >= N)
            placement[i] = placement[i] % N;
        if (placement[i] < 0) placement[i] += N;
    }

    auto usage = ConstraintChecker::compute_node_usage(ds, placement);

    // Iteratively fix overloaded nodes
    int max_iter = S * 2;  // prevent infinite loops
    for (int iter = 0; iter < max_iter; ++iter) {
        // Find the most overloaded node
        int worst_node = -1;
        double worst_excess = 0;
        for (int j = 0; j < N; ++j) {
            double excess = usage[j].cpu_excess() + usage[j].ram_excess() + usage[j].bw_excess();
            if (excess > worst_excess) {
                worst_excess = excess;
                worst_node = j;
            }
        }
        if (worst_node < 0) break;  // all feasible

        // Find the service on this node with highest demand
        int move_svc = -1;
        double move_demand = -1;
        for (int i = 0; i < S; ++i) {
            if (placement[i] != worst_node) continue;
            double d = ds.services[i].aggregate_demand();
            if (d > move_demand) { move_demand = d; move_svc = i; }
        }
        if (move_svc < 0) break;

        // Find best alternative node (least communication cost increase)
        int best_alt = -1;
        double best_score = 1e18;
        for (int k = 0; k < N; ++k) {
            if (k == worst_node) continue;
            if (!ConstraintChecker::can_fit(ds, usage, move_svc, k)) continue;

            double score = 0;
            for (const auto& [nbr, traffic] : ds.adjacency[move_svc]) {
                if (placement[nbr] >= 0)
                    score += traffic * ds.latency_between(k, placement[nbr]);
            }
            if (score < best_score) { best_score = score; best_alt = k; }
        }

        if (best_alt < 0) {
            // Fallback: least loaded node
            double min_load = 1e18;
            for (int k = 0; k < N; ++k) {
                if (k == worst_node) continue;
                double load = usage[k].cpu_used / std::max(usage[k].cpu_cap, 1.0);
                if (load < min_load) { min_load = load; best_alt = k; }
            }
        }
        if (best_alt < 0) break;

        // Execute move
        const auto& svc = ds.services[move_svc];
        usage[worst_node].cpu_used -= svc.cpu_demand;
        usage[worst_node].ram_used -= svc.ram_demand;
        usage[worst_node].bw_used  -= svc.bw_demand;
        usage[best_alt].cpu_used   += svc.cpu_demand;
        usage[best_alt].ram_used   += svc.ram_demand;
        usage[best_alt].bw_used    += svc.bw_demand;
        placement[move_svc] = best_alt;
    }

    return placement;
}

// ═══════════════════════════════════════════════════════════
// MAIN SOLVE
// ═══════════════════════════════════════════════════════════

Solution QIEA::solve(const Dataset& ds,
                     const AlgorithmConfig& config,
                     ProgressCallback callback)
{
    Timer timer;
    Random rng(config.seed);

    const auto& qc = config.qiea;
    int S = ds.num_services();
    int N = ds.num_nodes();
    int bpn = bits_per_node(N);
    int L = chromosome_length(S, N);
    int P = qc.population_size;
    int G = qc.max_generations;

    // ── Step 1: Initialise Q-bit population ──────────────
    std::vector<QIndividual> pop;
    initialise_population(pop, P, L, rng);

    // Global best
    Placement global_best_placement(S, 0);
    double global_best_fitness = 1e18;
    std::vector<double> convergence;
    convergence.reserve(G);

    // ── Step 2: Generational loop ────────────────────────
    for (int gen = 0; gen < G; ++gen) {

        // ── 2a: Measure Q-bit states → placements ────────
        for (auto& ind : pop) {
            ind.observed = measure(ind, S, N, bpn, rng);

            // ── 2b: Repair infeasible placements ─────────
            if (qc.enable_repair)
                ind.observed = repair(ds, ind.observed, rng);

            // ── 2c: Evaluate fitness ─────────────────────
            ind.fitness = Fitness::quick_evaluate(ds, ind.observed, config.weights);
        }

        // ── 2d: Update best-known (elitism) ──────────────
        for (const auto& ind : pop) {
            if (ind.fitness < global_best_fitness) {
                global_best_fitness = ind.fitness;
                global_best_placement = ind.observed;
            }
        }

        convergence.push_back(global_best_fitness);

        // ── 2e: Rotate Q-bits toward best solution ───────
        for (auto& ind : pop) {
            // Don't rotate the elite individuals (preserve exploration)
            rotate_qbits(ind, global_best_placement, N, bpn, qc.rotation_angle);
        }

        // ── 2f: Apply random mutation for diversity ──────
        for (auto& ind : pop) {
            mutate_qbits(ind, qc.mutation_prob, rng);
        }

        // ── 2g: Diversity control ────────────────────────
        if (qc.enable_diversity) {
            enforce_diversity(pop, L, qc.diversity_threshold, rng);
        }

        // ── Progress callback ────────────────────────────
        if (callback) {
            auto report = ConstraintChecker::check(ds, global_best_placement);
            int feas_count = 0;
            for (const auto& ind : pop) {
                auto r = ConstraintChecker::check(ds, ind.observed);
                if (r.all_feasible) feas_count++;
            }
            double feas_rate = static_cast<double>(feas_count) / P;

            std::set<int> active;
            for (int i = 0; i < S; ++i)
                if (global_best_placement[i] >= 0) active.insert(global_best_placement[i]);

            callback(gen + 1, global_best_fitness, feas_rate,
                     report.sla_violations, static_cast<int>(active.size()));
        }
    }

    // ── Step 3: Build final solution ─────────────────────
    auto fitness = Fitness::evaluate(ds, global_best_placement, config.weights);

    Solution sol;
    sol.placement       = global_best_placement;
    sol.fitness          = fitness;
    sol.status           = fitness.is_feasible() ? SolutionStatus::FEASIBLE : SolutionStatus::INFEASIBLE;
    sol.algorithm        = AlgorithmType::QIEA;
    sol.runtime_ms       = timer.elapsed_ms();
    sol.generations_run  = G;
    sol.seed_used        = config.seed;
    sol.convergence      = convergence;

    return sol;
}

}  // namespace qedgeopt
