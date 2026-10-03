#pragma once
/// @file QIEA.h
/// @brief Quantum-Inspired Evolutionary Algorithm for microservice placement.
///
/// IMPORTANT ACADEMIC NOTE:
/// This is a QUANTUM-INSPIRED algorithm running entirely on a classical CPU.
/// It does NOT use real quantum hardware. The "quantum" aspect is a probabilistic
/// representation inspired by quantum computing concepts:
///   - Each decision variable is represented as a Q-bit: q = (alpha, beta)
///   - The constraint alpha² + beta² = 1 is maintained (unit circle)
///   - P(observing state |1⟩) = beta²
///   - Rotation gates update the probability distribution toward better solutions
/// This is analogous to how quantum annealing explores solution spaces, but
/// implemented entirely with classical floating-point arithmetic.

#include "algorithms/IAlgorithm.h"
#include "utils/Random.h"

#include <vector>
#include <cmath>

namespace qedgeopt {

// Provide M_SQRT1_2 if not available (MSVC / strict standard)
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/// A single Q-bit: probability amplitudes on the unit circle.
struct QBit {
    double alpha = M_SQRT1_2;  // 1/√2 ≈ 0.7071 — equal superposition
    double beta  = M_SQRT1_2;

    /// Probability of measuring |1⟩.
    double prob_one() const { return beta * beta; }

    /// Probability of measuring |0⟩.
    double prob_zero() const { return alpha * alpha; }

    /// Current angle θ on the unit circle (alpha = cos θ, beta = sin θ).
    double theta() const { return std::atan2(beta, alpha); }

    /// Set from angle.
    void set_theta(double t) {
        alpha = std::cos(t);
        beta  = std::sin(t);
    }

    /// Normalise to maintain alpha² + beta² = 1 (numerical safety).
    void normalise() {
        double norm = std::sqrt(alpha * alpha + beta * beta);
        if (norm > 1e-15) { alpha /= norm; beta /= norm; }
        else { alpha = M_SQRT1_2; beta = M_SQRT1_2; }
    }
};

/// A Q-bit individual: one Q-bit per bit of the encoded chromosome.
struct QIndividual {
    std::vector<QBit> qbits;        ///< Probabilistic representation
    Placement         observed;     ///< Last measured (collapsed) placement
    double            fitness = 1e18;
};

class QIEA : public IAlgorithm {
public:
    std::string   name() const override { return "QIEA"; }
    AlgorithmType type() const override { return AlgorithmType::QIEA; }

    Solution solve(const Dataset& ds,
                   const AlgorithmConfig& config,
                   ProgressCallback callback = nullptr) override;

private:
    // ── Encoding ─────────────────────────────────────────
    /// Number of bits needed to encode one node index.
    int bits_per_node(int num_nodes) const;

    /// Total chromosome length = num_services × bits_per_node.
    int chromosome_length(int num_services, int num_nodes) const;

    // ── Core QIEA operations ─────────────────────────────
    /// Initialise population of Q-bit individuals.
    void initialise_population(std::vector<QIndividual>& pop,
                               int pop_size, int chrom_len, Random& rng);

    /// Measure (collapse) Q-bits into a binary string, then decode to placement.
    Placement measure(const QIndividual& ind, int num_services,
                      int num_nodes, int bpn, Random& rng);

    /// Decode a binary string into a placement vector.
    Placement decode_binary(const std::vector<int>& bits,
                            int num_services, int num_nodes, int bpn);

    /// Update Q-bits using rotation gate toward the best-known solution.
    void rotate_qbits(QIndividual& ind,
                      const Placement& best_placement,
                      int num_nodes, int bpn,
                      double delta_theta);

    /// Apply random mutation to Q-bits for diversity.
    void mutate_qbits(QIndividual& ind, double mut_prob, Random& rng);

    /// Measure population diversity (fraction of distinct placements).
    double measure_diversity(const std::vector<QIndividual>& pop);

    /// Re-initialise low-diversity individuals.
    void enforce_diversity(std::vector<QIndividual>& pop,
                           int chrom_len, double threshold, Random& rng);

    // ── Repair ───────────────────────────────────────────
    /// Repair infeasible placements by moving services from overloaded nodes.
    Placement repair(const Dataset& ds, Placement placement, Random& rng);
};

}  // namespace qedgeopt

