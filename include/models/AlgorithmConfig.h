#pragma once
/// @file AlgorithmConfig.h
/// @brief Configuration parameters for all three optimisation algorithms.

#include "models/Types.h"
#include <vector>

namespace qedgeopt {

/// Fitness function weight configuration.
struct FitnessWeights {
    double w_communication = 1.0;   ///< Weight for communication cost
    double w_latency       = 1.0;   ///< Weight for latency cost
    double w_fragmentation = 0.5;   ///< Weight for resource fragmentation
    double w_active_nodes  = 0.3;   ///< Weight for active node count

    double penalty_capacity = 1000.0;  ///< Penalty multiplier per unit capacity violation
    double penalty_latency  = 500.0;   ///< Penalty multiplier per ms SLA violation
    double penalty_deadline = 500.0;   ///< Penalty multiplier per ms deadline violation
};

/// QIEA-specific parameters.
struct QIEAConfig {
    int    population_size   = 30;
    int    max_generations   = 200;
    double rotation_angle    = 0.05;   ///< Δθ for Q-bit rotation (radians)
    double mutation_prob     = 0.02;   ///< Probability of random Q-bit perturbation
    int    elite_count       = 2;
    bool   enable_repair     = true;
    bool   enable_diversity  = true;
    double diversity_threshold = 0.1;  ///< Re-initialise when diversity drops below this
};

/// Classical GA parameters.
struct GAConfig {
    int    population_size   = 50;
    int    max_generations   = 200;
    double crossover_prob    = 0.85;
    double mutation_prob     = 0.05;
    int    elite_count       = 2;
    int    tournament_size   = 3;
    bool   enable_repair     = true;
};

/// FFD-specific parameters (mostly sorting strategy).
struct FFDConfig {
    bool consider_sla = true;  ///< Whether to factor in SLA when choosing nodes
};

/// Benchmark experiment configuration.
struct BenchmarkConfig {
    std::vector<int> seeds       = {42, 100, 200, 300, 400};
    bool run_qiea                = true;
    bool run_ga                  = true;
    bool run_ffd                 = true;
};

/// Aggregated configuration for a run.
struct AlgorithmConfig {
    AlgorithmType  algorithm = AlgorithmType::QIEA;
    uint32_t       seed      = DEFAULT_SEED;
    FitnessWeights weights;
    QIEAConfig     qiea;
    GAConfig       ga;
    FFDConfig      ffd;
    BenchmarkConfig benchmark;
};

}  // namespace qedgeopt
