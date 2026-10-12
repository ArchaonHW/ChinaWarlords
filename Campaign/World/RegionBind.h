#pragma once

#include <string>
#include <string_view>

namespace Potato::Campaign {

struct ChapterDef;
struct WorldNode;
class MythState;

// Region binding (Story 12.7) — the bridge between world-node ids
// and the two systems that already key on "place":
//
//   * Ledger fold tags: `region:<node>` is the reserved world
//     namespace (Ledger::TAG_REGION) that FoldGovernanceByRegion
//     reads; battle-map-local region indices ride `field:<n>`
//     instead so the two never collide.
//   * Myth infiltration: MythState keys place as a string +
//     slot int. A bound chapter infiltrates under its world node
//     id (the durable identity — chapter ids can change across
//     content revisions, ground cannot); an unbound chapter keeps
//     its own id. Shrine-flagged world nodes hold their own myth
//     state in slot 0 — a shrine's sanctity is a world fact, not
//     a chapter fact.

// `region:<node>` — the ledger fold key for a world node. The
// caller owns tag-length validation (Ledger::MAX_TAG_LEN).
std::string RegionTagFor(std::string_view node);

// The place a chapter's myth state keys on: its bound node when
// bound to one, otherwise its own chapter id. Borrows into `def`.
std::string_view MythPlaceKey(const ChapterDef& def);

// Shrine flag on a world node (Gameplay MYTH_SHRINE bit).
bool IsShrinePoi(const WorldNode& n);

// A shrine's sanctity lives in MythState slot 0 under the node id.
// Level 0 (quiet) is the default; absent and explicit-0 agree.
int ShrineLevel(const MythState& s, std::string_view node);
// Level must be 0..3; 0 erases. Returns false on bad input —
// never partially mutates (MythState::Set semantics).
bool SetShrineLevel(MythState& s, std::string_view node, int level);

} // namespace Potato::Campaign
