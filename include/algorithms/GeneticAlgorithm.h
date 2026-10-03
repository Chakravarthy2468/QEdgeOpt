#pragma once
/// @file GeneticAlgorithm.h
/// @brief Classical Genetic Algorithm baseline for microservice placement.

#include "algorithms/IAlgorithm.h"
#include "utils/Random.h"

#include <vector>

namespace qedgeopt {

/// A GA individual: direct placement encoding + cached fitness.
struct GAIndividual {
    Placement placement;
    double    fitness = 1e18;
};

class GeneticAlgorithm : public IAlgorithm {
public:
    std::string   name() const override { return "Classical GA"; }
    AlgorithmType type() const override { return AlgorithmType::GA; }

    Solution solve(const Dataset& ds,
                   const AlgorithmConfig& config,
                   ProgressCallback callback = nullptr) override;

private:
    // ── GA operators ─────────────────────────────────────
    /// Random feasible-ish initialisation.
    GAIndividual random_individual(const Dataset& ds, Random& rng);

    /// Tournament selection: pick the best of k random individuals.
    int tournament_select(const std::vector<GAIndividual>& pop,
                          int tournament_size, Random& rng);

    /// Uniform crossover: each gene from parent A or B with 50% probability.
    Placement crossover(const Placement& a, const Placement& b, Random& rng);

    /// Point mutation: reassign one random service to a random node.
    Placement mutate(const Placement& p, int num_nodes,
                     double mut_prob, Random& rng);

    /// Repair infeasible placements.
    Placement repair(const Dataset& ds, Placement placement, Random& rng);
};

}  // namespace qedgeopt
