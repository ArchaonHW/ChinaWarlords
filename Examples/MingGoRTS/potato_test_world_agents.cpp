// Epic 17 — campaign-persisted world agent identity and LOD projection.
#include "Campaign/State/CampaignState.h"
#include "Campaign/World/WorldMap.h"
#include "Campaign/World/WorldState.h"
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Sim/SimulationLod.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* name) {
    if (condition) {
        std::printf("[PASS] %s\n", name);
    } else {
        std::printf("[FAIL] %s\n", name);
        ++failures;
    }
}

} // namespace

int main() {
    using namespace Potato;
    using Campaign::WorldAgentState;

    const auto worldDoc = Gameplay::JsonValue::Parse(
        R"({"schema":"potato.world/1","id":"world","nodes":[{"id":"alpha"},{"id":"beta"}],"start":"alpha"})");
    Check(worldDoc.ok(), "world fixture parses");
    if (!worldDoc.ok()) return 1;

    const auto map = Campaign::WorldMap::FromJson(worldDoc.value);
    Check(map.ok(), "world fixture validates");
    if (!map.ok()) return 1;

    auto state = Campaign::WorldState::Init(map.value, 13);
    Check(state.ok(), "world state initializes");
    if (!state.ok()) return 1;

    const WorldAgentState remote{
        std::numeric_limits<std::uint64_t>::max(), "beta", {}};
    const WorldAgentState nearby{
        7, "alpha", {1000, 1000, 1000, 1000, 1000, 1000}};
    const WorldAgentState input[] = {remote, nearby};
    const auto set = state.value.SetAgents(map.value, input);
    Check(set.ok() && state.value.Agents().size() == 2 &&
              state.value.Agents()[0].id == 7 &&
              state.value.Agents()[1].id ==
                  std::numeric_limits<std::uint64_t>::max(),
          "world agents validate and canonicalize by stable ID");

    Gameplay::SimulationLodPolicy policy;
    Gameplay::SimulationBudget budget;
    const auto plan = state.value.PlanAgentLod(policy, budget);
    Check(plan.ok() && plan.value.size() == 2 &&
              plan.value[0].agentId == 7 &&
              plan.value[0].lod == Gameplay::SimulationLod::Full &&
              plan.value[1].agentId ==
                  std::numeric_limits<std::uint64_t>::max() &&
              plan.value[1].lod == Gameplay::SimulationLod::Dormant,
          "public campaign factors project into deterministic LOD");

    Campaign::CampaignState campaign;
    campaign.SetWorld(state.value);
    const auto save = campaign.ToJson();
    Check(save.ok(), "campaign with world agents serializes");
    if (save.ok()) {
        const auto restored = Campaign::CampaignState::FromJson(save.value);
        Check(restored.ok() &&
                  restored.value.GetWorld().Agents() == state.value.Agents(),
              "stable world agents survive CampaignState save round-trip");
        if (restored.ok()) {
            const auto replayed = restored.value.GetWorld().PlanAgentLod(
                policy, budget);
            Check(replayed.ok() && replayed.value == plan.value,
                  "LOD plan reproduces after campaign reload");
        }
    }

    const auto before = state.value.Agents();
    const WorldAgentState duplicate[] = {nearby, nearby};
    Check(!state.value.SetAgents(map.value, duplicate).ok() &&
              state.value.Agents() == before,
          "duplicate IDs reject without replacing current agents");
    const WorldAgentState unknownNode[] = {{8, "unknown", {}}};
    Check(!state.value.SetAgents(map.value, unknownNode).ok() &&
              state.value.Agents() == before,
          "unknown public node rejects without replacing current agents");
    const WorldAgentState invalidFactor[] = {
        {8, "alpha", {1001, 0, 0, 0, 0, 0}}};
    Check(!state.value.SetAgents(map.value, invalidFactor).ok() &&
              state.value.Agents() == before,
          "out-of-range factor rejects without replacing current agents");

    const auto malformed = Gameplay::JsonValue::Parse(
        R"({"schema":"potato.worldstate/1","world":"world","day":0,"warband":"alpha","agents":[{"id":8,"public_node":"alpha","lod":{"distance":1001,"combat":0,"narrative":0,"strategic":0,"player_interest":0,"mythic_importance":0}}]})");
    Check(malformed.ok() && !Campaign::WorldState::FromJson(malformed.value).ok(),
          "malformed persisted LOD factor rejects");

    auto lifecycle = Campaign::WorldState::Init(map.value, 29);
    Check(lifecycle.ok(), "LOD lifecycle world initializes");
    if (!lifecycle.ok()) return 1;

    const WorldAgentState cohortA{11, "alpha", {50, 50, 50, 50, 50, 50}, 77};
    const WorldAgentState cohortB{12, "alpha", {100, 100, 100, 100, 100, 100}, 77};
    const WorldAgentState full{13, "alpha", {1000, 1000, 1000, 1000, 1000, 1000}};
    const WorldAgentState dormant{14, "beta", {}};
    const WorldAgentState lifecycleAgents[] = {cohortA, cohortB, full, dormant};
    const auto lifecycleSet = lifecycle.value.SetAgents(map.value, lifecycleAgents);
    const auto lifecyclePlan = lifecycle.value.PlanAgentLod(policy, budget);
    Check(lifecycleSet.ok() && lifecyclePlan.ok() &&
              lifecyclePlan.value.size() == 4 &&
              lifecyclePlan.value[0].lod == Gameplay::SimulationLod::Aggregate &&
              lifecyclePlan.value[1].lod == Gameplay::SimulationLod::Aggregate &&
              lifecyclePlan.value[2].lod == Gameplay::SimulationLod::Full &&
              lifecyclePlan.value[3].lod == Gameplay::SimulationLod::Dormant &&
              lifecycle.value.ApplyAgentLodPlan(lifecyclePlan.value).ok() &&
              lifecycle.value.AgentLodHistory().size() == 3,
          "complete LOD plan records deterministic initial transitions");
    if (lifecyclePlan.ok()) {
        const auto aggregates = lifecycle.value.AgentLodAggregates();
        Check(aggregates.size() == 1 &&
                  aggregates[0].publicNode == "alpha" &&
                  aggregates[0].aggregateGroupId == 77 &&
                  aggregates[0].agentIds == std::vector<std::uint64_t>{11, 12} &&
                  aggregates[0].factorTotals ==
                      std::array<std::uint64_t, 6>{150, 150, 150, 150, 150, 150},
              "AGGREGATE summarizes stable public cohort membership and factors");

        const auto historyBeforeBadPlan = lifecycle.value.AgentLodHistory();
        const auto statesBeforeBadPlan = lifecycle.value.AgentLodStates();
        const Gameplay::SimulationLodAssignment partial[] = {
            lifecyclePlan.value[0], lifecyclePlan.value[1],
            lifecyclePlan.value[2]};
        Check(!lifecycle.value.ApplyAgentLodPlan(partial).ok() &&
                  lifecycle.value.AgentLodStates() == statesBeforeBadPlan &&
                  lifecycle.value.AgentLodHistory() == historyBeforeBadPlan,
              "partial LOD plan rejects without changing current state/history");

        auto invalidTier = lifecyclePlan.value;
        invalidTier[0].lod = static_cast<Gameplay::SimulationLod>(255);
        Check(!lifecycle.value.ApplyAgentLodPlan(invalidTier).ok() &&
                  lifecycle.value.AgentLodStates() == statesBeforeBadPlan &&
                  lifecycle.value.AgentLodHistory() == historyBeforeBadPlan,
              "unknown LOD tier rejects without changing current state/history");

        Check(lifecycle.value.ApplyAgentLodPlan(lifecyclePlan.value).ok() &&
                  lifecycle.value.AgentLodHistory() == historyBeforeBadPlan,
              "reapplying the same complete plan is idempotent");
    }

    WorldAgentState reactivatedAgents[] = {cohortA, cohortB, full,
        WorldAgentState{14, "beta", {1000, 1000, 1000, 1000, 1000, 1000}}};
    Check(lifecycle.value.SetAgents(map.value, reactivatedAgents).ok(),
          "public factors can update while dormant identity is retained");
    const auto reactivatedPlan = lifecycle.value.PlanAgentLod(policy, budget);
    Check(reactivatedPlan.ok() &&
              reactivatedPlan.value[3].lod == Gameplay::SimulationLod::Full &&
              lifecycle.value.ApplyAgentLodPlan(reactivatedPlan.value).ok() &&
              lifecycle.value.AgentLodHistory().back().agentId == 14 &&
              lifecycle.value.AgentLodHistory().back().from ==
                  Gameplay::SimulationLod::Dormant &&
              lifecycle.value.AgentLodHistory().back().to ==
                  Gameplay::SimulationLod::Full,
          "DORMANT agent reactivates from its retained public record");

    WorldAgentState fadedAgents[] = {
        WorldAgentState{11, "alpha", {}, 77}, cohortB, full,
        WorldAgentState{14, "beta", {1000, 1000, 1000, 1000, 1000, 1000}}};
    Check(lifecycle.value.SetAgents(map.value, fadedAgents).ok(),
          "public factors can lower an active agent's detail");
    const auto fadedPlan = lifecycle.value.PlanAgentLod(policy, budget);
    Check(fadedPlan.ok() &&
              fadedPlan.value[0].lod == Gameplay::SimulationLod::Dormant &&
              lifecycle.value.ApplyAgentLodPlan(fadedPlan.value).ok() &&
              lifecycle.value.AgentLodHistory().back().agentId == 11 &&
              lifecycle.value.AgentLodHistory().back().from ==
                  Gameplay::SimulationLod::Aggregate &&
              lifecycle.value.AgentLodHistory().back().to ==
                  Gameplay::SimulationLod::Dormant &&
              lifecycle.value.AgentLodAggregates().size() == 1 &&
              lifecycle.value.AgentLodAggregates()[0].agentIds ==
                  std::vector<std::uint64_t>{12},
          "AGGREGATE membership is removed on deterministic dormancy transition");

    Campaign::CampaignState lifecycleCampaign;
    lifecycleCampaign.SetWorld(lifecycle.value);
    const auto lifecycleSave = lifecycleCampaign.ToJson();
    if (lifecycleSave.ok()) {
        const auto restored = Campaign::CampaignState::FromJson(lifecycleSave.value);
        Check(restored.ok() &&
                  restored.value.GetWorld().Agents() == lifecycle.value.Agents() &&
                  restored.value.GetWorld().AgentLodStates() ==
                      lifecycle.value.AgentLodStates() &&
                  restored.value.GetWorld().AgentLodHistory() ==
                      lifecycle.value.AgentLodHistory() &&
                  restored.value.GetWorld().AgentLodAggregates() ==
                      lifecycle.value.AgentLodAggregates(),
              "aggregate groups and LOD lifecycle survive campaign save round-trip");
        if (restored.ok() && fadedPlan.ok()) {
            auto resumed = restored.value.GetWorld();
            const auto resumedPlan = resumed.PlanAgentLod(policy, budget);
            const std::size_t priorEvents = resumed.AgentLodHistory().size();
            Check(resumedPlan.ok() && resumedPlan.value == fadedPlan.value &&
                      resumed.ApplyAgentLodPlan(resumedPlan.value).ok() &&
                      resumed.AgentLodHistory().size() == priorEvents,
                  "same LOD plan reproduces after lifecycle save reload");
        }
    } else {
        Check(false, "aggregate groups and LOD lifecycle survive campaign save round-trip");
    }

    const auto badRuntime = Gameplay::JsonValue::Parse(
        R"({"schema":"potato.worldstate/1","world":"world","day":0,"warband":"alpha","agents":[{"id":1,"public_node":"alpha","lod":{"distance":0,"combat":0,"narrative":0,"strategic":0,"player_interest":0,"mythic_importance":0}}],"lod_runtime":{"current":[{"id":1,"lod":"UNKNOWN"}],"events":[]}})");
    Check(badRuntime.ok() &&
              !Campaign::WorldState::FromJson(badRuntime.value).ok(),
          "unknown persisted LOD tier rejects the complete state document");
    const auto legacyAgents = Gameplay::JsonValue::Parse(
        R"({"schema":"potato.worldstate/1","world":"world","day":0,"warband":"alpha","agents":[{"id":1,"public_node":"alpha","lod":{"distance":500,"combat":0,"narrative":0,"strategic":0,"player_interest":0,"mythic_importance":0}}]})");
    bool legacyLodDefaultsDormant = false;
    if (legacyAgents.ok()) {
        const auto restoredLegacy =
            Campaign::WorldState::FromJson(legacyAgents.value);
        legacyLodDefaultsDormant = restoredLegacy.ok() &&
            restoredLegacy.value.AgentLodStates().size() == 1 &&
            restoredLegacy.value.AgentLodStates()[0].lod ==
                Gameplay::SimulationLod::Dormant;
    }
    Check(legacyLodDefaultsDormant,
          "legacy world saves initialize agent LOD runtime as dormant");

    return failures == 0 ? 0 : 1;
}
