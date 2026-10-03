#pragma once
/// @file ApiServer.h
/// @brief Local REST API server for the QEdgeOpt dashboard.

#include "models/Dataset.h"
#include "models/Solution.h"
#include "models/AlgorithmConfig.h"
#include "io/DatasetGenerator.h"

#include <string>
#include <memory>
#include <mutex>
#include <atomic>

namespace qedgeopt {

class ApiServer {
public:
    /// Start the HTTP server on the given port.
    /// Blocks until the server is stopped.
    void start(int port = 8080, const std::string& web_dir = "web");

    /// Stop the server gracefully.
    void stop();

private:
    // ── State (protected by mutex) ───────────────────────
    std::mutex         mtx_;
    Dataset            current_dataset_;
    bool               dataset_loaded_ = false;
    Solution           last_solution_;
    std::vector<Solution> benchmark_solutions_;
    std::atomic<bool>  optimizing_{false};

    // ── Helpers ──────────────────────────────────────────
    AlgorithmConfig parse_config(const std::string& json_body);
    GeneratorParams parse_generator_params(const std::string& json_body);
    std::string     error_json(const std::string& msg);
    std::string     ok_json(const std::string& msg);
};

}  // namespace qedgeopt
