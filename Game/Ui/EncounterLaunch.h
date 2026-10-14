#pragma once

#include "Campaign/World/Encounter.h"
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Result.h"

#include <cstdint>
#include <map>
#include <span>
#include <string>

namespace Potato::Campaign {
struct Character;
class WorldMap;
} // namespace Potato::Campaign

namespace Potato::Game {

using BattleMapCatalog = std::map<std::string, Gameplay::BattleMap>;

// Resolve all encounter content and deployment requirements before
// the shell changes its active battle or screen. Failures are returned
// as content-boundary results so the caller can show the reason.
Gameplay::Result<Campaign::EncounterAssembly> PrepareEncounterLaunch(
    const Campaign::EncounterDef& encounter,
    const Campaign::WorldMap& world,
    const BattleMapCatalog& battleMaps,
    const Gameplay::SquadTemplateLibrary& squads,
    const Gameplay::DoctrineLibrary& cards,
    std::span<const Campaign::PlayerDeploy> players,
    const Campaign::Character* commander,
    std::uint64_t seed);

} // namespace Potato::Game
