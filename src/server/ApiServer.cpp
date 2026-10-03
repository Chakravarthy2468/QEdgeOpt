/// @file ApiServer.cpp
/// @brief Local REST API server implementation using cpp-httplib.

#include "server/ApiServer.h"
#include "io/DatasetLoader.h"
#include "io/JsonExporter.h"
#include "algorithms/QIEA.h"
#include "algorithms/GeneticAlgorithm.h"
#include "algorithms/FFD.h"
#include "evaluation/Metrics.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>

using json = nlohmann::json;

namespace qedgeopt {

AlgorithmConfig ApiServer::parse_config(const std::string& json_body) {
    AlgorithmConfig cfg;
    if (json_body.empty()) return cfg;

    try {
        auto j = json::parse(json_body);
        if (j.contains("seed")) cfg.seed = j["seed"];

        if (j.contains("weights")) {
            auto jw = j["weights"];
            if (jw.contains("w_communication")) cfg.weights.w_communication = jw["w_communication"];
            if (jw.contains("w_latency"))       cfg.weights.w_latency = jw["w_latency"];
            if (jw.contains("w_fragmentation")) cfg.weights.w_fragmentation = jw["w_fragmentation"];
            if (jw.contains("w_active_nodes"))  cfg.weights.w_active_nodes = jw["w_active_nodes"];
            if (jw.contains("penalty_capacity")) cfg.weights.penalty_capacity = jw["penalty_capacity"];
            if (jw.contains("penalty_latency"))  cfg.weights.penalty_latency = jw["penalty_latency"];
            if (jw.contains("penalty_deadline")) cfg.weights.penalty_deadline = jw["penalty_deadline"];
        }
        if (j.contains("qiea")) {
            auto jq = j["qiea"];
            if (jq.contains("population_size")) cfg.qiea.population_size = jq["population_size"];
            if (jq.contains("max_generations")) cfg.qiea.max_generations = jq["max_generations"];
            if (jq.contains("rotation_angle"))  cfg.qiea.rotation_angle = jq["rotation_angle"];
            if (jq.contains("mutation_prob"))   cfg.qiea.mutation_prob = jq["mutation_prob"];
            if (jq.contains("elite_count"))     cfg.qiea.elite_count = jq["elite_count"];
        }
        if (j.contains("ga")) {
            auto jg = j["ga"];
            if (jg.contains("population_size")) cfg.ga.population_size = jg["population_size"];
            if (jg.contains("max_generations")) cfg.ga.max_generations = jg["max_generations"];
            if (jg.contains("crossover_prob"))  cfg.ga.crossover_prob = jg["crossover_prob"];
            if (jg.contains("mutation_prob"))   cfg.ga.mutation_prob = jg["mutation_prob"];
            if (jg.contains("tournament_size")) cfg.ga.tournament_size = jg["tournament_size"];
            if (jg.contains("elite_count"))     cfg.ga.elite_count = jg["elite_count"];
        }
        if (j.contains("benchmark")) {
            auto jb = j["benchmark"];
            if (jb.contains("seeds")) {
                cfg.benchmark.seeds.clear();
                for (auto& s : jb["seeds"]) cfg.benchmark.seeds.push_back(s);
            }
            if (jb.contains("run_qiea")) cfg.benchmark.run_qiea = jb["run_qiea"];
            if (jb.contains("run_ga"))   cfg.benchmark.run_ga   = jb["run_ga"];
            if (jb.contains("run_ffd"))  cfg.benchmark.run_ffd  = jb["run_ffd"];
        }
    } catch (...) {}
    return cfg;
}

GeneratorParams ApiServer::parse_generator_params(const std::string& json_body) {
    GeneratorParams p;
    if (json_body.empty()) return p;
    try {
        auto j = json::parse(json_body);
        if (j.contains("num_services"))          p.num_services = j["num_services"];
        if (j.contains("num_nodes"))             p.num_nodes = j["num_nodes"];
        if (j.contains("communication_density")) p.communication_density = j["communication_density"];
        if (j.contains("resource_variability"))  p.resource_variability = j["resource_variability"];
        if (j.contains("sla_tightness"))         p.sla_tightness = j["sla_tightness"];
        if (j.contains("seed"))                  p.seed = j["seed"];
    } catch (...) {}
    return p;
}

std::string ApiServer::error_json(const std::string& msg) {
    return json{{"status", "error"}, {"message", msg}}.dump();
}

std::string ApiServer::ok_json(const std::string& msg) {
    return json{{"status", "ok"}, {"message", msg}}.dump();
}

void ApiServer::start(int port, const std::string& web_dir) {
    httplib::Server svr;

    // Static files
    svr.set_mount_point("/", web_dir);

    // ── API: Status ──────────────────────────────────────
    svr.Get("/api/status", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx_);
        json j;
        j["status"] = "ok";
        j["dataset_loaded"] = dataset_loaded_;
        if (dataset_loaded_) {
            j["num_services"] = current_dataset_.num_services();
            j["num_nodes"]    = current_dataset_.num_nodes();
        }
        j["optimizing"] = optimizing_.load();
        res.set_content(j.dump(), "application/json");
    });

    // ── API: Generate Dataset ────────────────────────────
    svr.Post("/api/dataset/generate", [&](const httplib::Request& req, httplib::Response& res) {
        auto params = parse_generator_params(req.body);
        try {
            auto ds = DatasetGenerator::generate(params);
            std::lock_guard<std::mutex> lock(mtx_);
            current_dataset_ = std::move(ds);
            dataset_loaded_ = true;
            res.set_content(JsonExporter::dataset_to_json(current_dataset_), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(error_json(e.what()), "application/json");
        }
    });

    // ── API: Upload Dataset ──────────────────────────────
    svr.Post("/api/dataset/upload", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto ds = DatasetLoader::load_from_string(req.body);
            std::lock_guard<std::mutex> lock(mtx_);
            current_dataset_ = std::move(ds);
            dataset_loaded_ = true;
            res.set_content(ok_json("Dataset loaded successfully"), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(error_json(e.what()), "application/json");
        }
    });

    // ── API: Get Dataset ─────────────────────────────────
    svr.Get("/api/dataset", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!dataset_loaded_) {
            res.status = 404;
            res.set_content(error_json("No dataset loaded"), "application/json");
            return;
        }
        res.set_content(JsonExporter::dataset_to_json(current_dataset_), "application/json");
    });

    // ── Macro for algorithm endpoints ────────────────────
    auto handle_algo = [&](const httplib::Request& req, httplib::Response& res,
                           std::shared_ptr<IAlgorithm> algo) {
        if (optimizing_.exchange(true)) {
            res.status = 429;
            res.set_content(error_json("Optimization already in progress"), "application/json");
            return;
        }

        AlgorithmConfig config = parse_config(req.body);
        Dataset ds;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!dataset_loaded_) {
                optimizing_ = false;
                res.status = 400;
                res.set_content(error_json("No dataset loaded"), "application/json");
                return;
            }
            ds = current_dataset_; // copy for thread safety
        }

        try {
            // No live callback for standard synchronous HTTP request
            // For a real async dashboard, you'd use websockets or server-sent events.
            // We'll keep it simple and block.
            auto sol = algo->solve(ds, config, nullptr);

            std::lock_guard<std::mutex> lock(mtx_);
            last_solution_ = sol;
            optimizing_ = false;
            res.set_content(JsonExporter::solution_to_json(sol, ds), "application/json");
        } catch (const std::exception& e) {
            optimizing_ = false;
            res.status = 500;
            res.set_content(error_json(e.what()), "application/json");
        }
    };

    // ── API: Run QIEA ────────────────────────────────────
    svr.Post("/api/optimize/qiea", [&](const httplib::Request& req, httplib::Response& res) {
        handle_algo(req, res, std::make_shared<QIEA>());
    });

    // ── API: Run GA ──────────────────────────────────────
    svr.Post("/api/optimize/ga", [&](const httplib::Request& req, httplib::Response& res) {
        handle_algo(req, res, std::make_shared<GeneticAlgorithm>());
    });

    // ── API: Run FFD ─────────────────────────────────────
    svr.Post("/api/optimize/ffd", [&](const httplib::Request& req, httplib::Response& res) {
        handle_algo(req, res, std::make_shared<FFD>());
    });

    // ── API: Run Benchmark ───────────────────────────────
    svr.Post("/api/benchmark", [&](const httplib::Request& req, httplib::Response& res) {
        if (optimizing_.exchange(true)) {
            res.status = 429;
            res.set_content(error_json("Optimization in progress"), "application/json");
            return;
        }

        AlgorithmConfig config = parse_config(req.body);
        Dataset ds;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!dataset_loaded_) {
                optimizing_ = false;
                res.status = 400;
                res.set_content(error_json("No dataset loaded"), "application/json");
                return;
            }
            ds = current_dataset_;
        }

        std::vector<BenchmarkResult> results;
        try {
            if (config.benchmark.run_qiea) {
                QIEA qiea;
                std::vector<Solution> runs;
                for (int s : config.benchmark.seeds) {
                    config.seed = s;
                    runs.push_back(qiea.solve(ds, config));
                }
                results.push_back(Metrics::aggregate(AlgorithmType::QIEA, runs));
            }
            if (config.benchmark.run_ga) {
                GeneticAlgorithm ga;
                std::vector<Solution> runs;
                for (int s : config.benchmark.seeds) {
                    config.seed = s;
                    runs.push_back(ga.solve(ds, config));
                }
                results.push_back(Metrics::aggregate(AlgorithmType::GA, runs));
            }
            if (config.benchmark.run_ffd) {
                FFD ffd;
                std::vector<Solution> runs;
                // FFD is deterministic except for tie-breaking, but we'll run it once per seed for fairness
                for (int s : config.benchmark.seeds) {
                    config.seed = s;
                    runs.push_back(ffd.solve(ds, config));
                }
                results.push_back(Metrics::aggregate(AlgorithmType::FFD, runs));
            }

            optimizing_ = false;
            res.set_content(JsonExporter::comparison_to_json(results, ds), "application/json");
        } catch (const std::exception& e) {
            optimizing_ = false;
            res.status = 500;
            res.set_content(error_json(e.what()), "application/json");
        }
    });

    std::cout << "Starting QEdgeOpt server on http://localhost:" << port << std::endl;
    std::cout << "Serving static files from: " << web_dir << std::endl;
    svr.listen("0.0.0.0", port);
}

void ApiServer::stop() {
    // httplib doesn't have a simple async stop without passing the svr around,
    // but this suffices for the CLI entry point.
}

}  // namespace qedgeopt
