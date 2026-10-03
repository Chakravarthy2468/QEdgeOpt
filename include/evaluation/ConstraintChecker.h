#pragma once
/// @file ConstraintChecker.h
/// @brief Checks capacity, SLA, and deadline constraints for a placement.

#include "models/Dataset.h"
#include "models/Types.h"

#include <vector>
#include <string>

namespace qedgeopt {

/// Per-node resource usage snapshot.
struct NodeResourceUsage {
    double cpu_used = 0;
    double ram_used = 0;
    double bw_used  = 0;
    double cpu_cap  = 0;
    double ram_cap  = 0;
    double bw_cap   = 0;

    bool cpu_ok() const { return cpu_used <= cpu_cap + 1e-9; }
    bool ram_ok() const { return ram_used <= ram_cap + 1e-9; }
    bool bw_ok()  const { return bw_used  <= bw_cap  + 1e-9; }
    bool all_ok() const { return cpu_ok() && ram_ok() && bw_ok(); }

    double cpu_excess() const { return std::max(0.0, cpu_used - cpu_cap); }
    double ram_excess() const { return std::max(0.0, ram_used - ram_cap); }
    double bw_excess()  const { return std::max(0.0, bw_used  - bw_cap); }
};

/// Result of constraint validation.
struct ConstraintReport {
    bool all_feasible = true;

    int  capacity_violations  = 0;
    int  sla_violations       = 0;
    int  deadline_violations  = 0;

    double total_cpu_excess   = 0;
    double total_ram_excess   = 0;
    double total_bw_excess    = 0;
    double total_latency_excess = 0;
    double total_deadline_excess = 0;

    std::vector<NodeResourceUsage> node_usage;

    /// Per-link latency
    struct LinkLatency {
        int source_svc;
        int target_svc;
        double latency;
        bool   sla_violated;
    };
    std::vector<LinkLatency> link_latencies;
};

class ConstraintChecker {
public:
    /// Compute full constraint report for a given placement.
    static ConstraintReport check(const Dataset& ds, const Placement& placement);

    /// Compute resource usage per node.
    static std::vector<NodeResourceUsage> compute_node_usage(
        const Dataset& ds, const Placement& placement);

    /// Check whether a specific service can fit on a specific node
    /// given current usage.
    static bool can_fit(const Dataset& ds,
                        const std::vector<NodeResourceUsage>& usage,
                        int service_idx, int node_idx);

    /// Compute end-to-end latency for a communication link.
    static double link_latency(const Dataset& ds,
                               const Placement& placement,
                               int src_svc, int tgt_svc);
};

}  // namespace qedgeopt
