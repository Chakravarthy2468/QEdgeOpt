# QEdgeOpt: DAA Viva Preparation Guide

This document contains concise answers to common questions expected during the Design and Analysis of Algorithms (DAA) viva for the QEdgeOpt project.

## 1. What is the optimization problem being solved?
We are solving the **Microservice Placement Problem** in a heterogeneous Edge/Fog/Cloudlet environment. Given a set of microservices with CPU, RAM, and Bandwidth demands, and a dependency graph detailing communication traffic, we must assign each microservice to exactly one infrastructure node. The goal is to minimize a composite cost function (communication cost, latency, resource fragmentation) while strictly respecting node capacity constraints and Service Level Agreements (SLA / Deadlines).

## 2. Why is microservice placement NP-hard?
The problem is a generalization of the **Multi-dimensional Bin Packing Problem (MDBP)** combined with **Graph Partitioning**. MDBP itself is strongly NP-hard. We are not just packing items (services) into bins (nodes) based on 3 dimensions (CPU, RAM, BW), but the cost of the packing depends on the relationship between items (communication edges crossing bins). Finding the optimal solution requires exploring a search space of size $O(N^S)$ where $N$ is nodes and $S$ is services.

## 3. What is a Quantum-Inspired Evolutionary Algorithm (QIEA)?
QIEA is an evolutionary algorithm that runs entirely on **classical hardware** but uses a probabilistic representation inspired by quantum mechanics. Instead of deterministic binary bits (0 or 1), it uses **Q-bits**. A Q-bit represents a linear superposition of states and collapses to 0 or 1 upon observation (measurement).

## 4. What is a Q-bit and how does measurement work?
A Q-bit is defined as $q = (\alpha, \beta)$ where $\alpha^2 + \beta^2 = 1$. 
- $\alpha^2$ is the probability of observing state 0.
- $\beta^2$ is the probability of observing state 1.
During **measurement**, a random number $r \in [0, 1)$ is generated. If $r < \beta^2$, the Q-bit collapses to 1; otherwise, it collapses to 0. This binary string is then decoded into node assignments.

## 5. What is the rotation gate (update mechanism)?
In classical GAs, population evolves via crossover and mutation. In QIEA, the primary evolution operator is the **Rotation Gate**. It shifts the probability amplitudes $(\alpha, \beta)$ of each Q-bit towards the state of the best-known solution. 
The matrix multiplication is:
$$ \begin{bmatrix} \alpha' \\ \beta' \end{bmatrix} = \begin{bmatrix} \cos(\Delta\theta) & -\sin(\Delta\theta) \\ \sin(\Delta\theta) & \cos(\Delta\theta) \end{bmatrix} \begin{bmatrix} \alpha \\ \beta \end{bmatrix} $$
The direction (sign) of $\Delta\theta$ is determined by comparing the currently observed binary bit with the corresponding bit in the elite solution.

## 6. How is fitness calculated?
Fitness is a composite objective function to be **minimized**:
`Fitness = w1(CommCost) + w2(Latency) + w3(Fragmentation) + w4(ActiveNodes) + Penalty`
Penalties are extremely high (e.g., $10^{12}$ or heavily weighted) for capacity or SLA violations, ensuring that infeasible solutions are strongly disfavored.

## 7. Why use a repair mechanism?
Because Q-bit measurement generates random binary strings, the resulting placement often violates node capacities (e.g., placing too many services on one Edge node). The deterministic heuristic **repair mechanism** detects overloaded nodes, selects the most demanding service, and moves it to a feasible node (preferably one that minimizes additional communication latency). This converts infeasible guesses into feasible boundary solutions, drastically improving convergence speed.

## 8. What is the difference between QIEA and the classical GA?
- **Representation:** GA uses a direct integer array `placement[i] = node_id`. QIEA uses a probabilistic Q-bit array.
- **Diversity:** QIEA naturally maintains genetic diversity early on (when $\alpha \approx \beta \approx 0.707$) and gradually converges as probabilities shift toward 0 or 1. GA relies on mutation for diversity.
- **Operators:** GA uses crossover (mixing parents). QIEA uses rotation gates (shifting probabilities toward an elite archetype).

## 9. What is the FFD baseline?
**First-Fit Decreasing (FFD)** is a greedy heuristic. It sorts microservices by their aggregate normalized demand in descending order. It then places each service into the first node that can accommodate it. Our SLA-aware variant checks all feasible nodes and picks the one yielding the lowest communication cost. It is very fast but gets trapped in local optima.

## 10. What is the time complexity of the algorithms?
Let $S$ = services, $N$ = nodes, $E$ = edges, $P$ = population size, $G$ = generations.
- **Fitness Evaluation:** $O(S + N + E)$
- **FFD:** $O(S \log S + S \cdot N)$
- **GA:** $O(G \cdot P \cdot (S \cdot N + E))$ (dominated by evaluation and repair)
- **QIEA:** $O(G \cdot P \cdot (L + S \cdot N + E))$ where $L = S \lceil\log_2 N\rceil$ is chromosome length.

## 11. How are experiments made fair?
All three algorithms (QIEA, GA, FFD) receive the exact same dataset, capacity constraints, SLA limits, and deterministic random seed. GA and QIEA use the same population size, generation limit, and fitness function evaluation logic. Benchmarks are aggregated over multiple seeds to prevent "lucky" initializations from skewing results.
