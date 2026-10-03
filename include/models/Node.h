#pragma once
/// @file Node.h
/// @brief Infrastructure node (Edge / Fog / Cloudlet) model.

#include "models/Types.h"
#include <string>

namespace qedgeopt {

/// An infrastructure node onto which microservices can be placed.
struct Node {
    std::string id;                 ///< Unique identifier (e.g. "E1", "F2")
    NodeType    type = NodeType::EDGE;
    double      cpu_capacity   = 0; ///< Total CPU cores / units
    double      ram_capacity   = 0; ///< Total RAM in GB
    double      bw_capacity    = 0; ///< Total bandwidth in Mbps
    double      processing_delay = 0; ///< Inherent processing delay in ms

    /// Compact label for display.
    std::string label() const {
        return std::string(to_string(type)) + " " + id;
    }
};

}  // namespace qedgeopt
