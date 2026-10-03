#pragma once
/// @file Types.h
/// @brief Core type definitions, enumerations, and constants for QEdgeOpt.

#include <string>
#include <vector>
#include <cstdint>
#include <limits>

namespace qedgeopt {

// ── Node classification ──────────────────────────────────
enum class NodeType : uint8_t {
    EDGE     = 0,
    FOG      = 1,
    CLOUDLET = 2
};

inline const char* to_string(NodeType t) {
    switch (t) {
        case NodeType::EDGE:     return "EDGE";
        case NodeType::FOG:      return "FOG";
        case NodeType::CLOUDLET: return "CLOUDLET";
    }
    return "UNKNOWN";
}

inline NodeType node_type_from_string(const std::string& s) {
    if (s == "EDGE" || s == "edge")         return NodeType::EDGE;
    if (s == "FOG"  || s == "fog")          return NodeType::FOG;
    if (s == "CLOUDLET" || s == "cloudlet") return NodeType::CLOUDLET;
    return NodeType::EDGE;  // default
}

// ── Algorithm selector ───────────────────────────────────
enum class AlgorithmType : uint8_t {
    QIEA = 0,
    GA   = 1,
    FFD  = 2
};

inline const char* to_string(AlgorithmType a) {
    switch (a) {
        case AlgorithmType::QIEA: return "QIEA";
        case AlgorithmType::GA:   return "GA";
        case AlgorithmType::FFD:  return "FFD";
    }
    return "UNKNOWN";
}

// ── Solution feasibility ─────────────────────────────────
enum class SolutionStatus : uint8_t {
    FEASIBLE   = 0,
    INFEASIBLE = 1,
    PARTIAL    = 2
};

inline const char* to_string(SolutionStatus s) {
    switch (s) {
        case SolutionStatus::FEASIBLE:   return "FEASIBLE";
        case SolutionStatus::INFEASIBLE: return "INFEASIBLE";
        case SolutionStatus::PARTIAL:    return "PARTIAL";
    }
    return "UNKNOWN";
}

// ── Global constants ─────────────────────────────────────
constexpr double INFINITY_PENALTY   = 1e12;
constexpr double DEFAULT_SLA_LIMIT  = 100.0;    // ms
constexpr double SAME_NODE_LATENCY  = 0.1;      // ms, near-zero for co-located services
constexpr int    DEFAULT_SEED       = 42;

// ── Placement = vector of node indices ───────────────────
/// placement[i] = node index for microservice i
using Placement = std::vector<int>;

}  // namespace qedgeopt
