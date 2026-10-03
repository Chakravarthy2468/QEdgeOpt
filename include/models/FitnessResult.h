#pragma once
/// @file FitnessResult.h
/// @brief Detailed fitness breakdown for explainability.

#include <vector>
#include <string>

namespace qedgeopt {

/// Complete breakdown of a solution's fitness evaluation.
/// Every component is stored separately so the UI can explain
/// exactly why a solution received its score.
struct FitnessResult {
    // ── Composite score (lower is better) ────────────────
    double total_fitness         = 0;

    // ── Objective components ─────────────────────────────
    double communication_cost    = 0;   ///< Σ traffic × latency for cross-node edges
    double latency_cost          = 0;   ///< Weighted sum of end-to-end latencies
    double fragmentation_cost    = 0;   ///< Resource fragmentation across nodes
    double active_node_cost      = 0;   ///< Number of active nodes (fewer = cheaper)

    // ── Penalty components ───────────────────────────────
    double penalty_capacity      = 0;   ///< Penalty for capacity constraint violations
    double penalty_latency       = 0;   ///< Penalty for SLA latency violations
    double penalty_deadline      = 0;   ///< Penalty for deadline violations

    // ── Statistics ───────────────────────────────────────
    int    sla_violations        = 0;   ///< Count of SLA violations
    int    deadline_violations   = 0;   ///< Count of deadline violations
    int    capacity_violations   = 0;   ///< Count of overloaded nodes
    int    active_nodes          = 0;   ///< Number of nodes hosting at least one service

    double avg_cpu_utilization   = 0;   ///< Average CPU util across active nodes [0,1]
    double avg_ram_utilization   = 0;   ///< Average RAM util across active nodes [0,1]
    double avg_bw_utilization    = 0;   ///< Average BW util across active nodes [0,1]
    double avg_latency           = 0;   ///< Average communication latency
    double max_latency           = 0;   ///< Worst-case communication latency

    int    cross_node_edges      = 0;   ///< Edges crossing node boundaries
    int    intra_node_edges      = 0;   ///< Edges within the same node
    double total_cross_traffic   = 0;   ///< Traffic volume crossing nodes

    // ── Feasibility ──────────────────────────────────────
    bool is_feasible() const {
        return capacity_violations == 0
            && sla_violations == 0
            && deadline_violations == 0;
    }

    // ── Per-service explanations ─────────────────────────
    struct ServiceExplanation {
        std::string service_id;
        std::string node_id;
        bool cpu_feasible     = true;
        bool ram_feasible     = true;
        bool bw_feasible      = true;
        bool sla_met          = true;
        bool deadline_met     = true;
        double added_comm_cost = 0;
        std::string reason;
    };
    std::vector<ServiceExplanation> explanations;

    // ── Per-node resource usage ──────────────────────────
    struct NodeUsage {
        std::string node_id;
        double cpu_used    = 0;
        double cpu_cap     = 0;
        double ram_used    = 0;
        double ram_cap     = 0;
        double bw_used     = 0;
        double bw_cap      = 0;
        int    services_hosted = 0;
    };
    std::vector<NodeUsage> node_usages;
};

}  // namespace qedgeopt
