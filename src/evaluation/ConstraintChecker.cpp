/// @file ConstraintChecker.cpp
/// @brief Implementation of constraint checking for capacity, SLA, and deadlines.

#include "evaluation/ConstraintChecker.h"
#include <algorithm>
#include <cmath>

namespace qedgeopt {

std::vector<NodeResourceUsage> ConstraintChecker::compute_node_usage(
    const Dataset& ds, const Placement& placement)
{
    int N = ds.num_nodes();
    std::vector<NodeResourceUsage> usage(N);

    // Initialise capacities
    for (int j = 0; j < N; ++j) {
        usage[j].cpu_cap = ds.nodes[j].cpu_capacity;
        usage[j].ram_cap = ds.nodes[j].ram_capacity;
        usage[j].bw_cap  = ds.nodes[j].bw_capacity;
    }

    // Accumulate service demands
    int S = ds.num_services();
    for (int i = 0; i < S; ++i) {
        int j = placement[i];
        if (j < 0 || j >= N) continue;  // unassigned / invalid
        usage[j].cpu_used += ds.services[i].cpu_demand;
        usage[j].ram_used += ds.services[i].ram_demand;
        usage[j].bw_used  += ds.services[i].bw_demand;
    }

    return usage;
}

bool ConstraintChecker::can_fit(const Dataset& ds,
                                const std::vector<NodeResourceUsage>& usage,
                                int service_idx, int node_idx)
{
    const auto& s = ds.services[service_idx];
    const auto& u = usage[node_idx];
    return (u.cpu_used + s.cpu_demand <= u.cpu_cap + 1e-9) &&
           (u.ram_used + s.ram_demand <= u.ram_cap + 1e-9) &&
           (u.bw_used  + s.bw_demand  <= u.bw_cap  + 1e-9);
}

double ConstraintChecker::link_latency(const Dataset& ds,
                                       const Placement& placement,
                                       int src_svc, int tgt_svc)
{
    int n1 = placement[src_svc];
    int n2 = placement[tgt_svc];
    double network_lat = ds.latency_between(n1, n2);
    // End-to-end = processing delay at source node + network + processing delay at target
    double proc_src = (n1 >= 0 && n1 < ds.num_nodes()) ? ds.nodes[n1].processing_delay : 0;
    double proc_tgt = (n2 >= 0 && n2 < ds.num_nodes()) ? ds.nodes[n2].processing_delay : 0;
    return proc_src + network_lat + proc_tgt;
}

ConstraintReport ConstraintChecker::check(const Dataset& ds, const Placement& placement)
{
    ConstraintReport report;
    report.all_feasible = true;

    int S = ds.num_services();
    int N = ds.num_nodes();

    // ── Capacity constraints ─────────────────────────────
    report.node_usage = compute_node_usage(ds, placement);

    for (int j = 0; j < N; ++j) {
        const auto& u = report.node_usage[j];
        if (!u.all_ok()) {
            report.capacity_violations++;
            report.all_feasible = false;
            report.total_cpu_excess += u.cpu_excess();
            report.total_ram_excess += u.ram_excess();
            report.total_bw_excess  += u.bw_excess();
        }
    }

    // ── SLA & latency constraints ────────────────────────
    for (const auto& lnk : ds.links) {
        if (lnk.source_idx < 0 || lnk.target_idx < 0) continue;
        if (lnk.source_idx >= S || lnk.target_idx >= S) continue;

        double lat = link_latency(ds, placement, lnk.source_idx, lnk.target_idx);
        bool violated = (lat > ds.sla_max_latency);

        report.link_latencies.push_back({
            lnk.source_idx, lnk.target_idx, lat, violated
        });

        if (violated) {
            report.sla_violations++;
            report.all_feasible = false;
            report.total_latency_excess += (lat - ds.sla_max_latency);
        }
    }

    // ── Deadline constraints ─────────────────────────────
    for (int i = 0; i < S; ++i) {
        if (!ds.services[i].has_deadline()) continue;
        int j = placement[i];
        if (j < 0 || j >= N) continue;

        // Completion time = processing delay at the assigned node
        // plus the sum of communication latencies for this service's links
        double completion = ds.nodes[j].processing_delay;
        for (const auto& [neighbor, traffic] : ds.adjacency[i]) {
            completion += link_latency(ds, placement, i, neighbor);
        }

        if (completion > ds.services[i].deadline) {
            report.deadline_violations++;
            report.all_feasible = false;
            report.total_deadline_excess += (completion - ds.services[i].deadline);
        }
    }

    return report;
}

}  // namespace qedgeopt
