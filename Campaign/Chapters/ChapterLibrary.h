#pragma once

#include "Campaign/World/WorldMap.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// Ledger axis for bind.ledger predicates — the FoldGovernance
// vocabulary (campaign-scope totals; per-region folds are 12.7's).
enum class LedgerAxis : std::uint8_t {
    PopularSupport,
    Order,
    Corruption,
};

// Mandatory-beat names (narrative-design: commission / midpoint
// pivot / final confrontation) — a beat implies mandatory.
enum class ChapterBeat : std::uint8_t {
    None,
    Commission,
    Pivot,
    Final,
};

// Optional world-binding (12.6): gates a chapter's availability
// on world-state predicates and/or prerequisite chapters instead
// of the linear index sequence. Absent bind = legacy unbound
// chapter (predecessor rule). Presence of any field is optional;
// an empty bind latches as soon as its (vacuous) gates hold.
struct ChapterBind {
    // `node` alone is an arrival gate (warband must stand on it);
    // `node` + `control` is a held-region gate — `node` becomes
    // control's argument and arrival is not required.
    std::string node;
    WorldControl control = WorldControl::Neutral;
    bool hasControl = false;             // requires `node`
    std::vector<std::string> resolved;   // WorldState resolved-ids
    std::vector<std::string> prereqs;    // wire "requires":
                                         // chapter-id prereqs
    bool hasLedger = false;
    LedgerAxis axis = LedgerAxis::PopularSupport;
    std::int64_t atLeast = 0;
    bool mandatory = false;              // spine membership
    ChapterBeat beat = ChapterBeat::None;
    bool IsMandatory() const {
        return mandatory || beat != ChapterBeat::None;
    }
};

// One potato.chapter/1 pack — the spine; Epic 4/6 grow it via
// schema bumps. `index` is the persisted chapter coordinate
// (saves store chapter.current as an index into this space), so
// it is explicit content, not a filename artifact. Indexes may be
// SPARSE ({0,5} is legal); the chapter space is
// [0, MaxIndex()+1) — see below.
struct ChapterDef {
    std::string id;           // unique across the library
    std::int64_t index = 0;   // campaign ordering
    std::string title;
    std::string map;          // content ref (assets/maps/...)
    bool combat = true;       // false = designated zero-combat (E)
    std::string briefing;
    bool bound = false;       // bind block present (12.6)
    ChapterBind bind;
};

struct RejectedChapter {
    std::filesystem::path path;
    std::string error;
    std::string reason;
};

struct ChapterLoadResult {
    bool ok = false;
    std::string error;
    std::string reason;
    std::vector<RejectedChapter> rejected; // per-file failures
};

// Boot-time registry: chapters are content, not code. Built once
// by Load(), immutable thereafter (NFR9). Per-file isolation: a
// bad chapter file is rejected and logged, never fails the
// library; Load fails wholesale only if the directory itself is
// unreadable.
class ChapterLibrary {
public:
    static constexpr std::string_view SCHEMA = "potato.chapter/1";
    static constexpr std::size_t MAX_CHAPTERS = 64;   // matches
                                                    // CampaignState
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::size_t MAX_TITLE_LEN = 128;
    static constexpr std::size_t MAX_REF_LEN = 256;
    static constexpr std::size_t MAX_BRIEFING_LEN = 4096;

    // Sorted-filename iteration, per-file rejection into
    // result.rejected. Directory unreadable -> !ok.
    static ChapterLoadResult Load(
        const std::filesystem::path& dir, ChapterLibrary& out);

    const ChapterDef* Find(std::string_view id) const;
    const ChapterDef* AtIndex(std::int64_t index) const;
    // Chapter space bound: max index + 1 (0 when empty). Sparse
    // indexes mean MaxIndex()+1 can exceed Size() — progression
    // vectors (CampaignState::ChapterProgress) must be sized to
    // MaxIndex()+1 and indexed BY ChapterDef.index, not by
    // position in Chapters().
    std::int64_t MaxIndex() const {
        return chapters_.empty() ? -1 : chapters_.back().index;
    }
    std::size_t Size() const { return chapters_.size(); }
    const std::vector<ChapterDef>& Chapters() const {
        return chapters_;
    }

private:
    // Ordered by `index` after Load.
    std::vector<ChapterDef> chapters_;
};

} // namespace Potato::Campaign
