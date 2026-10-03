/// @file FFD.cpp
/// @brief First-Fit Decreasing greedy baseline implementation.
///
/// Algorithm:
///   1. Sort microservices by aggregate demand (CPU + normalised RAM + normalised BW), descending.
///   2. For each service in sorted order:
///      a. Find the first node with sufficient remaining capacity.
///      b. If SLA-aware mode is enabled, among all feasible nodes pick the one
///         that minimises additional communication cost with already-placed neighbours.
///      c. If no feasible node exists, assign to the least-overloaded node.
///   3. Evaluate the final placement.
///
/// Time complexity: O(S·N) where S = services, N = nodes.
///   - Sorting is O(S log S).
///   - For each of S services, we scan N nodes.

#include "algorithms/FFD.h"
#include "evaluation/Fitness.h"
#include "evaluation/ConstraintChecker.h"
#include "utils/Timer.h"

#include <algorithm>
#include <numeric>
#include <cmath>

namespace qedgeopt {

Solution FFD::solve(const Dataset& ds,
                    const AlgorithmConfig& config,
                    ProgressCallback callback)
{
    Timer timer;

    int S = ds.num_services();
    int N = ds.num_nodes();

    // ── Step 1: Sort services by aggregate demand (decreasing) ──
    std::vector<int> order(S);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return ds.normalised_demand(a) > ds.normalised_demand(b);
    });

    // ── Step 2: Place each service ──────────────────────
    Placement placement(S, -1);
    auto usage = ConstraintChecker::compute_node_usage(ds, placement);
    // Initialise usage capacities
    for (int j = 0; j < N; ++j) {
        usage[j].cpu_cap = ds.nodes[j].cpu_capacity;
        usage[j].ram_cap = ds.nodes[j].ram_capacity;
        usage[j].bw_cap  = ds.nodes[j].bw_capacity;
    }

    for (int idx = 0; idx < S; ++idx) {
        int i = order[idx];
        const auto& svc = ds.services[i];

        int best_node = -1;
        double best_score = 1e18;

        for (int j = 0; j < N; ++j) {
            // Check capacity feasibility
            bool fits = (usage[j].cpu_used + svc.cpu_demand <= usage[j].cpu_cap + 1e-9) &&
                        (usage[j].ram_used + svc.ram_demand <= usage[j].ram_cap + 1e-9) &&
                        (usage[j].bw_used  + svc.bw_demand  <= usage[j].bw_cap  + 1e-9);

            if (!fits) continue;

            if (config.ffd.consider_sla) {
                // SLA-aware: compute additional communication cost
                double comm_score = 0;
                for (const auto& [neighbor, traffic] : ds.adjacency[i]) {
                    if (placement[neighbor] >= 0) {
                        double lat = ds.latency_between(j, placement[neighbor]);
                        lat += ds.nodes[j].processing_delay;
                        comm_score += traffic * lat;

                        // Bonus penalty if SLA would be violated
                        double total_lat = lat + ds.nodes[placement[neighbor]].processing_delay;
                        if (total_lat > ds.sla_max_latency)
                            comm_score += 1000.0;
                    }
                }

                if (comm_score < best_score) {
                    best_score = comm_score;
                    best_node = j;
                }
            } else {
                // Pure FFD: take the first feasible node
                best_node = j;
                break;
            }
        }

        // Fallback: if no feasible node, pick the least overloaded
        if (best_node < 0) {
            double min_load = 1e18;
            for (int j = 0; j < N; ++j) {
                double load = (usage[j].cpu_used / std::max(usage[j].cpu_cap, 1.0)) +
                              (usage[j].ram_used / std::max(usage[j].ram_cap, 1.0)) +
                              (usage[j].bw_used  / std::max(usage[j].bw_cap,  1.0));
                if (load < min_load) {
                    min_load = load;
                    best_node = j;
                }
            }
        }

        // Assign
        placement[i] = best_node;
        if (best_node >= 0) {
            usage[best_node].cpu_used += svc.cpu_demand;
            usage[best_node].ram_used += svc.ram_demand;
            usage[best_node].bw_used  += svc.bw_demand;
        }

        // Progress callback (treat each service placement as a "generation")
        if (callback && (idx % 5 == 0 || idx == S - 1)) {
            double f = Fitness::quick_evaluate(ds, placement, config.weights);
            callback(idx, f, 0, 0, 0);
        }
    }

    // ── Step 3: Evaluate final placement ─────────────────
    auto fitness = Fitness::evaluate(ds, placement, config.weights);

    Solution sol;
    sol.placement      = placement;
    sol.fitness         = fitness;
    sol.status          = fitness.is_feasible() ? SolutionStatus::FEASIBLE : SolutionStatus::INFEASIBLE;
    sol.algorithm       = AlgorithmType::FFD;
    sol.runtime_ms      = timer.elapsed_ms();
    sol.generations_run = S;  // one "step" per service
    sol.seed_used       = config.seed;
    sol.convergence     = {fitness.total_fitness};

    return sol;
}

}  // namespace qedgeopt
