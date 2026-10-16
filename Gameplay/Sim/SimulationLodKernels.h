#pragma once

#include <cstdint>
#include <vector>

namespace Potato::Gameplay {

// LOD macro updates use the same 0..1000 scale as SimulationLodFactors.
// Deltas describe the complete interval, whose duration is elapsedTicks;
// they are not per-20-Hz-tick rates.
struct SimulationLodStep {
    std::uint64_t elapsedTicks = 0;
    std::int32_t strengthDelta = 0;
    std::int32_t moraleDelta = 0;
    std::int32_t supplyDelta = 0;
};

// REDUCED keeps one representative agent's three bounded integer measures.
struct ReducedAgentState {
    std::uint16_t strength = 0;
    std::uint16_t morale = 0;
    std::uint16_t supply = 0;
    std::uint64_t elapsedTicks = 0;

    friend bool operator==(const ReducedAgentState&,
                           const ReducedAgentState&) = default;
};

// Applies one interval's aggregate deltas. Returns false for an invalid state,
// an out-of-range delta, a zero-length interval, or tick counter overflow.
// On failure, `next` is left unchanged.
bool AdvanceReduced(const ReducedAgentState& current,
                    const SimulationLodStep& step,
                    ReducedAgentState& next) noexcept;

// ABSTRACT stores cohort population and sums of each member's bounded
// measures. Totals therefore remain exact integers even when members differ.
struct AbstractCohortState {
    std::uint32_t population = 0;
    std::uint64_t strengthTotal = 0;
    std::uint64_t moraleTotal = 0;
    std::uint64_t supplyTotal = 0;
    std::uint64_t elapsedTicks = 0;

    friend bool operator==(const AbstractCohortState&,
                           const AbstractCohortState&) = default;
};

struct AbstractLodStep {
    std::uint64_t elapsedTicks = 0;
    std::uint32_t losses = 0;
    std::int32_t strengthDeltaPerSurvivor = 0;
    std::int32_t moraleDeltaPerSurvivor = 0;
    std::int32_t supplyDeltaPerSurvivor = 0;
};

// Removes losses proportionally from aggregate totals (integer division,
// remainder retained by survivors), then applies per-survivor deltas and
// clamps totals to [0, population * 1000]. On failure, `next` is unchanged.
bool AdvanceAbstract(const AbstractCohortState& current,
                     const AbstractLodStep& step,
                     AbstractCohortState& next) noexcept;

// Canonical per-agent snapshot for the bounded strength/morale/supply
// measures used by the macro kernels. These are simulation state, not
// campaign visibility or LOD importance factors.
struct SimulationAgentSnapshot {
    std::uint64_t agentId = 0;
    std::uint16_t strength = 0;
    std::uint16_t morale = 0;
    std::uint16_t supply = 0;

    friend bool operator==(const SimulationAgentSnapshot&,
                           const SimulationAgentSnapshot&) = default;
};

// A member-level roster and its shared simulation clock. Input member order
// is unspecified; projection canonicalizes by stable agent ID.
struct SimulationRosterSnapshot {
    std::uint64_t elapsedTicks = 0;
    std::vector<SimulationAgentSnapshot> agents;

    friend bool operator==(const SimulationRosterSnapshot&,
                           const SimulationRosterSnapshot&) = default;
};

// ABSTRACT retains exact cohort totals and stable member identities, but not
// each member's original measure values.
struct AbstractSimulationSnapshot {
    std::vector<std::uint64_t> agentIds; // strictly ascending
    AbstractCohortState cohort;

    friend bool operator==(const AbstractSimulationSnapshot&,
                           const AbstractSimulationSnapshot&) = default;
};

// Projects a member roster into exact integer totals. Duplicate IDs,
// out-of-range measures, or an unrepresentable population reject the whole
// operation and leave `next` unchanged. This allocation/sort path is for LOD
// transitions, not the fixed-rate simulation tick.
bool ProjectToAbstract(const SimulationRosterSnapshot& current,
                       AbstractSimulationSnapshot& next);

// Rehydrates member records in ascending ID order. Each total is divided by
// population; any remainder rotates from agentIds[elapsedTicks % population]
// through stable-ID order. Cohort totals and elapsedTicks are preserved
// exactly, while individual values are a deterministic reconstruction rather
// than their pre-aggregation values.
// Invalid membership or totals reject without changing `next`.
bool RehydrateFromAbstract(const AbstractSimulationSnapshot& current,
                           SimulationRosterSnapshot& next);

} // namespace Potato::Gameplay
