#include "Game/Ui/EncounterLaunch.h"

#include "Campaign/Characters/Character.h"
#include "Campaign/World/WorldMap.h"

namespace Potato::Game {

Gameplay::Result<Campaign::EncounterAssembly>
PrepareEncounterLaunch(
    const Campaign::EncounterDef& encounter,
    const Campaign::WorldMap& world,
    const BattleMapCatalog& battleMaps,
    const Gameplay::SquadTemplateLibrary& squads,
    const Gameplay::DoctrineLibrary& cards,
    std::span<const Campaign::PlayerDeploy> players,
    const Campaign::Character* commander,
    std::uint64_t seed) {
    const Campaign::WorldNode* node =
        world.FindNode(encounter.node);
    if (node == nullptr) {
        return Gameplay::Fail<Campaign::EncounterAssembly>(
            "encounter", "遭遇節點不在目前世界：" + encounter.node);
    }

    const std::string& mapId =
        encounter.map.empty() ? node->map : encounter.map;
    if (mapId.empty()) {
        return Gameplay::Fail<Campaign::EncounterAssembly>(
            "map", "遭遇未指定戰場地圖");
    }
    const auto mapIt = battleMaps.find(mapId);
    if (mapIt == battleMaps.end()) {
        return Gameplay::Fail<Campaign::EncounterAssembly>(
            "map", "找不到戰場地圖「" + mapId + "」");
    }

    auto assembly = Campaign::MarshalEncounter(
        encounter, world, mapIt->second, squads, cards, players,
        commander, seed);
    if (!assembly.ok()) {
        const std::string detail = assembly.reason.empty()
            ? "遭遇編隊驗證失敗"
            : "編隊失敗：" + assembly.reason;
        return Gameplay::Fail<Campaign::EncounterAssembly>(
            assembly.error.empty() ? "marshal" : assembly.error,
            detail);
    }
    return assembly;
}

} // namespace Potato::Game
