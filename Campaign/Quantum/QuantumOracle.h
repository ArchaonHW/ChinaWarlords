#pragma once

#include "Gameplay/Result.h"
#include "Gameplay/Sim/Sim.h" // Prng — the campaign's seeded stream

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace Potato {

namespace Gameplay { class JsonValue; }

namespace Campaign {

// QuantumOracle (Story 13.2) — the campaign-layer adapter over the
// engine's Quantum module (state-vector simulator, FP internals).
//
// BOUNDARY (D-ARCH-11): this is the ONLY sanctioned crossing point.
// Callers speak integers — permille probabilities and index weights
// in, measured outcomes out. std::complex<double> amplitudes never
// leave this unit; nothing in Gameplay/ may include Quantum/.
//
// DETERMINISM: every draw builds a fresh circuit whose measurement
// roll comes from an injected IRandomSource backed by this oracle's
// own Gameplay::Prng stream (the campaign stream discipline — one
// seeded stream, canonical draw order). StreamState() exposes the
// raw counter for save persistence: FromSeed(seed, state) resumes
// the stream bit-exactly (Prng is counter-based, no degenerate
// states).
//
// FP caveat (recorded, accepted): the amplitude math is IEEE-754;
// cross-platform ULP drift could flip a measurement only when the
// roll lands within ~1e-15 of the probability boundary. The sampled
// outcome is what the ledger books — replay reads the record, never
// re-runs the circuit.
//
// Degenerate draws (permille 0/1000, single/outcome-forced
// categoricals) short-circuit WITHOUT consuming the stream, so the
// PRNG position stays aligned between oracle-configured and
// fallback paths.

// potato.oracle/1 — minimal config: the stream is all the oracle
// owns; probabilities are supplied per-draw by the caller.
//   {"schema":"potato.oracle/1","seed":<u64>,"state":<u64, optional>}
// seed is REQUIRED and must be nonzero — the engine module treats
// seed 0 as "OS entropy", which would silently break reproducibility.
// state is the resume counter written back by StreamState().

class QuantumOracle {
public:
    // Programmatic construction (tests, fallback wiring).
    explicit QuantumOracle(std::uint64_t seed, std::uint64_t state = 0);

    // Result<T>'s `T value{}` placeholder — deterministically
    // seeded (never the OS-entropy path FromJson guards), and
    // always overwritten by Ok's move-assign before use.
    QuantumOracle() : QuantumOracle(1, 0) {}

    // Versioned-doc construction. Pure validation — rejects without
    // touching *this.
    static Gameplay::Result<QuantumOracle> FromJson(
        const Gameplay::JsonValue& doc);
    // File boundary: loads + validates a potato.oracle/1 document.
    static Gameplay::Result<QuantumOracle> Load(
        std::string_view path);

    // Bernoulli draw — permille ∈ [0,1000] returns true with
    // probability permille/1000. Circuit: |0⟩ → Ry(2·asin(√p)) →
    // Measure — exact Bernoulli via Born rule. 0 and 1000
    // short-circuit without consuming the stream.
    bool DrawPermille(int permille);

    // Categorical draw over integer weights; returns an index in
    // [0, weights.size()). Zero-weight entries are unreachable.
    // Empty or all-zero weights return -1 — the caller decides;
    // the oracle never silently normalizes an empty question.
    // Single nonzero weight returns its index without consuming.
    int DrawCategorical(const int* weights, int count);
    int DrawCategorical(const std::vector<int>& weights) {
        return DrawCategorical(weights.data(),
                               static_cast<int>(weights.size()));
    }

    // Stream position for persistence: FromSeed(seed, StreamState())
    // resumes bit-exactly.
    std::uint64_t StreamState() const { return rng_.State(); }

private:
    // The oracle's own stream — callers never draw through it
    // directly; it exists only to feed injected IRandomSource
    // adapters (one per fresh circuit, forwarding to this member).
    Gameplay::Prng rng_;
};

} // namespace Campaign
} // namespace Potato
