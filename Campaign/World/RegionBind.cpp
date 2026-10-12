#include "Campaign/World/RegionBind.h"

#include "Campaign/Chapters/ChapterLibrary.h" // ChapterDef/bind
#include "Campaign/Ledger/Ledger.h"           // TAG_REGION
#include "Campaign/Myth/MythState.h"
#include "Campaign/World/WorldMap.h" // WorldNode
#include "Gameplay/Map/BattleMap.h"  // MYTH_SHRINE

namespace Potato::Campaign {

// Shrine world-scale level lives in region slot 0 of the node's
// MythState set — a documented convention (story Dev Notes), not a
// schema change. Chapters bound to the same node share the place
// key: the shrine remembers, not the book.
static constexpr int SHRINE_SLOT = 0;

std::string RegionTagFor(std::string_view node) {
    return std::string(Ledger::TAG_REGION) + std::string(node);
}

std::string_view MythPlaceKey(const ChapterDef& def) {
    if (def.bound && !def.bind.node.empty()) {
        return def.bind.node;
    }
    return def.id;
}

bool IsShrinePoi(const WorldNode& n) {
    return (n.myth & Gameplay::MYTH_SHRINE) != 0;
}

int ShrineLevel(const MythState& s, std::string_view node) {
    return s.LevelAt(node, SHRINE_SLOT);
}

bool SetShrineLevel(MythState& s, std::string_view node,
                    int level) {
    return s.Set(node, SHRINE_SLOT, level);
}

} // namespace Potato::Campaign
