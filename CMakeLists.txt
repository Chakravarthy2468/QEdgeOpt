cmake_minimum_required(VERSION 3.16)
project(QEdgeOpt
    VERSION 1.0.0
    DESCRIPTION "Quantum-Inspired Edge Microservice Placement Optimizer"
    LANGUAGES CXX
)

# ── C++ Standard ──────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build Type Default ────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ── Compiler Warnings ────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Fetch Dependencies ───────────────────────────────────
include(FetchContent)

# nlohmann/json — header-only JSON library
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# cpp-httplib — header-only HTTP server
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.15.3
    GIT_SHALLOW    TRUE
)

set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json httplib)

# ── Source Files ──────────────────────────────────────────
set(QEDGEOPT_SOURCES
    src/main.cpp
    src/evaluation/ConstraintChecker.cpp
    src/evaluation/Fitness.cpp
    src/evaluation/Metrics.cpp
    src/algorithms/FFD.cpp
    src/algorithms/GeneticAlgorithm.cpp
    src/algorithms/QIEA.cpp
    src/io/DatasetLoader.cpp
    src/io/JsonExporter.cpp
    src/io/DatasetGenerator.cpp
    src/server/ApiServer.cpp
)

# ── Main Executable ──────────────────────────────────────
add_executable(qedgeopt ${QEDGEOPT_SOURCES})

target_include_directories(qedgeopt PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(qedgeopt PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
)

# On Windows, link Ws2_32 for networking
if(WIN32)
    target_link_libraries(qedgeopt PRIVATE ws2_32)
endif()

# ── Install web assets next to the binary ─────────────────
add_custom_command(TARGET qedgeopt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/web
        $<TARGET_FILE_DIR:qedgeopt>/web
    COMMENT "Copying web assets..."
)

add_custom_command(TARGET qedgeopt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:qedgeopt>/data
    COMMENT "Copying data files..."
)

# ── Tests (optional) ─────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()

    set(TEST_SOURCES
        tests/test_main.cpp
    )

    add_executable(qedgeopt_tests ${TEST_SOURCES}
        src/evaluation/ConstraintChecker.cpp
        src/evaluation/Fitness.cpp
        src/evaluation/Metrics.cpp
        src/algorithms/FFD.cpp
        src/algorithms/GeneticAlgorithm.cpp
        src/algorithms/QIEA.cpp
        src/io/DatasetLoader.cpp
        src/io/JsonExporter.cpp
        src/io/DatasetGenerator.cpp
    )

    target_include_directories(qedgeopt_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(qedgeopt_tests PRIVATE
        nlohmann_json::nlohmann_json
        httplib::httplib
    )

    if(WIN32)
        target_link_libraries(qedgeopt_tests PRIVATE ws2_32)
    endif()

    add_custom_command(TARGET qedgeopt_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/data
            $<TARGET_FILE_DIR:qedgeopt_tests>/data
        COMMENT "Copying data files for tests..."
    )

    add_test(NAME QEdgeOptTests COMMAND qedgeopt_tests)
endif()

message(STATUS "╔══════════════════════════════════════════════╗")
message(STATUS "║  QEdgeOpt — Build Configuration              ║")
message(STATUS "╠══════════════════════════════════════════════╣")
message(STATUS "║  C++ Standard : ${CMAKE_CXX_STANDARD}                        ║")
message(STATUS "║  Build Type   : ${CMAKE_BUILD_TYPE}                     ║")
message(STATUS "║  Tests        : ${BUILD_TESTS}                         ║")
message(STATUS "╚══════════════════════════════════════════════╝")
