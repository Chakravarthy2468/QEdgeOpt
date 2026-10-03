#pragma once
/// @file Service.h
/// @brief Microservice model with resource demands and optional deadline.

#include <string>

namespace qedgeopt {

/// A microservice to be placed onto an infrastructure node.
struct Service {
    std::string id;               ///< Unique identifier (e.g. "S1")
    double      cpu_demand  = 0;  ///< CPU units required
    double      ram_demand  = 0;  ///< RAM in GB required
    double      bw_demand   = 0;  ///< Bandwidth in Mbps required
    double      deadline    = -1; ///< Deadline in ms; <= 0 means no deadline

    /// True if this service has an active deadline constraint.
    bool has_deadline() const { return deadline > 0; }

    /// Combined demand metric for sorting (e.g., FFD).
    /// Normalisation can be applied externally; this is a raw sum.
    double aggregate_demand() const {
        return cpu_demand + ram_demand + bw_demand;
    }
};

}  // namespace qedgeopt
