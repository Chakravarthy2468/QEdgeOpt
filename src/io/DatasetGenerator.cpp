/// @file DatasetGenerator.cpp
/// @brief Synthetic dataset generator with reproducible seeds.

#include "io/DatasetGenerator.h"
#include "utils/Random.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <set>

namespace qedgeopt {

Dataset DatasetGenerator::generate(const GeneratorParams& params)
{
    Random rng(params.seed);
    Dataset ds;

    int S = params.num_services;
    int N = params.num_nodes;

    // ── Generate nodes ───────────────────────────────────
    double total_frac = params.edge_fraction + params.fog_fraction + params.cloudlet_fraction;
    double edge_f = params.edge_fraction / total_frac;
    double fog_f  = params.fog_fraction / total_frac;

    for (int j = 0; j < N; ++j) {
        Node node;
        std::ostringstream oss;

        double r = static_cast<double>(j) / N;
        if (r < edge_f) {
            node.type = NodeType::EDGE;
            oss << "E" << (j + 1);
        } else if (r < edge_f + fog_f) {
            node.type = NodeType::FOG;
            oss << "F" << (j + 1);
        } else {
            node.type = NodeType::CLOUDLET;
            oss << "C" << (j + 1);
        }
        node.id = oss.str();

        // Base capacities depend on node type
        double cpu_base = 0, ram_base = 0, bw_base = 0, delay_base = 0;
        switch (node.type) {
            case NodeType::EDGE:
                cpu_base = 8;   ram_base = 16;  bw_base = 500;   delay_base = 1.0;
                break;
            case NodeType::FOG:
                cpu_base = 16;  ram_base = 32;  bw_base = 1000;  delay_base = 2.0;
                break;
            case NodeType::CLOUDLET:
                cpu_base = 32;  ram_base = 64;  bw_base = 2000;  delay_base = 5.0;
                break;
        }

        // Apply variability
        double var = params.resource_variability;
        node.cpu_capacity = cpu_base * (1.0 + rng.uniform(-var, var));
        node.ram_capacity = ram_base * (1.0 + rng.uniform(-var, var));
        node.bw_capacity  = bw_base  * (1.0 + rng.uniform(-var, var));
        node.processing_delay = delay_base * (1.0 + rng.uniform(0, var));

        // Ensure minimum capacities
        node.cpu_capacity = std::max(node.cpu_capacity, 2.0);
        node.ram_capacity = std::max(node.ram_capacity, 4.0);
        node.bw_capacity  = std::max(node.bw_capacity,  100.0);

        ds.nodes.push_back(node);
    }

    // ── Generate services ────────────────────────────────
    // Ensure total demand fits into total capacity with some slack
    double total_cpu_cap = 0, total_ram_cap = 0, total_bw_cap = 0;
    for (const auto& n : ds.nodes) {
        total_cpu_cap += n.cpu_capacity;
        total_ram_cap += n.ram_capacity;
        total_bw_cap  += n.bw_capacity;
    }

    double avg_cpu = (total_cpu_cap / S) * 0.6;  // 60% target utilisation
    double avg_ram = (total_ram_cap / S) * 0.6;
    double avg_bw  = (total_bw_cap  / S) * 0.6;

    for (int i = 0; i < S; ++i) {
        Service svc;
        svc.id = "S" + std::to_string(i + 1);

        double var = params.resource_variability;
        svc.cpu_demand = avg_cpu * (0.5 + rng.uniform(0, 1.0 + var));
        svc.ram_demand = avg_ram * (0.5 + rng.uniform(0, 1.0 + var));
        svc.bw_demand  = avg_bw  * (0.5 + rng.uniform(0, 1.0 + var));

        // Minimum demands
        svc.cpu_demand = std::max(svc.cpu_demand, 0.5);
        svc.ram_demand = std::max(svc.ram_demand, 0.5);
        svc.bw_demand  = std::max(svc.bw_demand,  10.0);

        // Deadline (only some services have deadlines)
        if (rng.uniform() < 0.4) {
            double base_deadline = 50.0;
            svc.deadline = base_deadline * (2.0 - params.sla_tightness) *
                          (0.8 + rng.uniform(0, 0.4));
        }

        ds.services.push_back(svc);
    }

    // ── Generate communication links ─────────────────────
    int max_links = S * (S - 1) / 2;
    int target_links = static_cast<int>(max_links * params.communication_density);
    target_links = std::max(target_links, std::min(S - 1, 3));  // at least a few links

    // Create a random spanning tree first (ensures connectivity)
    std::vector<int> perm(S);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), rng.engine());

    std::set<std::pair<int,int>> used_pairs;

    for (int i = 1; i < S && (int)ds.links.size() < target_links; ++i) {
        int a = perm[i - 1], b = perm[i];
        if (a > b) std::swap(a, b);
        if (used_pairs.insert({a, b}).second) {
            Link lnk;
            lnk.source  = ds.services[a].id;
            lnk.target  = ds.services[b].id;
            lnk.traffic = rng.uniform(5, 200);
            ds.links.push_back(lnk);
        }
    }

    // Add more random links to reach target density
    int attempts = 0;
    while ((int)ds.links.size() < target_links && attempts < max_links * 2) {
        int a = rng.uniform_int(0, S - 1);
        int b = rng.uniform_int(0, S - 1);
        if (a == b) { attempts++; continue; }
        if (a > b) std::swap(a, b);
        if (used_pairs.insert({a, b}).second) {
            Link lnk;
            lnk.source  = ds.services[a].id;
            lnk.target  = ds.services[b].id;
            lnk.traffic = rng.uniform(5, 200);
            ds.links.push_back(lnk);
        }
        attempts++;
    }

    // ── Generate node-to-node latency matrix ─────────────
    ds.node_latency.assign(N, std::vector<double>(N, 0));
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            double lat = rng.uniform(params.latency_range_min, params.latency_range_max);

            // Same type = lower latency; different type = higher
            if (ds.nodes[i].type == ds.nodes[j].type)
                lat *= 0.6;
            if (ds.nodes[i].type == NodeType::CLOUDLET || ds.nodes[j].type == NodeType::CLOUDLET)
                lat *= 1.5;

            ds.node_latency[i][j] = lat;
            ds.node_latency[j][i] = lat;
        }
    }

    // ── SLA ──────────────────────────────────────────────
    // SLA based on the average latency in the matrix
    double avg_lat = 0;
    int lat_count = 0;
    for (int i = 0; i < N; ++i)
        for (int j = i + 1; j < N; ++j) {
            avg_lat += ds.node_latency[i][j];
            lat_count++;
        }
    if (lat_count > 0) avg_lat /= lat_count;

    // Tighter SLA = stricter constraint
    ds.sla_max_latency = avg_lat * (3.0 - 2.0 * params.sla_tightness);
    ds.sla_max_latency = std::max(ds.sla_max_latency, 5.0);

    // Build indices
    ds.build_indices();

    return ds;
}

// ── Predefined benchmarks ────────────────────────────────

Dataset DatasetGenerator::small_benchmark(uint32_t seed) {
    GeneratorParams p;
    p.num_services = 10;  p.num_nodes = 4;
    p.communication_density = 0.3;
    p.resource_variability = 0.3;
    p.sla_tightness = 0.4;
    p.seed = seed;
    return generate(p);
}

Dataset DatasetGenerator::medium_benchmark(uint32_t seed) {
    GeneratorParams p;
    p.num_services = 30;  p.num_nodes = 10;
    p.communication_density = 0.2;
    p.resource_variability = 0.4;
    p.sla_tightness = 0.5;
    p.seed = seed;
    return generate(p);
}

Dataset DatasetGenerator::large_benchmark(uint32_t seed) {
    GeneratorParams p;
    p.num_services = 100;  p.num_nodes = 25;
    p.communication_density = 0.1;
    p.resource_variability = 0.5;
    p.sla_tightness = 0.5;
    p.seed = seed;
    return generate(p);
}

Dataset DatasetGenerator::very_large_benchmark(uint32_t seed) {
    GeneratorParams p;
    p.num_services = 250;  p.num_nodes = 50;
    p.communication_density = 0.05;
    p.resource_variability = 0.5;
    p.sla_tightness = 0.5;
    p.seed = seed;
    return generate(p);
}

}  // namespace qedgeopt
