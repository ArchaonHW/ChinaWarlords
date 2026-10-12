#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Potato::Campaign {
class WorldMap;
class WorldState;
struct MarchRules;
} // namespace Potato::Campaign

namespace Potato::Game {

// OrderMenu — the movement-order input surface (Story 12.9).
// A pure view-model: it lists the march orders the sim would
// accept right now, so the shell (ImGui list, keys, clicks)
// renders the data and commits through Campaign::IssueMarch —
// the menu is a view, not a second rulebook.

struct MarchOption {
    std::string nodeId;      // destination world node id
    std::string name;        // display name (may carry CJK)
    std::int64_t days = 0;   // route cost
    std::int64_t supply = 0; // days * rules.supplyPerDay
    bool affordable = false; // supplyAvailable >= supply
};

// Legal targets at the warband's current node: adjacent routes
// only, in the map's canonical neighbor order. Empty while a
// march is in flight (IssueMarch semantics: one order at a time)
// or with no warband. `supplyAvailable` is the caller's read of
// Ledger::Balance(Account::Materiel) — the menu never touches
// the ledger itself.
std::vector<MarchOption> MarchOptions(
    const Campaign::WorldMap& map, const Campaign::WorldState& ws,
    std::int64_t supplyAvailable,
    const Campaign::MarchRules& rules);

// Same, with the canonical MarchRules{} — keeps this header free
// of March.h (MarchRules is only forward-declared above).
std::vector<MarchOption> MarchOptions(
    const Campaign::WorldMap& map, const Campaign::WorldState& ws,
    std::int64_t supplyAvailable);

} // namespace Potato::Game
