#pragma once
/// @file Solution.h
/// @brief Placement solution with fitness, convergence history, and metadata.

#include "models/Types.h"
#include "models/FitnessResult.h"

#include <vector>
#include <string>

namespace qedgeopt {

/// A complete placement solution produced by an algorithm.
struct Solution {
    // ── Placement ────────────────────────────────────────
    /// placement[i] = node index for microservice i.
    Placement placement;

    // ── Evaluation ───────────────────────────────────────
    FitnessResult  fitness;
    SolutionStatus status = SolutionStatus::PARTIAL;

    // ── Metadata ─────────────────────────────────────────
    AlgorithmType algorithm   = AlgorithmType::QIEA;
    double        runtime_ms  = 0;
    int           generations_run = 0;
    uint32_t      seed_used   = 42;

    // ── Convergence curve ────────────────────────────────
    /// convergence[g] = best fitness at generation g.
    std::vector<double> convergence;

    // ── Helpers ──────────────────────────────────────────
    bool is_feasible() const { return fitness.is_feasible(); }
    bool empty()       const { return placement.empty(); }
};

/// Result of a benchmark run: one solution per seed.
struct BenchmarkResult {
    AlgorithmType algorithm;
    std::vector<Solution> runs;       // one per seed

    // Aggregated statistics (computed after all runs)
    double best_fitness      = 0;
    double avg_fitness       = 0;
    double std_fitness       = 0;
    double avg_runtime_ms    = 0;
    double feasible_rate     = 0;     // fraction in [0,1]
    double avg_cpu_util      = 0;
    double avg_ram_util      = 0;
    double avg_bw_util       = 0;
    double avg_comm_cost     = 0;
    double avg_latency       = 0;
    double avg_max_latency   = 0;
    double avg_active_nodes  = 0;
    double avg_sla_violations = 0;
    double avg_deadline_violations = 0;
    int    convergence_gen   = 0;     // generation where best was first found
};

}  // namespace qedgeopt
