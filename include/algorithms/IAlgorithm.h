#pragma once
/// @file IAlgorithm.h
/// @brief Common interface for all placement algorithms.

#include "models/Dataset.h"
#include "models/Solution.h"
#include "models/AlgorithmConfig.h"

#include <functional>
#include <string>

namespace qedgeopt {

/// Callback invoked after each generation/iteration for live progress reporting.
/// Parameters: generation, best_fitness, feasible_rate, sla_violations, active_nodes.
using ProgressCallback = std::function<void(int, double, double, int, int)>;

/// Abstract base class for placement algorithms.
class IAlgorithm {
public:
    virtual ~IAlgorithm() = default;

    /// Algorithm display name.
    virtual std::string name() const = 0;

    /// Algorithm type enum.
    virtual AlgorithmType type() const = 0;

    /// Run the algorithm and return the best solution found.
    /// @param ds       Problem instance.
    /// @param config   Algorithm configuration.
    /// @param callback Optional progress callback for live UI updates.
    virtual Solution solve(const Dataset& ds,
                           const AlgorithmConfig& config,
                           ProgressCallback callback = nullptr) = 0;
};

}  // namespace qedgeopt
