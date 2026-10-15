#pragma once

#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Potato::Gameplay {

// Detail level selected for a world entity. These are policy labels;
// per-level update kernels are a later implementation step.
enum class SimulationLod : std::uint8_t {
    Full,
    Reduced,
    Abstract,
    Aggregate,
    Dormant
};

// Importance inputs use a fixed 0..1000 scale. The distance value is
// normalized proximity/interest: 1000 means near or otherwise relevant,
// while 0 means remote. It is not a raw world-unit distance.
struct SimulationLodFactors {
    std::uint16_t distance = 0;
    std::uint16_t combat = 0;
    std::uint16_t narrative = 0;
    std::uint16_t strategic = 0;
    std::uint16_t playerInterest = 0;
    std::uint16_t mythicImportance = 0;

    friend bool operator==(const SimulationLodFactors&,
                           const SimulationLodFactors&) = default;
};

// Nonnegative integer weights keep scoring deterministic and avoid
// floating-point differences across platforms.
struct SimulationLodWeights {
    std::uint32_t distance = 1;
    std::uint32_t combat = 1;
    std::uint32_t narrative = 1;
    std::uint32_t strategic = 1;
    std::uint32_t playerInterest = 1;
    std::uint32_t mythicImportance = 1;
};

struct SimulationLodThresholds {
    std::int64_t full = 4000;
    std::int64_t reduced = 2500;
    std::int64_t abstract = 1250;
    std::int64_t aggregate = 250;
};

// Maximum entities eligible for each detail level. Defaults retain the
// target scale from the project spec: 100 FULL and 100,000 ABSTRACT.
struct SimulationBudget {
    std::size_t maxFull = 100;
    std::size_t maxReduced = 1000;
    std::size_t maxAbstract = 100000;
    std::size_t maxAggregate = 10000;
};

struct SimulationLodSubject {
    std::uint64_t agentId = 0;
    SimulationLodFactors factors;
};

struct SimulationLodAssignment {
    std::uint64_t agentId = 0;
    std::int64_t importance = 0;
    SimulationLod lod = SimulationLod::Dormant;

    friend bool operator==(const SimulationLodAssignment&,
                           const SimulationLodAssignment&) = default;
};

// Pure deterministic policy. It does not read or mutate simulation state,
// and it does not run inside the 20 Hz battle tick.
class SimulationLodPolicy {
public:
    static constexpr std::uint32_t MAX_WEIGHT = 1000000;
    static constexpr std::uint16_t MAX_FACTOR = 1000;

    SimulationLodWeights weights;
    SimulationLodThresholds thresholds;

    // Invalid config, out-of-range factors, or duplicate IDs return an
    // error without producing a partial assignment.
    Result<std::vector<SimulationLodAssignment>> Plan(
        std::span<const SimulationLodSubject> subjects,
        const SimulationBudget& budget) const;

    static const char* Name(SimulationLod lod);
};

} // namespace Potato::Gameplay
