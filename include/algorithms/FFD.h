#pragma once
/// @file FFD.h
/// @brief First-Fit Decreasing greedy baseline for microservice placement.
///
/// Sorts microservices by aggregate demand (decreasing), then assigns each
/// to the first node with sufficient remaining capacity. An optional SLA-aware
/// variant considers communication cost when choosing among feasible nodes.

#include "algorithms/IAlgorithm.h"

namespace qedgeopt {

class FFD : public IAlgorithm {
public:
    std::string   name() const override { return "FFD (Greedy)"; }
    AlgorithmType type() const override { return AlgorithmType::FFD; }

    Solution solve(const Dataset& ds,
                   const AlgorithmConfig& config,
                   ProgressCallback callback = nullptr) override;
};

}  // namespace qedgeopt
