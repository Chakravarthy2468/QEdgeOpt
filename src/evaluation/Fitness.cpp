/// @file Fitness.cpp
/// @brief Composite fitness function implementation with full breakdown.
///
/// Fitness = w1·communication_cost + w2·latency_cost + w3·fragmentation_cost
///         + w4·active_node_cost + penalty_capacity + penalty_latency + penalty_deadline
///
/// Lower fitness = better solution.
/// Hard SLA and capacity violations receive very large penalties.

#include "evaluation/Fitness.h"
#include "evaluation/ConstraintChecker.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace qedgeopt {

FitnessResult Fitness::evaluate(const Dataset& ds,
                                const Placement& placement,
                                const FitnessWeights& weights)
{
    FitnessResult result;
    int S = ds.num_services();
    int N = ds.num_nodes();

    if ((int)placement.size() != S) {
        result.total_fitness = INFINITY_PENALTY;
        return result;
    }

    // ── Compute resource usage per node ──────────────────
    auto node_usage = ConstraintChecker::compute_node_usage(ds, placement);

    // Track active nodes
    std::set<int> active_set;
    for (int i = 0; i < S; ++i) {
        if (placement[i] >= 0 && placement[i] < N)
            active_set.insert(placement[i]);
    }
    result.active_nodes = static_cast<int>(active_set.size());

    // ── Node usage details ───────────────────────────────
    result.node_usages.resize(N);
    double sum_cpu_util = 0, sum_ram_util = 0, sum_bw_util = 0;

    for (int j = 0; j < N; ++j) {
        auto& nu = result.node_usages[j];
        nu.node_id   = ds.nodes[j].id;
        nu.cpu_used  = node_usage[j].cpu_used;
        nu.cpu_cap   = node_usage[j].cpu_cap;
        nu.ram_used  = node_usage[j].ram_used;
        nu.ram_cap   = node_usage[j].ram_cap;
        nu.bw_used   = node_usage[j].bw_used;
        nu.bw_cap    = node_usage[j].bw_cap;

        // Count services on this node
        for (int i = 0; i < S; ++i)
            if (placement[i] == j) nu.services_hosted++;

        // Capacity violations
        if (!node_usage[j].all_ok()) {
            result.capacity_violations++;
            result.penalty_capacity +=
                weights.penalty_capacity * (
                    node_usage[j].cpu_excess() +
                    node_usage[j].ram_excess() +
                    node_usage[j].bw_excess()
                );
        }

        // Utilisation (for active nodes only)
        if (active_set.count(j)) {
            double cpu_u = (nu.cpu_cap > 0) ? nu.cpu_used / nu.cpu_cap : 0;
            double ram_u = (nu.ram_cap > 0) ? nu.ram_used / nu.ram_cap : 0;
            double bw_u  = (nu.bw_cap > 0)  ? nu.bw_used  / nu.bw_cap  : 0;
            sum_cpu_util += cpu_u;
            sum_ram_util += ram_u;
            sum_bw_util  += bw_u;
        }
    }

    if (result.active_nodes > 0) {
        result.avg_cpu_utilization = sum_cpu_util / result.active_nodes;
        result.avg_ram_utilization = sum_ram_util / result.active_nodes;
        result.avg_bw_utilization  = sum_bw_util  / result.active_nodes;
    }

    // ── Communication cost & latency ─────────────────────
    double total_comm_cost = 0;
    double total_latency   = 0;
    double max_lat         = 0;
    int    link_count      = 0;

    for (const auto& lnk : ds.links) {
        if (lnk.source_idx < 0 || lnk.target_idx < 0) continue;

        int n1 = placement[lnk.source_idx];
        int n2 = placement[lnk.target_idx];

        double lat = ConstraintChecker::link_latency(ds, placement,
                                                      lnk.source_idx, lnk.target_idx);
        double comm = lnk.traffic * lat;

        total_comm_cost += comm;
        total_latency   += lat;
        max_lat = std::max(max_lat, lat);
        link_count++;

        if (n1 != n2) {
            result.cross_node_edges++;
            result.total_cross_traffic += lnk.traffic;
        } else {
            result.intra_node_edges++;
        }

        // SLA violation check
        if (lat > ds.sla_max_latency) {
            result.sla_violations++;
            result.penalty_latency +=
                weights.penalty_latency * (lat - ds.sla_max_latency);
        }
    }

    result.communication_cost = total_comm_cost;
    result.avg_latency = (link_count > 0) ? total_latency / link_count : 0;
    result.max_latency = max_lat;

    // ── Deadline violations ──────────────────────────────
    for (int i = 0; i < S; ++i) {
        if (!ds.services[i].has_deadline()) continue;
        int j = placement[i];
        if (j < 0 || j >= N) continue;

        double completion = ds.nodes[j].processing_delay;
        for (const auto& [neighbor, traffic] : ds.adjacency[i]) {
            completion += ConstraintChecker::link_latency(ds, placement, i, neighbor);
        }

        if (completion > ds.services[i].deadline) {
            result.deadline_violations++;
            result.penalty_deadline +=
                weights.penalty_deadline * (completion - ds.services[i].deadline);
        }
    }

    // ── Fragmentation cost ───────────────────────────────
    // Measure variance in utilisation across active nodes (lower variance = less fragmentation)
    if (result.active_nodes > 1) {
        double frag = 0;
        for (int j : active_set) {
            double cpu_u = (node_usage[j].cpu_cap > 0) ?
                node_usage[j].cpu_used / node_usage[j].cpu_cap : 0;
            double ram_u = (node_usage[j].ram_cap > 0) ?
                node_usage[j].ram_used / node_usage[j].ram_cap : 0;
            double bw_u  = (node_usage[j].bw_cap > 0) ?
                node_usage[j].bw_used  / node_usage[j].bw_cap  : 0;

            // Deviation from average utilisation
            frag += std::abs(cpu_u - result.avg_cpu_utilization);
            frag += std::abs(ram_u - result.avg_ram_utilization);
            frag += std::abs(bw_u  - result.avg_bw_utilization);
        }
        result.fragmentation_cost = frag / result.active_nodes;
    }

    // ── Active node cost ─────────────────────────────────
    result.active_node_cost = static_cast<double>(result.active_nodes);

    // ── Composite fitness ────────────────────────────────
    result.latency_cost = result.avg_latency;

    result.total_fitness =
        weights.w_communication * result.communication_cost +
        weights.w_latency       * result.latency_cost +
        weights.w_fragmentation * result.fragmentation_cost +
        weights.w_active_nodes  * result.active_node_cost +
        result.penalty_capacity +
        result.penalty_latency +
        result.penalty_deadline;

    // ── Per-service explanations ─────────────────────────
    result.explanations.resize(S);
    for (int i = 0; i < S; ++i) {
        auto& ex = result.explanations[i];
        ex.service_id = ds.services[i].id;
        int j = placement[i];
        ex.node_id = (j >= 0 && j < N) ? ds.nodes[j].id : "UNASSIGNED";

        if (j >= 0 && j < N) {
            ex.cpu_feasible = (node_usage[j].cpu_used <= node_usage[j].cpu_cap + 1e-9);
            ex.ram_feasible = (node_usage[j].ram_used <= node_usage[j].ram_cap + 1e-9);
            ex.bw_feasible  = (node_usage[j].bw_used  <= node_usage[j].bw_cap  + 1e-9);

            // Communication cost added by this service
            for (const auto& [neighbor, traffic] : ds.adjacency[i]) {
                double lat = ConstraintChecker::link_latency(ds, placement, i, neighbor);
                ex.added_comm_cost += traffic * lat;
                if (lat > ds.sla_max_latency) ex.sla_met = false;
            }

            // Deadline check
            if (ds.services[i].has_deadline()) {
                double completion = ds.nodes[j].processing_delay;
                for (const auto& [neighbor, traffic] : ds.adjacency[i]) {
                    completion += ConstraintChecker::link_latency(ds, placement, i, neighbor);
                }
                ex.deadline_met = (completion <= ds.services[i].deadline);
            }

            // Build reason string
            std::string reason;
            reason += ex.cpu_feasible ? "CPU feasible. " : "CPU EXCEEDED. ";
            reason += ex.ram_feasible ? "RAM feasible. " : "RAM EXCEEDED. ";
            reason += ex.bw_feasible  ? "BW feasible. "  : "BW EXCEEDED. ";
            reason += ex.sla_met      ? "SLA met. "      : "SLA VIOLATED. ";
            reason += ex.deadline_met ? "Deadline met."   : "DEADLINE MISSED.";
            ex.reason = reason;
        } else {
            ex.reason = "UNASSIGNED — no valid node found.";
        }
    }

    return result;
}

double Fitness::quick_evaluate(const Dataset& ds,
                               const Placement& placement,
                               const FitnessWeights& weights)
{
    int S = ds.num_services();
    int N = ds.num_nodes();

    if ((int)placement.size() != S) return INFINITY_PENALTY;

    // Lightweight version without explanations
    auto node_usage = ConstraintChecker::compute_node_usage(ds, placement);

    double penalty_cap = 0;
    int active_count = 0;
    std::set<int> active_set;
    for (int i = 0; i < S; ++i)
        if (placement[i] >= 0 && placement[i] < N)
            active_set.insert(placement[i]);
    active_count = static_cast<int>(active_set.size());

    for (int j = 0; j < N; ++j) {
        if (!node_usage[j].all_ok()) {
            penalty_cap += weights.penalty_capacity * (
                node_usage[j].cpu_excess() +
                node_usage[j].ram_excess() +
                node_usage[j].bw_excess()
            );
        }
    }

    double comm_cost = 0, penalty_lat = 0, penalty_ddl = 0;
    double total_latency = 0;
    int link_count = 0;

    for (const auto& lnk : ds.links) {
        if (lnk.source_idx < 0 || lnk.target_idx < 0) continue;
        double lat = ConstraintChecker::link_latency(ds, placement,
                                                      lnk.source_idx, lnk.target_idx);
        comm_cost += lnk.traffic * lat;
        total_latency += lat;
        link_count++;
        if (lat > ds.sla_max_latency)
            penalty_lat += weights.penalty_latency * (lat - ds.sla_max_latency);
    }

    double avg_lat = (link_count > 0) ? total_latency / link_count : 0;

    // Deadline penalties
    for (int i = 0; i < S; ++i) {
        if (!ds.services[i].has_deadline()) continue;
        int j = placement[i];
        if (j < 0 || j >= N) continue;
        double completion = ds.nodes[j].processing_delay;
        for (const auto& [neighbor, traffic] : ds.adjacency[i])
            completion += ConstraintChecker::link_latency(ds, placement, i, neighbor);
        if (completion > ds.services[i].deadline)
            penalty_ddl += weights.penalty_deadline * (completion - ds.services[i].deadline);
    }

    // Fragmentation (simplified)
    double frag = 0;
    if (active_count > 1) {
        double avg_cpu = 0, avg_ram = 0, avg_bw = 0;
        for (int j : active_set) {
            avg_cpu += (node_usage[j].cpu_cap > 0) ? node_usage[j].cpu_used / node_usage[j].cpu_cap : 0;
            avg_ram += (node_usage[j].ram_cap > 0) ? node_usage[j].ram_used / node_usage[j].ram_cap : 0;
            avg_bw  += (node_usage[j].bw_cap > 0)  ? node_usage[j].bw_used  / node_usage[j].bw_cap  : 0;
        }
        avg_cpu /= active_count; avg_ram /= active_count; avg_bw /= active_count;
        for (int j : active_set) {
            double cu = (node_usage[j].cpu_cap > 0) ? node_usage[j].cpu_used / node_usage[j].cpu_cap : 0;
            double ru = (node_usage[j].ram_cap > 0) ? node_usage[j].ram_used / node_usage[j].ram_cap : 0;
            double bu = (node_usage[j].bw_cap > 0)  ? node_usage[j].bw_used  / node_usage[j].bw_cap  : 0;
            frag += std::abs(cu - avg_cpu) + std::abs(ru - avg_ram) + std::abs(bu - avg_bw);
        }
        frag /= active_count;
    }

    return weights.w_communication * comm_cost +
           weights.w_latency       * avg_lat +
           weights.w_fragmentation * frag +
           weights.w_active_nodes  * active_count +
           penalty_cap + penalty_lat + penalty_ddl;
}

}  // namespace qedgeopt
