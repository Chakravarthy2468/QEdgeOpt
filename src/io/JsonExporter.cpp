/// @file JsonExporter.cpp
/// @brief Export solutions, benchmarks, and datasets to JSON.

#include "io/JsonExporter.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace qedgeopt {

static json fitness_to_json(const FitnessResult& f) {
    json jf;
    jf["total_fitness"]       = f.total_fitness;
    jf["communication_cost"]  = f.communication_cost;
    jf["latency_cost"]        = f.latency_cost;
    jf["fragmentation_cost"]  = f.fragmentation_cost;
    jf["active_node_cost"]    = f.active_node_cost;
    jf["penalty_capacity"]    = f.penalty_capacity;
    jf["penalty_latency"]     = f.penalty_latency;
    jf["penalty_deadline"]    = f.penalty_deadline;
    jf["sla_violations"]      = f.sla_violations;
    jf["deadline_violations"] = f.deadline_violations;
    jf["capacity_violations"] = f.capacity_violations;
    jf["active_nodes"]        = f.active_nodes;
    jf["avg_cpu_utilization"]  = f.avg_cpu_utilization;
    jf["avg_ram_utilization"]  = f.avg_ram_utilization;
    jf["avg_bw_utilization"]   = f.avg_bw_utilization;
    jf["avg_latency"]          = f.avg_latency;
    jf["max_latency"]          = f.max_latency;
    jf["cross_node_edges"]     = f.cross_node_edges;
    jf["intra_node_edges"]     = f.intra_node_edges;
    jf["total_cross_traffic"]  = f.total_cross_traffic;
    jf["is_feasible"]          = f.is_feasible();

    // Explanations
    json expl = json::array();
    for (const auto& e : f.explanations) {
        expl.push_back({
            {"service_id", e.service_id},
            {"node_id", e.node_id},
            {"cpu_feasible", e.cpu_feasible},
            {"ram_feasible", e.ram_feasible},
            {"bw_feasible", e.bw_feasible},
            {"sla_met", e.sla_met},
            {"deadline_met", e.deadline_met},
            {"added_comm_cost", e.added_comm_cost},
            {"reason", e.reason}
        });
    }
    jf["explanations"] = expl;

    // Node usages
    json nodes = json::array();
    for (const auto& nu : f.node_usages) {
        nodes.push_back({
            {"node_id", nu.node_id},
            {"cpu_used", nu.cpu_used},
            {"cpu_cap", nu.cpu_cap},
            {"ram_used", nu.ram_used},
            {"ram_cap", nu.ram_cap},
            {"bw_used", nu.bw_used},
            {"bw_cap", nu.bw_cap},
            {"services_hosted", nu.services_hosted}
        });
    }
    jf["node_usages"] = nodes;

    return jf;
}

std::string JsonExporter::solution_to_json(const Solution& sol, const Dataset& ds)
{
    json j;
    j["algorithm"]       = to_string(sol.algorithm);
    j["status"]          = to_string(sol.status);
    j["runtime_ms"]      = sol.runtime_ms;
    j["generations_run"] = sol.generations_run;
    j["seed_used"]       = sol.seed_used;
    j["fitness"]         = fitness_to_json(sol.fitness);
    j["convergence"]     = sol.convergence;

    // Placement table
    json placement_table = json::array();
    for (int i = 0; i < ds.num_services(); ++i) {
        int node_idx = (i < (int)sol.placement.size()) ? sol.placement[i] : -1;
        std::string node_id = (node_idx >= 0 && node_idx < ds.num_nodes())
                              ? ds.nodes[node_idx].id : "N/A";
        placement_table.push_back({
            {"service_id", ds.services[i].id},
            {"node_id", node_id},
            {"node_index", node_idx},
            {"cpu_demand", ds.services[i].cpu_demand},
            {"ram_demand", ds.services[i].ram_demand},
            {"bw_demand", ds.services[i].bw_demand},
            {"deadline", ds.services[i].deadline}
        });
    }
    j["placement"] = placement_table;

    return j.dump(2);
}

std::string JsonExporter::benchmark_to_json(const BenchmarkResult& br,
                                            const Dataset& ds)
{
    (void)ds;
    json j;
    j["algorithm"]         = to_string(br.algorithm);
    j["num_runs"]          = static_cast<int>(br.runs.size());
    j["best_fitness"]      = br.best_fitness;
    j["avg_fitness"]       = br.avg_fitness;
    j["std_fitness"]       = br.std_fitness;
    j["avg_runtime_ms"]    = br.avg_runtime_ms;
    j["feasible_rate"]     = br.feasible_rate;
    j["avg_cpu_util"]      = br.avg_cpu_util;
    j["avg_ram_util"]      = br.avg_ram_util;
    j["avg_bw_util"]       = br.avg_bw_util;
    j["avg_comm_cost"]     = br.avg_comm_cost;
    j["avg_latency"]       = br.avg_latency;
    j["avg_max_latency"]   = br.avg_max_latency;
    j["avg_active_nodes"]  = br.avg_active_nodes;
    j["avg_sla_violations"] = br.avg_sla_violations;
    j["avg_deadline_violations"] = br.avg_deadline_violations;
    j["convergence_gen"]   = br.convergence_gen;

    // Per-run details
    json runs = json::array();
    for (const auto& sol : br.runs) {
        json r;
        r["seed"]       = sol.seed_used;
        r["fitness"]    = sol.fitness.total_fitness;
        r["runtime_ms"] = sol.runtime_ms;
        r["feasible"]   = sol.is_feasible();
        r["sla_violations"] = sol.fitness.sla_violations;
        r["convergence"] = sol.convergence;
        runs.push_back(r);
    }
    j["runs"] = runs;

    return j.dump(2);
}

std::string JsonExporter::comparison_to_json(
    const std::vector<BenchmarkResult>& results, const Dataset& ds)
{
    json j;
    j["dataset_info"] = {
        {"num_services", ds.num_services()},
        {"num_nodes", ds.num_nodes()},
        {"num_links", ds.num_links()},
        {"sla_max_latency", ds.sla_max_latency}
    };

    json algos = json::array();
    for (const auto& br : results) {
        json a;
        a["algorithm"]      = to_string(br.algorithm);
        a["best_fitness"]   = br.best_fitness;
        a["avg_fitness"]    = br.avg_fitness;
        a["std_fitness"]    = br.std_fitness;
        a["avg_runtime_ms"] = br.avg_runtime_ms;
        a["feasible_rate"]  = br.feasible_rate;
        a["avg_cpu_util"]   = br.avg_cpu_util;
        a["avg_ram_util"]   = br.avg_ram_util;
        a["avg_bw_util"]    = br.avg_bw_util;
        a["avg_comm_cost"]  = br.avg_comm_cost;
        a["avg_latency"]    = br.avg_latency;
        a["avg_active_nodes"] = br.avg_active_nodes;
        a["avg_sla_violations"] = br.avg_sla_violations;

        // Best run convergence
        if (!br.runs.empty()) {
            double best = 1e18;
            int best_idx = 0;
            for (int i = 0; i < (int)br.runs.size(); ++i) {
                if (br.runs[i].fitness.total_fitness < best) {
                    best = br.runs[i].fitness.total_fitness;
                    best_idx = i;
                }
            }
            a["best_convergence"] = br.runs[best_idx].convergence;
        }

        algos.push_back(a);
    }
    j["algorithms"] = algos;

    return j.dump(2);
}

std::string JsonExporter::dataset_to_json(const Dataset& ds)
{
    json j;

    json nodes = json::array();
    for (const auto& n : ds.nodes) {
        nodes.push_back({
            {"id", n.id},
            {"type", to_string(n.type)},
            {"cpu", n.cpu_capacity},
            {"ram", n.ram_capacity},
            {"bandwidth", n.bw_capacity},
            {"processing_delay", n.processing_delay}
        });
    }
    j["nodes"] = nodes;

    json services = json::array();
    for (const auto& s : ds.services) {
        services.push_back({
            {"id", s.id},
            {"cpu", s.cpu_demand},
            {"ram", s.ram_demand},
            {"bandwidth", s.bw_demand},
            {"deadline", s.deadline}
        });
    }
    j["services"] = services;

    json links = json::array();
    for (const auto& l : ds.links) {
        links.push_back({
            {"source", l.source},
            {"target", l.target},
            {"traffic", l.traffic}
        });
    }
    j["links"] = links;

    j["node_latency"] = ds.node_latency;
    j["sla"] = {{"max_end_to_end_latency", ds.sla_max_latency}};

    return j.dump(2);
}

}  // namespace qedgeopt
