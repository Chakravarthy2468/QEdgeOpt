#pragma once
/// @file DatasetGenerator.h
/// @brief Synthetic dataset generator with configurable parameters.

#include "models/Dataset.h"
#include <cstdint>

namespace qedgeopt {

/// Parameters for synthetic dataset generation.
struct GeneratorParams {
    int      num_services         = 10;
    int      num_nodes            = 4;
    double   communication_density = 0.3;  ///< Fraction of possible service pairs that communicate
    double   resource_variability  = 0.5;  ///< 0=uniform, 1=highly variable
    double   sla_tightness         = 0.5;  ///< 0=relaxed, 1=very tight
    double   latency_range_min     = 2.0;  ///< Minimum inter-node latency (ms)
    double   latency_range_max     = 30.0; ///< Maximum inter-node latency (ms)
    uint32_t seed                  = 42;

    // Node type distribution (fractions, will be normalised)
    double edge_fraction     = 0.5;
    double fog_fraction      = 0.3;
    double cloudlet_fraction = 0.2;
};

class DatasetGenerator {
public:
    /// Generate a synthetic dataset.
    static Dataset generate(const GeneratorParams& params);

    /// Predefined small benchmark (10 services, 4 nodes).
    static Dataset small_benchmark(uint32_t seed = 42);

    /// Predefined medium benchmark (30 services, 10 nodes).
    static Dataset medium_benchmark(uint32_t seed = 42);

    /// Predefined large benchmark (100 services, 25 nodes).
    static Dataset large_benchmark(uint32_t seed = 42);

    /// Predefined very large benchmark (250 services, 50 nodes).
    static Dataset very_large_benchmark(uint32_t seed = 42);
};

}  // namespace qedgeopt
