/// @file main.cpp
/// @brief Entry point for the QEdgeOpt application.

#include "server/ApiServer.h"
#include <iostream>
#include <string>

using namespace qedgeopt;

int main(int argc, char** argv) {
    int port = 8080;
    std::string web_dir = "web";

    // Basic argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = std::stoi(argv[++i]);
            }
        } else if (arg == "-w" || arg == "--web") {
            if (i + 1 < argc) {
                web_dir = argv[++i];
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: qedgeopt [options]\n"
                      << "Options:\n"
                      << "  -p, --port <port>    Port to listen on (default: 8080)\n"
                      << "  -w, --web <dir>      Directory serving static web files (default: web)\n"
                      << "  -h, --help           Show this help message\n";
            return 0;
        }
    }

    std::cout << "============================================================\n"
              << " QEdgeOpt: Quantum-Inspired Edge Microservice Optimizer\n"
              << "============================================================\n";

    ApiServer server;
    try {
        server.start(port, web_dir);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
