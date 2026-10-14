#include "Gameplay/Sim/SimulationLod.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* name) {
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
}

using namespace Potato::Gameplay;

SimulationLodFactors Uniform(std::uint16_t value) {
    return SimulationLodFactors{
        value, value, value, value, value, value};
}

std::size_t Count(const std::vector<SimulationLodAssignment>& assignments,
                  SimulationLod lod) {
    return static_cast<std::size_t>(std::count_if(
        assignments.begin(), assignments.end(),
        [lod](const SimulationLodAssignment& a) { return a.lod == lod; }));
}

} // namespace

int main() {
    SimulationLodPolicy policy;

    // The weighted formula respects all six factors, not distance alone.
    {
        std::vector<SimulationLodSubject> subjects{
            {1, Uniform(1000)},
            {2, Uniform(500)},
            {3, Uniform(250)},
            {4, Uniform(50)},
            {5, Uniform(0)},
            {6, {0, 0, 1000, 1000, 1000, 0}}};
        auto plan = policy.Plan(subjects, SimulationBudget{});
        Check(plan.ok(), "valid factor set produces a plan");
        if (plan.ok()) {
            Check(plan.value[0].lod == SimulationLod::Full,
                  "high importance requests FULL");
            Check(plan.value[1].lod == SimulationLod::Reduced,
                  "mid importance requests REDUCED");
            Check(plan.value[2].lod == SimulationLod::Abstract,
                  "lower importance requests ABSTRACT");
            Check(plan.value[3].lod == SimulationLod::Aggregate,
                  "low nonzero importance requests AGGREGATE");
            Check(plan.value[4].lod == SimulationLod::Dormant,
                  "zero importance requests DORMANT");
            Check(plan.value[5].lod == SimulationLod::Reduced,
                  "narrative, strategic, and player interest can raise LOD");
        }
    }

    // Tier capacities deterministically degrade overflow. Equal importance
    // uses ascending agent ID, independent of input order.
    {
        std::vector<SimulationLodSubject> subjects;
        for (std::uint64_t id = 1; id <= 102; ++id) {
            subjects.push_back({id, Uniform(1000)});
        }
        SimulationBudget budget;
        budget.maxFull = 100;
        budget.maxReduced = 1;
        budget.maxAbstract = 1;
        budget.maxAggregate = 0;

        auto plan = policy.Plan(subjects, budget);
        Check(plan.ok(), "capacity-limited plan succeeds");
        if (plan.ok()) {
            Check(Count(plan.value, SimulationLod::Full) == 100,
                  "FULL capacity is enforced");
            Check(plan.value[99].agentId == 100 &&
                      plan.value[99].lod == SimulationLod::Full,
                  "canonical ID tie-break keeps first 100 FULL");
            Check(plan.value[100].agentId == 101 &&
                      plan.value[100].lod == SimulationLod::Reduced,
                  "first overflow degrades to REDUCED");
            Check(plan.value[101].agentId == 102 &&
                      plan.value[101].lod == SimulationLod::Abstract,
                  "second overflow degrades to ABSTRACT");

            std::reverse(subjects.begin(), subjects.end());
            auto reordered = policy.Plan(subjects, budget);
            Check(reordered.ok() && reordered.value == plan.value,
                  "plan is independent of input order");
        }
    }

    // Invalid data is rejected as a whole, including duplicate identities.
    {
        std::vector<SimulationLodSubject> duplicate{
            {7, Uniform(1000)}, {7, Uniform(500)}};
        auto duplicatePlan = policy.Plan(duplicate, SimulationBudget{});
        Check(!duplicatePlan.ok() && duplicatePlan.error == "validation",
              "duplicate agent IDs are rejected");

        std::vector<SimulationLodSubject> outOfRange{
            {8, {1001, 0, 0, 0, 0, 0}}};
        auto factorPlan = policy.Plan(outOfRange, SimulationBudget{});
        Check(!factorPlan.ok(), "out-of-range factor is rejected");

        SimulationLodPolicy invalidWeights;
        invalidWeights.weights.combat = SimulationLodPolicy::MAX_WEIGHT + 1;
        auto weightPlan = invalidWeights.Plan({}, SimulationBudget{});
        Check(!weightPlan.ok() && weightPlan.error == "validation",
              "out-of-range weights are rejected");
        SimulationLodPolicy invalidThresholds;
        invalidThresholds.thresholds.reduced =
            invalidThresholds.thresholds.full + 1;
        auto thresholdPlan =
            invalidThresholds.Plan({}, SimulationBudget{});
        Check(!thresholdPlan.ok(), "unordered thresholds are rejected");
    }

    // Exercise the target mixed workload in the specification: 100 FULL
    // agents and 100,000 ABSTRACT agents in the same plan.
    {
        std::vector<SimulationLodSubject> subjects;
        subjects.reserve(100100);
        for (std::uint64_t id = 0; id < 100; ++id) {
            subjects.push_back({id, Uniform(1000)});
        }
        for (std::uint64_t id = 100; id < 100100; ++id) {
            subjects.push_back({id, Uniform(250)});
        }

        SimulationBudget budget;
        auto plan = policy.Plan(subjects, budget);
        Check(plan.ok(), "100 FULL plus 100,000 ABSTRACT subjects are planned");
        if (plan.ok()) {
            Check(Count(plan.value, SimulationLod::Full) == 100,
                  "mixed workload retains 100 FULL slots");
            Check(Count(plan.value, SimulationLod::Abstract) == 100000,
                  "mixed workload retains 100,000 ABSTRACT slots");
            Check(plan.value.size() == 100100,
                  "every subject has exactly one assignment");
        }
    }

    Check(std::string_view(SimulationLodPolicy::Name(SimulationLod::Dormant)) ==
              "DORMANT",
          "LOD names are stable");

    std::printf("%s\n", failures == 0 ? "SIMULATION LOD PASS"
                                       : "SIMULATION LOD FAIL");
    return failures == 0 ? 0 : 1;
}
