#include "Gameplay/Sim/SimulationLod.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Potato::Gameplay {

namespace {

struct Candidate {
    std::uint64_t id = 0;
    std::int64_t importance = 0;
    SimulationLod requested = SimulationLod::Dormant;
};

bool ValidFactors(const SimulationLodFactors& f) {
    return f.distance <= SimulationLodPolicy::MAX_FACTOR &&
           f.combat <= SimulationLodPolicy::MAX_FACTOR &&
           f.narrative <= SimulationLodPolicy::MAX_FACTOR &&
           f.strategic <= SimulationLodPolicy::MAX_FACTOR &&
           f.playerInterest <= SimulationLodPolicy::MAX_FACTOR &&
           f.mythicImportance <= SimulationLodPolicy::MAX_FACTOR;
}

bool ValidWeights(const SimulationLodWeights& w) {
    return w.distance <= SimulationLodPolicy::MAX_WEIGHT &&
           w.combat <= SimulationLodPolicy::MAX_WEIGHT &&
           w.narrative <= SimulationLodPolicy::MAX_WEIGHT &&
           w.strategic <= SimulationLodPolicy::MAX_WEIGHT &&
           w.playerInterest <= SimulationLodPolicy::MAX_WEIGHT &&
           w.mythicImportance <= SimulationLodPolicy::MAX_WEIGHT;
}

bool ValidThresholds(const SimulationLodThresholds& t) {
    return t.full >= t.reduced && t.reduced >= t.abstract &&
           t.abstract >= t.aggregate && t.aggregate >= 0;
}

SimulationLod RequestedLevel(std::int64_t importance,
                             const SimulationLodThresholds& thresholds) {
    if (importance >= thresholds.full) return SimulationLod::Full;
    if (importance >= thresholds.reduced) return SimulationLod::Reduced;
    if (importance >= thresholds.abstract) return SimulationLod::Abstract;
    if (importance >= thresholds.aggregate) return SimulationLod::Aggregate;
    return SimulationLod::Dormant;
}

std::size_t TierIndex(SimulationLod lod) {
    return static_cast<std::size_t>(lod);
}

std::size_t Capacity(const SimulationBudget& budget, std::size_t tier) {
    switch (tier) {
    case 0: return budget.maxFull;
    case 1: return budget.maxReduced;
    case 2: return budget.maxAbstract;
    case 3: return budget.maxAggregate;
    default: return std::numeric_limits<std::size_t>::max();
    }
}

} // namespace

const char* SimulationLodPolicy::Name(SimulationLod lod) {
    switch (lod) {
    case SimulationLod::Full: return "FULL";
    case SimulationLod::Reduced: return "REDUCED";
    case SimulationLod::Abstract: return "ABSTRACT";
    case SimulationLod::Aggregate: return "AGGREGATE";
    case SimulationLod::Dormant: return "DORMANT";
    }
    return "DORMANT";
}

Result<std::vector<SimulationLodAssignment>> SimulationLodPolicy::Plan(
    std::span<const SimulationLodSubject> subjects,
    const SimulationBudget& budget) const {
    if (!ValidWeights(weights)) {
        return Fail<std::vector<SimulationLodAssignment>>(
            "validation", "simulation LOD weights must be between 0 and MAX_WEIGHT");
    }
    if (!ValidThresholds(thresholds)) {
        return Fail<std::vector<SimulationLodAssignment>>(
            "validation", "simulation LOD thresholds must be nonnegative and descending");
    }

    std::vector<Candidate> candidates;
    candidates.reserve(subjects.size());
    for (const SimulationLodSubject& subject : subjects) {
        if (!ValidFactors(subject.factors)) {
            return Fail<std::vector<SimulationLodAssignment>>(
                "validation", "simulation LOD factors must be between 0 and 1000");
        }

        const SimulationLodFactors& f = subject.factors;
        const std::int64_t importance =
            static_cast<std::int64_t>(weights.distance) * f.distance +
            static_cast<std::int64_t>(weights.combat) * f.combat +
            static_cast<std::int64_t>(weights.narrative) * f.narrative +
            static_cast<std::int64_t>(weights.strategic) * f.strategic +
            static_cast<std::int64_t>(weights.playerInterest) * f.playerInterest +
            static_cast<std::int64_t>(weights.mythicImportance) *
                f.mythicImportance;
        candidates.push_back(Candidate{
            subject.agentId, importance,
            RequestedLevel(importance, thresholds)});
    }

    // Canonical input order makes both duplicate detection and output
    // independent of the caller's container order.
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.id < b.id;
              });
    for (std::size_t i = 1; i < candidates.size(); ++i) {
        if (candidates[i - 1].id == candidates[i].id) {
            return Fail<std::vector<SimulationLodAssignment>>(
                "validation", "simulation LOD subject IDs must be unique");
        }
    }

    std::vector<SimulationLod> assigned(
        candidates.size(), SimulationLod::Dormant);
    std::vector<std::size_t> overflow;
    overflow.reserve(candidates.size());
    const auto importanceOrder = [&](std::size_t a, std::size_t b) {
        if (candidates[a].importance != candidates[b].importance) {
            return candidates[a].importance > candidates[b].importance;
        }
        return candidates[a].id < candidates[b].id;
    };

    // Each detail tier receives candidates that requested that tier plus
    // overflow from the preceding tier. Higher importance wins; ID breaks
    // ties. Remaining candidates deterministically degrade one tier.
    for (std::size_t tier = 0; tier < 4; ++tier) {
        std::vector<std::size_t> eligible = std::move(overflow);
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            if (TierIndex(candidates[i].requested) == tier) {
                eligible.push_back(i);
            }
        }
        std::sort(eligible.begin(), eligible.end(), importanceOrder);

        const std::size_t accepted =
            std::min(eligible.size(), Capacity(budget, tier));
        for (std::size_t i = 0; i < accepted; ++i) {
            assigned[eligible[i]] = static_cast<SimulationLod>(tier);
        }
        overflow.assign(eligible.begin() + accepted, eligible.end());
    }

    std::vector<SimulationLodAssignment> result;
    result.reserve(candidates.size());
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        result.push_back(SimulationLodAssignment{
            candidates[i].id, candidates[i].importance, assigned[i]});
    }
    return Ok(std::move(result));
}

} // namespace Potato::Gameplay
