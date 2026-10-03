/// @file Metrics.cpp
/// @brief Benchmark result aggregation across multiple seeds.

#include "evaluation/Metrics.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace qedgeopt {

BenchmarkResult Metrics::aggregate(AlgorithmType algo,
                                   const std::vector<Solution>& runs)
{
    BenchmarkResult br;
    br.algorithm = algo;
    br.runs = runs;

    if (runs.empty()) return br;

    int n = static_cast<int>(runs.size());

    // Collect per-run stats
    std::vector<double> fitnesses;
    double sum_runtime = 0;
    int feasible_count = 0;
    double sum_cpu = 0, sum_ram = 0, sum_bw = 0;
    double sum_comm = 0, sum_lat = 0, sum_maxlat = 0;
    double sum_active = 0;
    double sum_sla_v = 0, sum_ddl_v = 0;
    int    best_gen = 0;

    for (const auto& sol : runs) {
        fitnesses.push_back(sol.fitness.total_fitness);
        sum_runtime += sol.runtime_ms;
        if (sol.is_feasible()) feasible_count++;
        sum_cpu    += sol.fitness.avg_cpu_utilization;
        sum_ram    += sol.fitness.avg_ram_utilization;
        sum_bw     += sol.fitness.avg_bw_utilization;
        sum_comm   += sol.fitness.communication_cost;
        sum_lat    += sol.fitness.avg_latency;
        sum_maxlat += sol.fitness.max_latency;
        sum_active += sol.fitness.active_nodes;
        sum_sla_v  += sol.fitness.sla_violations;
        sum_ddl_v  += sol.fitness.deadline_violations;

        // Find generation where best fitness was achieved
        if (!sol.convergence.empty()) {
            double best = sol.convergence.back();
            for (int g = 0; g < (int)sol.convergence.size(); ++g) {
                if (std::abs(sol.convergence[g] - best) < 1e-9) {
                    best_gen = std::max(best_gen, g);
                    break;
                }
            }
        }
    }

    // Best, average, stddev of fitness
    br.best_fitness = *std::min_element(fitnesses.begin(), fitnesses.end());
    br.avg_fitness  = std::accumulate(fitnesses.begin(), fitnesses.end(), 0.0) / n;

    double var = 0;
    for (double f : fitnesses) var += (f - br.avg_fitness) * (f - br.avg_fitness);
    br.std_fitness = std::sqrt(var / n);

    br.avg_runtime_ms = sum_runtime / n;
    br.feasible_rate  = static_cast<double>(feasible_count) / n;
    br.avg_cpu_util   = sum_cpu / n;
    br.avg_ram_util   = sum_ram / n;
    br.avg_bw_util    = sum_bw / n;
    br.avg_comm_cost  = sum_comm / n;
    br.avg_latency    = sum_lat / n;
    br.avg_max_latency = sum_maxlat / n;
    br.avg_active_nodes = sum_active / n;
    br.avg_sla_violations = sum_sla_v / n;
    br.avg_deadline_violations = sum_ddl_v / n;
    br.convergence_gen = best_gen;

    return br;
}

}  // namespace qedgeopt
