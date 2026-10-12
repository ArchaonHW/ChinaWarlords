#pragma once

#include "Gameplay/Map/BattleMap.h" // MAX_REGIONS — the wire cap
#include "Gameplay/Myth/Infiltration.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace Potato::Gameplay {
class JsonValue;
}

namespace Potato::Campaign {

// Myth infiltration persistence (Story 5.1) — the campaign side of
// the dual-layer state machine.
//
// Region indices are map-local, so the durable identity is
// (chapterId, region): the chapter is the unit the campaign already
// keys progression on, and a battle's map only exists inside its
// chapter. Persisted as a sibling doc `potato.myth/1` (same pattern
// as potato.rivals/1) — schema revisions stay independent of
// potato.campaign.
//
// Wire shape:
//   {"schema":"potato.myth/1",
//    "chapters":{"<chapterId>":{"<region>":<level 0-3>}}}
//
// Canonical form: only levels 1-3 are stored — "quiet" (0) is the
// default and a 0 write erases the entry. The chronicle records what
// the myth layer changed; silence is the unremarkable ground state.
class MythState {
public:
    static constexpr std::string_view SCHEMA = "potato.myth/1";
    static constexpr std::size_t MAX_CHAPTERS = 128;
    // Lockstep with BattleMap::MAX_REGIONS — a battle's MythField can
    // never exceed what this doc persists.
    static constexpr std::size_t MAX_REGIONS_PER_CHAPTER =
        Gameplay::BattleMap::MAX_REGIONS;
    static constexpr std::size_t MAX_ID_LEN = 64;

    // Infiltration level (0-3) for (chapter, region); absent = 0.
    int LevelAt(std::string_view chapterId, int region) const;
    bool HasChapter(std::string_view chapterId) const {
        return byChapter_.find(std::string(chapterId)) !=
               byChapter_.end();
    }
    // Level must be 0..3. Setting 0 erases the entry (canonical
    // form). Returns false on bad input or capacity — never
    // partially mutates.
    bool Set(std::string_view chapterId, int region, int level);
    // Drop a chapter's entire region set (CommitMyth clears before
    // rewriting so stale regions can't linger).
    void ClearChapter(std::string_view chapterId);
    std::size_t ChapterCount() const { return byChapter_.size(); }

    Gameplay::Result<Gameplay::JsonValue> ToJson() const;
    static Gameplay::Result<MythState> FromJson(
        const Gameplay::JsonValue& doc);

private:
    // chapterId -> (region -> level 1..3)
    std::map<std::string, std::map<int, std::uint8_t>> byChapter_;
};

// The battle->campaign merge seam: fold a finished battle's
// MythField into per-chapter persistence. Replaces the chapter's
// region set wholesale — the field is the battle's complete truth,
// and a stale region from a previous map revision must not linger.
// All fallible conditions are preflighted BEFORE the chapter is
// cleared — a rejected commit leaves the stored state untouched.
// Returns the number of nonzero levels written.
Gameplay::Result<int> CommitMyth(MythState& state,
                                 std::string_view chapterId,
                                 const Gameplay::MythField& field);

} // namespace Potato::Campaign
