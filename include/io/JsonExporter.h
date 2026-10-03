#pragma once
/// @file JsonExporter.h
/// @brief Export solutions, fitness results, and benchmarks to JSON.

#include "models/Dataset.h"
#include "models/Solution.h"
#include <string>

namespace qedgeopt {

class JsonExporter {
public:
    /// Export a Solution to a JSON string.
    static std::string solution_to_json(const Solution& sol, const Dataset& ds);

    /// Export a BenchmarkResult to a JSON string.
    static std::string benchmark_to_json(const BenchmarkResult& br, const Dataset& ds);

    /// Export multiple BenchmarkResults (comparison) to JSON.
    static std::string comparison_to_json(
        const std::vector<BenchmarkResult>& results, const Dataset& ds);

    /// Export the dataset itself to JSON.
    static std::string dataset_to_json(const Dataset& ds);
};

}  // namespace qedgeopt
