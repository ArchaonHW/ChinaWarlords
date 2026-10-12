#pragma once

#include "Campaign/World/WorldMap.h"
#include "Gameplay/Doctrine/Doctrine.h" // SquadSheet (by value)
#include "Gameplay/Fog/QuantumFog.h"    // FogConfig
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay {
class BattleController;
class BattleMap;
class DoctrineLibrary;
struct SquadTemplate;
class SquadTemplateLibrary;
} // namespace Potato::Gameplay

namespace Potato::Campaign {

class WorldState;
struct Character;

// Encounter content (Story 12.5, FR23): potato.encounter/1 — an
// armed confrontation bound to a world region. Marshals into the
// three-beat BattleController; the aftermath writes back through
// EncounterSettle. Encounters are the dynamic-encounter vehicle —
// the anchored 回目 bind via 12.6, not here.
//
// Wire shape:
//   {"schema":"potato.encounter/1","id":"longmen_ambush",
//    "node":"longmen","trigger":"arrival","map":"ch01_plain",
//    "defenders":[{"template":"militia","region":1,
//                  "deck":["card_hold","card_brace","card_flee"]}],
//    "stakes":{"control":"player"},"seed":0}
enum class EncounterTrigger : std::uint8_t {
    Arrival = 0,   // warband stands on the node
    Proximity = 1, // warband at or adjacent to the node
    Poi = 2,       // warband at node AND node carries
                   // strategic/myth flag bits
};

struct EncounterDefender {
    std::string tmpl;                 // potato.squad/1 template id
    std::int64_t region = 0;          // battle-map region index
    std::vector<std::string> deck;    // 3..5 doctrine card ids
};

struct EncounterDef {
    std::string id;            // unique; also the resolved-marker key
    std::string node;          // bound world region id
    EncounterTrigger trigger = EncounterTrigger::Arrival;
    std::string map;           // optional battle-map ref — falls
                               // back to the node's `map` field
    std::vector<EncounterDefender> defenders;
    bool hasControlStake = false;    // stakes.control present?
    WorldControl controlStake = WorldControl::Neutral;
    bool hasSeed = false;            // authored seed overrides the
    std::uint64_t seed = 0;          // caller-supplied seed
};

struct RejectedEncounter {
    std::filesystem::path path;
    std::string error;
    std::string reason;
};

struct EncounterLoadResult {
    bool ok = false;
    std::string error;
    std::string reason;
    std::vector<RejectedEncounter> rejected;
};

// Boot-time registry (WorldLibrary precedent): sorted dir scan,
// per-file isolation, canonical order by encounter id.
class EncounterLibrary {
public:
    static constexpr std::string_view SCHEMA = "potato.encounter/1";
    static constexpr std::size_t MAX_ENCOUNTERS = 512;
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::size_t MAX_MAP_REF_LEN = 256;
    static constexpr std::size_t MAX_DEFENDERS = 16;

    static EncounterLoadResult Load(const std::filesystem::path& dir,
                                    EncounterLibrary& out);

    const EncounterDef* Find(std::string_view id) const;
    std::size_t Size() const { return encounters_.size(); }
    const std::vector<EncounterDef>& Encounters() const {
        return encounters_;
    }

private:
    std::vector<EncounterDef> encounters_;
};

// Which encounters fire right now — deterministic scan in library
// (id-sorted) order. Pure read: skips resolved encounters and
// encounters on vanished nodes.
std::vector<const EncounterDef*> PendingEncounters(
    const WorldState& ws, const WorldMap& map,
    const EncounterLibrary& lib);

// Caller-supplied warband side of the deployment. rosterName keys
// the casualty report back to CampaignState's roster.
struct PlayerDeploy {
    const Gameplay::SquadTemplate* tmpl;
    std::size_t region = 0;
    std::vector<std::string> deck;
    std::string rosterName;
};

struct DeployRow {
    int side = 0;                           // 0 player, 1 defender
    const Gameplay::SquadTemplate* tmpl = nullptr;
    std::size_t region = 0;
    Gameplay::SquadSheet sheet;
    std::string rosterName;                 // side 0 only
};

// Everything the shell needs to stand up the battle: borrowed
// pointers are library-owned — the assembly is valid only while
// `lib`/squads/cards live (same contract as BattleController's
// borrowed map/cards refs).
struct EncounterAssembly {
    const EncounterDef* enc = nullptr;
    std::string mapId;               // resolved battle-map ref
    Gameplay::FogConfig fog;         // commander prior applied
    std::uint64_t seed = 0;
    std::vector<DeployRow> rows;     // side 0 (caller order) then
                                     // side 1 (doc order)
};

// Resolve the encounter doc into deployable rows: node existence
// (world), map ref (enc.map else node.map), template/deck/region
// resolution (libs + battleMap bounds). `commander` (may be null)
// applies its prior bias to the fog config; `seed` is overridden
// by the doc's authored seed when present. All checks run before
// any output row is produced.
Gameplay::Result<EncounterAssembly> MarshalEncounter(
    const EncounterDef& enc, const WorldMap& world,
    const Gameplay::BattleMap& battleMap,
    const Gameplay::SquadTemplateLibrary& squads,
    const Gameplay::DoctrineLibrary& cards,
    std::span<const PlayerDeploy> players,
    const Character* commander, std::uint64_t seed);

// Apply every row via DeploySquad + SetSheet — must run during
// the Planning beat; marshal prevalidated everything, so a false
// return is a caller bug.
bool DeployAssembly(Gameplay::BattleController& bc,
                    const EncounterAssembly& a);

} // namespace Potato::Campaign
