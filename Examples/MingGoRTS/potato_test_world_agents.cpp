// Epic 17 — campaign-persisted world agent identity and LOD projection.
#include "Campaign/State/CampaignState.h"
#include "Campaign/World/WorldMap.h"
#include "Campaign/World/WorldState.h"
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Sim/SimulationLod.h"

#include <cstdint>
#include <cstdio>
#include <limits>

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

    return failures == 0 ? 0 : 1;
}
