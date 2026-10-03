#pragma once
/// @file Fitness.h
/// @brief Composite fitness function with configurable weights and full breakdown.

#include "models/Dataset.h"
#include "models/FitnessResult.h"
#include "models/AlgorithmConfig.h"
#include "models/Types.h"

namespace qedgeopt {

class Fitness {
public:
    /// Evaluate a placement and return a detailed fitness breakdown.
    /// Lower fitness = better solution.
    static FitnessResult evaluate(const Dataset& ds,
                                  const Placement& placement,
                                  const FitnessWeights& weights);

    /// Quick scalar fitness (no explanations populated).
    static double quick_evaluate(const Dataset& ds,
                                 const Placement& placement,
                                 const FitnessWeights& weights);
};

}  // namespace qedgeopt
