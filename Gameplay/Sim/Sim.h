#pragma once

#include <cstdint>

namespace Potato::Gameplay {

// Fixed-tick simulation rate. All sim time derives from integer ticks.
constexpr std::int32_t TICK_RATE_HZ = 20;

// SplitMix64 seeded PRNG — the single sanctioned random stream for the
// sim. Counter-based: state += golden-ratio increment per draw, then
// avalanche — one period-2^64 stream; the seed selects the offset into
// it (seed 0 is fully valid, no degenerate state, no remap branch).
// Draws happen in canonical evaluation order only.
class Prng {
public:
    explicit Prng(std::uint64_t seed) : state_(seed) {}
    std::uint64_t Next();
    // Raw counter for persistence: Prng(p.State()) resumes the
    // stream bit-exactly (counter-based — no degenerate states).
    std::uint64_t State() const { return state_; }
private:
    std::uint64_t state_;
};

// Minimal deterministic simulation kernel shell.
// Owns the battle's seeded PRNG stream — no global rand(), no floats.
// BattleState arrays and doctrine evaluation arrive in later stories;
// this skeleton proves the compile/link/tick contract and the
// determinism convention (same seed -> same checksum).
class Sim {
public:
    explicit Sim(std::uint64_t seed);

    // Advances the simulation exactly one tick.
    void Tick();

    // The battle's single PRNG stream — callers (doctrine interpreter)
    // draw through this; never construct a second stream.
    Prng& Rng() { return rng_; }

    std::uint64_t TickCount() const { return tickCount_; }

    // Deterministic fingerprint of sim state. Identical seeds produce
    // identical checksums across compilers and platforms.
    std::uint64_t Checksum() const { return checksum_; }

private:
    std::uint64_t tickCount_ = 0;
    std::uint64_t checksum_ = 0;
    Prng rng_;
};

} // namespace Potato::Gameplay
