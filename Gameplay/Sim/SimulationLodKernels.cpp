#include "Gameplay/Sim/SimulationLodKernels.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Potato::Gameplay {

namespace {

constexpr std::uint16_t MEASURE_MAX = 1000;
constexpr std::int32_t DELTA_MAX = 1000;

bool ValidDelta(std::int32_t delta) noexcept {
    return delta >= -DELTA_MAX && delta <= DELTA_MAX;
}

bool ValidStep(std::uint64_t currentTicks,
               std::uint64_t elapsedTicks) noexcept {
    return elapsedTicks != 0 &&
           elapsedTicks <= std::numeric_limits<std::uint64_t>::max() -
                               currentTicks;
}

std::uint16_t ApplyDelta(std::uint16_t value,
                         std::int32_t delta) noexcept {
    const std::int32_t changed = static_cast<std::int32_t>(value) + delta;
    return static_cast<std::uint16_t>(
        std::clamp(changed, std::int32_t{0},
                   static_cast<std::int32_t>(MEASURE_MAX)));
}

bool ValidTotal(std::uint64_t total, std::uint32_t population) noexcept {
    return total <= static_cast<std::uint64_t>(population) * MEASURE_MAX;
}

// Computes floor(total * removed / population) without overflowing uint64_t.
// The largest possible remainder product is (2^32 - 1)^2, which fits.
std::uint64_t ProportionalRemoval(std::uint64_t total,
                                  std::uint32_t removed,
                                  std::uint32_t population) noexcept {
    if (removed == 0 || population == 0) return 0;
    const std::uint64_t quotient = total / population;
    const std::uint64_t remainder = total % population;
    return quotient * removed + (remainder * removed) / population;
}

std::uint64_t ApplyDeltaToTotal(std::uint64_t total,
                                std::uint32_t population,
                                std::int32_t delta) noexcept {
    const std::int64_t changed = static_cast<std::int64_t>(total) +
        static_cast<std::int64_t>(population) * delta;
    const std::uint64_t maximum =
        static_cast<std::uint64_t>(population) * MEASURE_MAX;
    if (changed <= 0) return 0;
    return std::min(static_cast<std::uint64_t>(changed), maximum);
}

} // namespace

bool AdvanceReduced(const ReducedAgentState& current,
                    const SimulationLodStep& step,
                    ReducedAgentState& next) noexcept {
    if (current.strength > MEASURE_MAX || current.morale > MEASURE_MAX ||
        current.supply > MEASURE_MAX ||
        !ValidStep(current.elapsedTicks, step.elapsedTicks) ||
        !ValidDelta(step.strengthDelta) || !ValidDelta(step.moraleDelta) ||
        !ValidDelta(step.supplyDelta)) {
        return false;
    }

    const ReducedAgentState result{
        ApplyDelta(current.strength, step.strengthDelta),
        ApplyDelta(current.morale, step.moraleDelta),
        ApplyDelta(current.supply, step.supplyDelta),
        current.elapsedTicks + step.elapsedTicks};
    next = result;
    return true;
}

bool AdvanceAbstract(const AbstractCohortState& current,
                     const AbstractLodStep& step,
                     AbstractCohortState& next) noexcept {
    if (!ValidTotal(current.strengthTotal, current.population) ||
        !ValidTotal(current.moraleTotal, current.population) ||
        !ValidTotal(current.supplyTotal, current.population) ||
        step.losses > current.population ||
        !ValidStep(current.elapsedTicks, step.elapsedTicks) ||
        !ValidDelta(step.strengthDeltaPerSurvivor) ||
        !ValidDelta(step.moraleDeltaPerSurvivor) ||
        !ValidDelta(step.supplyDeltaPerSurvivor)) {
        return false;
    }

    const std::uint32_t survivors = current.population - step.losses;
    const AbstractCohortState result{
        survivors,
        ApplyDeltaToTotal(
            current.strengthTotal - ProportionalRemoval(
                current.strengthTotal, step.losses, current.population),
            survivors, step.strengthDeltaPerSurvivor),
        ApplyDeltaToTotal(
            current.moraleTotal - ProportionalRemoval(
                current.moraleTotal, step.losses, current.population),
            survivors, step.moraleDeltaPerSurvivor),
        ApplyDeltaToTotal(
            current.supplyTotal - ProportionalRemoval(
                current.supplyTotal, step.losses, current.population),
            survivors, step.supplyDeltaPerSurvivor),
        current.elapsedTicks + step.elapsedTicks};
    next = result;
    return true;
}

bool ProjectToAbstract(const SimulationRosterSnapshot& current,
                       AbstractSimulationSnapshot& next) {
    if (current.agents.size() >
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    std::vector<SimulationAgentSnapshot> agents = current.agents;
    std::sort(agents.begin(), agents.end(),
              [](const SimulationAgentSnapshot& left,
                 const SimulationAgentSnapshot& right) {
                  return left.agentId < right.agentId;
              });

    AbstractSimulationSnapshot result;
    result.agentIds.reserve(agents.size());
    result.cohort.population = static_cast<std::uint32_t>(agents.size());
    result.cohort.elapsedTicks = current.elapsedTicks;
    for (std::size_t i = 0; i < agents.size(); ++i) {
        const SimulationAgentSnapshot& agent = agents[i];
        if (agent.strength > MEASURE_MAX || agent.morale > MEASURE_MAX ||
            agent.supply > MEASURE_MAX ||
            (i != 0 && agents[i - 1].agentId == agent.agentId)) {
            return false;
        }
        result.agentIds.push_back(agent.agentId);
        result.cohort.strengthTotal += agent.strength;
        result.cohort.moraleTotal += agent.morale;
        result.cohort.supplyTotal += agent.supply;
    }
    next = std::move(result);
    return true;
}

bool RehydrateFromAbstract(const AbstractSimulationSnapshot& current,
                           SimulationRosterSnapshot& next) {
    const AbstractCohortState& cohort = current.cohort;
    const std::uint32_t population = cohort.population;
    if (current.agentIds.size() != population ||
        !ValidTotal(cohort.strengthTotal, population) ||
        !ValidTotal(cohort.moraleTotal, population) ||
        !ValidTotal(cohort.supplyTotal, population)) {
        return false;
    }
    for (std::size_t i = 1; i < current.agentIds.size(); ++i) {
        if (current.agentIds[i - 1] >= current.agentIds[i]) return false;
    }

    SimulationRosterSnapshot result;
    result.elapsedTicks = cohort.elapsedTicks;
    result.agents.reserve(population);

    const auto distribute = [population, ticks = cohort.elapsedTicks](
                                std::uint64_t total, std::size_t index) {
        if (population == 0) return std::uint16_t{0};
        const std::uint64_t quotient = total / population;
        const std::uint64_t remainder = total % population;
        const std::uint64_t first = ticks % population;
        const std::uint64_t rotated =
            (static_cast<std::uint64_t>(index) + population - first) %
            population;
        return static_cast<std::uint16_t>(
            quotient + (rotated < remainder ? 1 : 0));
    };
    for (std::size_t i = 0; i < current.agentIds.size(); ++i) {
        result.agents.push_back(SimulationAgentSnapshot{
            current.agentIds[i],
            distribute(cohort.strengthTotal, i),
            distribute(cohort.moraleTotal, i),
            distribute(cohort.supplyTotal, i)});
    }
    next = std::move(result);
    return true;
}

} // namespace Potato::Gameplay
