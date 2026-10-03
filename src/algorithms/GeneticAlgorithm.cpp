/// @file GeneticAlgorithm.cpp
/// @brief Classical Genetic Algorithm baseline for microservice placement.
///
/// This is a genuine, independent GA implementation — NOT a wrapper around QIEA.
///
/// Representation: placement[i] = node_id (direct integer encoding)
///
/// Operators:
///   - Random initialisation with capacity-aware bias
///   - Tournament selection (configurable k)
///   - Uniform crossover
///   - Point mutation (random service re-assignment)
///   - Elitism (top-k preserved unchanged)
///   - Deterministic + heuristic repair
///
/// Time complexity: O(G · P · (S·N + S·E))
///   G = generations, P = population, S = services, N = nodes, E = edges

#include "algorithms/GeneticAlgorithm.h"
#include "evaluation/Fitness.h"
#include "evaluation/ConstraintChecker.h"
#include "utils/Timer.h"

#include <algorithm>
#include <numeric>
#include <set>

namespace qedgeopt {

// ── Random individual ────────────────────────────────────

GAIndividual GeneticAlgorithm::random_individual(const Dataset& ds, Random& rng)
{
    int S = ds.num_services();
    int N = ds.num_nodes();
    GAIndividual ind;
    ind.placement.resize(S);
    for (int i = 0; i < S; ++i) {
        ind.placement[i] = rng.uniform_int(0, N - 1);
    }
    return ind;
}

// ── Tournament selection ─────────────────────────────────

int GeneticAlgorithm::tournament_select(const std::vector<GAIndividual>& pop,
                                        int tournament_size, Random& rng)
{
    int best = rng.uniform_int(0, static_cast<int>(pop.size()) - 1);
    for (int t = 1; t < tournament_size; ++t) {
        int candidate = rng.uniform_int(0, static_cast<int>(pop.size()) - 1);
        if (pop[candidate].fitness < pop[best].fitness)
            best = candidate;
    }
    return best;
}

// ── Crossover ────────────────────────────────────────────

Placement GeneticAlgorithm::crossover(const Placement& a, const Placement& b, Random& rng)
{
    int S = static_cast<int>(a.size());
    Placement child(S);
    for (int i = 0; i < S; ++i) {
        child[i] = rng.bernoulli(0.5) ? a[i] : b[i];
    }
    return child;
}

// ── Mutation ─────────────────────────────────────────────

Placement GeneticAlgorithm::mutate(const Placement& p, int num_nodes,
                                   double mut_prob, Random& rng)
{
    Placement result = p;
    for (int i = 0; i < static_cast<int>(result.size()); ++i) {
        if (rng.uniform() < mut_prob) {
            result[i] = rng.uniform_int(0, num_nodes - 1);
        }
    }
    return result;
}

// ── Repair ───────────────────────────────────────────────

Placement GeneticAlgorithm::repair(const Dataset& ds, Placement placement, Random& rng)
{
    (void)rng;
    int S = ds.num_services();
    int N = ds.num_nodes();

    // Compute current usage
    auto usage = ConstraintChecker::compute_node_usage(ds, placement);

    // Find overloaded nodes
    for (int j = 0; j < N; ++j) {
        while (!usage[j].all_ok()) {
            // Find the largest service on this node and try to move it
            int worst_svc = -1;
            double worst_demand = -1;

            for (int i = 0; i < S; ++i) {
                if (placement[i] != j) continue;
                double demand = ds.services[i].aggregate_demand();
                if (demand > worst_demand) {
                    worst_demand = demand;
                    worst_svc = i;
                }
            }

            if (worst_svc < 0) break;  // no service to move (shouldn't happen)

            // Find best alternative node
            int best_alt = -1;
            double best_alt_score = 1e18;

            for (int k = 0; k < N; ++k) {
                if (k == j) continue;
                if (!ConstraintChecker::can_fit(ds, usage, worst_svc, k)) continue;

                // Score: communication cost increase
                double score = 0;
                for (const auto& [neighbor, traffic] : ds.adjacency[worst_svc]) {
                    if (placement[neighbor] >= 0) {
                        score += traffic * ds.latency_between(k, placement[neighbor]);
                    }
                }
                if (score < best_alt_score) {
                    best_alt_score = score;
                    best_alt = k;
                }
            }

            if (best_alt < 0) {
                // No feasible node — assign to least loaded
                double min_load = 1e18;
                for (int k = 0; k < N; ++k) {
                    if (k == j) continue;
                    double load = (usage[k].cpu_used / std::max(usage[k].cpu_cap, 1.0)) +
                                  (usage[k].ram_used / std::max(usage[k].ram_cap, 1.0));
                    if (load < min_load) { min_load = load; best_alt = k; }
                }
            }

            if (best_alt < 0) break;  // truly stuck

            // Move service
            const auto& svc = ds.services[worst_svc];
            usage[j].cpu_used -= svc.cpu_demand;
            usage[j].ram_used -= svc.ram_demand;
            usage[j].bw_used  -= svc.bw_demand;

            usage[best_alt].cpu_used += svc.cpu_demand;
            usage[best_alt].ram_used += svc.ram_demand;
            usage[best_alt].bw_used  += svc.bw_demand;

            placement[worst_svc] = best_alt;
        }
    }

    return placement;
}

// ── Main solve ───────────────────────────────────────────

Solution GeneticAlgorithm::solve(const Dataset& ds,
                                 const AlgorithmConfig& config,
                                 ProgressCallback callback)
{
    Timer timer;
    Random rng(config.seed);

    const auto& gc = config.ga;
    int S = ds.num_services();
    int N = ds.num_nodes();
    int P = gc.population_size;
    int G = gc.max_generations;

    // ── Initialise population ────────────────────────────
    std::vector<GAIndividual> pop(P);
    for (int p = 0; p < P; ++p) {
        pop[p] = random_individual(ds, rng);
        if (gc.enable_repair)
            pop[p].placement = repair(ds, pop[p].placement, rng);
        pop[p].fitness = Fitness::quick_evaluate(ds, pop[p].placement, config.weights);
    }

    // Sort by fitness
    std::sort(pop.begin(), pop.end(),
              [](const GAIndividual& a, const GAIndividual& b) {
                  return a.fitness < b.fitness;
              });

    // Track global best
    GAIndividual global_best = pop[0];
    std::vector<double> convergence;
    convergence.reserve(G);

    // ── Generational loop ────────────────────────────────
    for (int gen = 0; gen < G; ++gen) {
        std::vector<GAIndividual> new_pop;
        new_pop.reserve(P);

        // Elitism: preserve top individuals
        int elite = std::min(gc.elite_count, P);
        for (int e = 0; e < elite; ++e)
            new_pop.push_back(pop[e]);

        // Fill rest with crossover + mutation
        while ((int)new_pop.size() < P) {
            int p1 = tournament_select(pop, gc.tournament_size, rng);
            int p2 = tournament_select(pop, gc.tournament_size, rng);

            Placement child_placement;
            if (rng.uniform() < gc.crossover_prob) {
                child_placement = crossover(pop[p1].placement, pop[p2].placement, rng);
            } else {
                child_placement = pop[p1].placement;  // copy parent
            }

            child_placement = mutate(child_placement, N, gc.mutation_prob, rng);

            if (gc.enable_repair)
                child_placement = repair(ds, child_placement, rng);

            GAIndividual child;
            child.placement = child_placement;
            child.fitness = Fitness::quick_evaluate(ds, child_placement, config.weights);
            new_pop.push_back(child);
        }

        pop = std::move(new_pop);

        // Sort
        std::sort(pop.begin(), pop.end(),
                  [](const GAIndividual& a, const GAIndividual& b) {
                      return a.fitness < b.fitness;
                  });

        // Update global best
        if (pop[0].fitness < global_best.fitness)
            global_best = pop[0];

        convergence.push_back(global_best.fitness);

        // Progress callback
        if (callback) {
            auto report = ConstraintChecker::check(ds, global_best.placement);
            double feas_rate = 0;
            int feas_count = 0;
            for (const auto& ind : pop) {
                auto r = ConstraintChecker::check(ds, ind.placement);
                if (r.all_feasible) feas_count++;
            }
            feas_rate = static_cast<double>(feas_count) / P;

            std::set<int> active;
            for (int i = 0; i < S; ++i)
                if (global_best.placement[i] >= 0) active.insert(global_best.placement[i]);

            callback(gen + 1, global_best.fitness, feas_rate,
                     report.sla_violations, static_cast<int>(active.size()));
        }
    }

    // ── Build solution ───────────────────────────────────
    auto fitness = Fitness::evaluate(ds, global_best.placement, config.weights);

    Solution sol;
    sol.placement       = global_best.placement;
    sol.fitness          = fitness;
    sol.status           = fitness.is_feasible() ? SolutionStatus::FEASIBLE : SolutionStatus::INFEASIBLE;
    sol.algorithm        = AlgorithmType::GA;
    sol.runtime_ms       = timer.elapsed_ms();
    sol.generations_run  = G;
    sol.seed_used        = config.seed;
    sol.convergence      = convergence;

    return sol;
}

}  // namespace qedgeopt
