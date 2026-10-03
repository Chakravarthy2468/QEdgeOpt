#pragma once
/// @file Link.h
/// @brief Communication dependency link between two microservices.

#include <string>

namespace qedgeopt {

/// A directed communication edge: source → target with traffic volume.
struct Link {
    std::string source;        ///< Source microservice ID
    std::string target;        ///< Target microservice ID
    double      traffic = 0;   ///< Traffic volume in Mbps

    // Internal indices (populated after dataset loading)
    int source_idx = -1;
    int target_idx = -1;
};

}  // namespace qedgeopt
