/// @file DatasetLoader.cpp
/// @brief Load Dataset from JSON using nlohmann/json.

#include "io/DatasetLoader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;

namespace qedgeopt {

static Dataset parse_json(const json& j) {
    Dataset ds;

    // ── Nodes ────────────────────────────────────────────
    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (const auto& jn : j["nodes"]) {
            Node node;
            node.id            = jn.value("id", "");
            node.type          = node_type_from_string(jn.value("type", "EDGE"));
            node.cpu_capacity  = jn.value("cpu", 0.0);
            node.ram_capacity  = jn.value("ram", 0.0);
            node.bw_capacity   = jn.value("bandwidth", 0.0);
            node.processing_delay = jn.value("processing_delay", 0.0);
            ds.nodes.push_back(node);
        }
    }

    // ── Services ─────────────────────────────────────────
    if (j.contains("services") && j["services"].is_array()) {
        for (const auto& js : j["services"]) {
            Service svc;
            svc.id         = js.value("id", "");
            svc.cpu_demand = js.value("cpu", 0.0);
            svc.ram_demand = js.value("ram", 0.0);
            svc.bw_demand  = js.value("bandwidth", 0.0);
            svc.deadline   = js.value("deadline", -1.0);
            ds.services.push_back(svc);
        }
    }

    // ── Links ────────────────────────────────────────────
    if (j.contains("links") && j["links"].is_array()) {
        for (const auto& jl : j["links"]) {
            Link lnk;
            lnk.source  = jl.value("source", "");
            lnk.target  = jl.value("target", "");
            lnk.traffic = jl.value("traffic", 0.0);
            ds.links.push_back(lnk);
        }
    }

    // ── Node-to-node latency matrix (optional) ───────────
    if (j.contains("node_latency") && j["node_latency"].is_array()) {
        ds.node_latency.clear();
        for (const auto& row : j["node_latency"]) {
            std::vector<double> lat_row;
            for (const auto& val : row)
                lat_row.push_back(val.get<double>());
            ds.node_latency.push_back(lat_row);
        }
    }

    // ── SLA ──────────────────────────────────────────────
    if (j.contains("sla")) {
        const auto& sla = j["sla"];
        ds.sla_max_latency = sla.value("max_end_to_end_latency", DEFAULT_SLA_LIMIT);
    }

    // Build indices and validate
    ds.build_indices();

    std::string err = ds.validate();
    if (!err.empty()) {
        throw std::runtime_error("Dataset validation failed: " + err);
    }

    return ds;
}

Dataset DatasetLoader::load_from_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open dataset file: " + path);

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("JSON parse error in " + path + ": " + e.what());
    }

    return parse_json(j);
}

Dataset DatasetLoader::load_from_string(const std::string& json_str)
{
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    return parse_json(j);
}

}  // namespace qedgeopt
