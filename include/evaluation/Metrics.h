#pragma once
/// @file Metrics.h
/// @brief Aggregation of benchmark statistics across multiple runs.

#include "models/Solution.h"
#include <vector>

namespace qedgeopt {

class Metrics {
public:
    /// Aggregate statistics from multiple solutions (one per seed).
    static BenchmarkResult aggregate(AlgorithmType algo,
                                     const std::vector<Solution>& runs);
};

}  // namespace qedgeopt
