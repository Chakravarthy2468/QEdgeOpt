#pragma once
/// @file Dataset.h
/// @brief Complete problem instance: nodes, services, links, latency matrix, SLA.

#include "models/Types.h"
#include "models/Node.h"
#include "models/Service.h"
#include "models/Link.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace qedgeopt {

/// The full problem instance that every algorithm receives.
class Dataset {
public:
    // ── Raw data ─────────────────────────────────────────
    std::vector<Node>    nodes;
    std::vector<Service> services;
    std::vector<Link>    links;

    /// node_latency[i][j] = network latency in ms between node i and node j.
    std::vector<std::vector<double>> node_latency;

    double sla_max_latency = DEFAULT_SLA_LIMIT;

    // ── Derived structures (built by build_indices) ──────

    /// Adjacency list: adj[i] = {(j, traffic), ...} for service i.
    std::vector<std::vector<std::pair<int, double>>> adjacency;

    /// Quick lookup: service_id → index, node_id → index.
    std::unordered_map<std::string, int> service_index;
    std::unordered_map<std::string, int> node_index;

    // ── Accessors ────────────────────────────────────────
    int num_services() const { return static_cast<int>(services.size()); }
    int num_nodes()    const { return static_cast<int>(nodes.size()); }
    int num_links()    const { return static_cast<int>(links.size()); }

    /// Latency between two nodes.  Same-node → SAME_NODE_LATENCY.
    double latency_between(int n1, int n2) const {
        if (n1 == n2) return SAME_NODE_LATENCY;
        if (n1 < 0 || n2 < 0) return 0;
        if (n1 >= num_nodes() || n2 >= num_nodes()) return 0;
        return node_latency[n1][n2];
    }

    /// Build index maps and adjacency list after raw data is loaded.
    void build_indices() {
        // Build ID → index maps
        service_index.clear();
        node_index.clear();
        for (int i = 0; i < num_services(); ++i)
            service_index[services[i].id] = i;
        for (int j = 0; j < num_nodes(); ++j)
            node_index[nodes[j].id] = j;

        // Resolve link indices
        for (auto& lnk : links) {
            auto it_s = service_index.find(lnk.source);
            auto it_t = service_index.find(lnk.target);
            if (it_s != service_index.end()) lnk.source_idx = it_s->second;
            if (it_t != service_index.end()) lnk.target_idx = it_t->second;
        }

        // Build adjacency list (undirected for latency analysis)
        adjacency.assign(num_services(), {});
        for (const auto& lnk : links) {
            if (lnk.source_idx >= 0 && lnk.target_idx >= 0) {
                adjacency[lnk.source_idx].emplace_back(lnk.target_idx, lnk.traffic);
                adjacency[lnk.target_idx].emplace_back(lnk.source_idx, lnk.traffic);
            }
        }

        // Generate default latency matrix if missing
        if (node_latency.empty() && num_nodes() > 0) {
            generate_default_latency();
        }
    }

    /// Validate dataset integrity.
    /// @returns Empty string on success, error message on failure.
    std::string validate() const {
        if (services.empty()) return "No services defined";
        if (nodes.empty())    return "No nodes defined";

        // Check total capacity
        double total_cpu_demand = 0, total_ram_demand = 0, total_bw_demand = 0;
        for (const auto& s : services) {
            total_cpu_demand += s.cpu_demand;
            total_ram_demand += s.ram_demand;
            total_bw_demand  += s.bw_demand;
        }
        double total_cpu_cap = 0, total_ram_cap = 0, total_bw_cap = 0;
        for (const auto& n : nodes) {
            total_cpu_cap += n.cpu_capacity;
            total_ram_cap += n.ram_capacity;
            total_bw_cap  += n.bw_capacity;
        }

        if (total_cpu_demand > total_cpu_cap)
            return "Total CPU demand exceeds total capacity";
        if (total_ram_demand > total_ram_cap)
            return "Total RAM demand exceeds total capacity";
        if (total_bw_demand > total_bw_cap)
            return "Total bandwidth demand exceeds total capacity";

        // Check link references
        for (const auto& lnk : links) {
            if (service_index.count(lnk.source) == 0)
                return "Link source '" + lnk.source + "' not found";
            if (service_index.count(lnk.target) == 0)
                return "Link target '" + lnk.target + "' not found";
        }

        return "";  // valid
    }

    /// Compute normalised demand for FFD sorting.
    /// Returns (cpu_norm + ram_norm + bw_norm) for service i.
    double normalised_demand(int i) const {
        double max_cpu = 1, max_ram = 1, max_bw = 1;
        for (const auto& n : nodes) {
            max_cpu = std::max(max_cpu, n.cpu_capacity);
            max_ram = std::max(max_ram, n.ram_capacity);
            max_bw  = std::max(max_bw,  n.bw_capacity);
        }
        const auto& s = services[i];
        return (s.cpu_demand / max_cpu) +
               (s.ram_demand / max_ram) +
               (s.bw_demand  / max_bw);
    }

private:
    void generate_default_latency() {
        int n = num_nodes();
        node_latency.assign(n, std::vector<double>(n, 0));
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                // Heuristic: EDGE↔EDGE ≈ 5ms, FOG↔FOG ≈ 10ms, CLOUDLET ≈ 20ms, mixed ≈ 15ms
                double lat = 10.0;
                if (nodes[i].type == NodeType::EDGE && nodes[j].type == NodeType::EDGE)
                    lat = 5.0;
                else if (nodes[i].type == NodeType::CLOUDLET || nodes[j].type == NodeType::CLOUDLET)
                    lat = 20.0;
                node_latency[i][j] = lat;
                node_latency[j][i] = lat;
            }
        }
    }
};

}  // namespace qedgeopt
