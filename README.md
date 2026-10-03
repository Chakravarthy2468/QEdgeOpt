# QEdgeOpt
**Quantum-Inspired Edge Microservice Placement Optimizer**

QEdgeOpt is a C++-based optimization engine designed to solve the NP-hard microservice placement problem across heterogeneous Edge, Fog, and Cloudlet infrastructure. 

It implements a **Quantum-Inspired Evolutionary Algorithm (QIEA)** and compares it against Classical Genetic Algorithm (GA) and First-Fit Decreasing (FFD) heuristics, strictly respecting multi-dimensional resource capacities and network SLA constraints.

---

## ⚠️ Academic Note
**This is a Quantum-Inspired algorithm running on classical CPU hardware.**
It does not use physical qubits or quantum processing units (QPUs). Instead, it uses a probabilistic mathematical representation (Q-bits) and rotation gates inspired by quantum superposition and interference to efficiently explore the combinatorial search space.

---

## Features
- **Three Algorithms:** QIEA, Classical GA, and SLA-aware FFD.
- **Strict Constraints:** Evaluates multi-dimensional bin packing (CPU, RAM, Bandwidth) and end-to-end communication latencies (SLA).
- **Synthetic Benchmarks:** Built-in reproducible dataset generator.
- **REST API:** `cpp-httplib` server exposing optimization endpoints.
- **Dashboard:** HTML5/TailwindCSS frontend with Chart.js convergence plotting and placement explainability.

## System Architecture

```text
QEdgeOpt/
├── src/
│   ├── algorithms/       # QIEA, GA, FFD implementations
│   ├── evaluation/       # Constraint checker, Fitness, Metrics
│   ├── io/               # JSON parsing, Dataset Generation
│   └── server/           # REST API
├── include/              # Header files
├── web/                  # HTML/JS/CSS Dashboard
├── tests/                # Unit Tests
└── CMakeLists.txt        # Build config
```

## Build Instructions (CMake)

### Prerequisites
- Modern C++17 Compiler (GCC, Clang, or MSVC)
- CMake 3.16+

### One-Click Launch
**Windows:**
```cmd
run.bat
```
**Linux / macOS:**
```bash
./run.sh
```

### Manual Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./qedgeopt
```
The application starts a web server on `http://localhost:8080`.

## API Documentation

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/status` | GET | Check server status and loaded dataset. |
| `/api/dataset/generate` | POST | Generate a synthetic dataset (pass JSON params). |
| `/api/dataset/upload` | POST | Upload a JSON dataset. |
| `/api/optimize/qiea` | POST | Run QIEA optimization. |
| `/api/optimize/ga` | POST | Run Classical GA optimization. |
| `/api/optimize/ffd` | POST | Run FFD optimization. |
| `/api/benchmark` | POST | Run multi-seed benchmark across algorithms. |

## Dataset JSON Schema
See `DatasetLoader` for full details. Basic structure:
```json
{
  "nodes": [
    { "id": "E1", "type": "EDGE", "cpu": 16, "ram": 32, "bandwidth": 1000, "processing_delay": 2.0 }
  ],
  "services": [
    { "id": "S1", "cpu": 2, "ram": 4, "bandwidth": 100, "deadline": 50 }
  ],
  "links": [
    { "source": "S1", "target": "S2", "traffic": 50 }
  ],
  "sla": {
    "max_end_to_end_latency": 100
  }
}
```

## DAA Viva Reference
Please see [docs/viva.md](docs/viva.md) for theoretical background, mathematical formulations, and complexity analysis intended for university evaluations.
