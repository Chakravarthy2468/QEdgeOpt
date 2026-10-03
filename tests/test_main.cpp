#define CATCH_CONFIG_MAIN
// Simple test macro setup, since we didn't fetch Catch2 explicitly in CMake,
// I'll write a minimal bespoke test runner that returns 0 on success.

#include <iostream>
#include <cassert>
#include <cmath>
#include "models/Types.h"
#include "models/Dataset.h"
#include "evaluation/ConstraintChecker.h"
#include "algorithms/QIEA.h"
#include "algorithms/GeneticAlgorithm.h"
#include "algorithms/FFD.h"
#include "io/DatasetGenerator.h"

using namespace qedgeopt;

void test_dataset_generation() {
    std::cout << "Running test_dataset_generation...\n";
    auto ds = DatasetGenerator::small_benchmark(42);
    assert(ds.num_services() == 10);
    assert(ds.num_nodes() == 4);
    assert(ds.validate() == "");
    std::cout << "  Passed.\n";
}

void test_qbit_math() {
    std::cout << "Running test_qbit_math...\n";
    QBit q;
    assert(std::abs(q.prob_one() - 0.5) < 1e-6);
    assert(std::abs(q.prob_zero() - 0.5) < 1e-6);
    
    q.set_theta(M_PI / 2); // alpha = 0, beta = 1
    assert(std::abs(q.prob_one() - 1.0) < 1e-6);
    assert(std::abs(q.prob_zero() - 0.0) < 1e-6);
    std::cout << "  Passed.\n";
}

void test_algorithms_run() {
    std::cout << "Running test_algorithms_run...\n";
    auto ds = DatasetGenerator::small_benchmark(42);
    AlgorithmConfig cfg;
    cfg.seed = 42;
    cfg.qiea.max_generations = 10;
    cfg.qiea.population_size = 10;
    
    QIEA qiea;
    auto sol_q = qiea.solve(ds, cfg);
    assert(!sol_q.empty());
    
    GeneticAlgorithm ga;
    cfg.ga.max_generations = 10;
    cfg.ga.population_size = 10;
    auto sol_ga = ga.solve(ds, cfg);
    assert(!sol_ga.empty());
    
    FFD ffd;
    auto sol_ffd = ffd.solve(ds, cfg);
    assert(!sol_ffd.empty());
    
    std::cout << "  Passed.\n";
}

int main() {
    std::cout << "Starting QEdgeOpt Unit Tests...\n";
    
    test_dataset_generation();
    test_qbit_math();
    test_algorithms_run();
    
    std::cout << "All tests passed successfully.\n";
    return 0;
}
