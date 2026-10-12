#pragma once

#include "Campaign/Narrative/Conventions.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// Story 6.5 — NarrativePack: versioned narrative bundles
// (potato.narrative/1) live in assets/narrative/ and register
// read-only at boot. One file = one pack = one ChapterConventions
// document; the library merges all accepted packs into a single
// conventions view (chapter ids are one global namespace —
// duplicates across packs reject the LATER file, sorted-filename
// order, same per-file isolation as BencaoLibrary).
//
// Pack identity: an optional "pack" string field in the doc,
// else the filename stem. Pack ids are informational (load
// diagnostics, future per-pack lookup); chapter lookup reads the
// merged view either way.
struct NarrativePackInfo {
    std::string id;
    std::size_t chapters = 0;
};

struct NarrativeRejected {
    std::filesystem::path path;
    std::string error; // "schema" / "parse" / "duplicate" / "io" / "field"
    std::string reason;
};

struct NarrativeLoadResult {
    bool ok = false;
    std::string error;
    std::string reason;
    std::vector<NarrativeRejected> rejected;
};

class NarrativeLibrary {
public:
    static constexpr std::string_view SCHEMA =
        ChapterConventions::SCHEMA; // potato.narrative/1
    static constexpr std::size_t MAX_PACKS = 64;
    static constexpr std::size_t MAX_PACK_ID_LEN = 64;

    // Unreadable dir -> ok=false,error="io". Empty readable dir ->
    // empty ok. Sorted .json files only; file count over MAX_PACKS
    // fails wholesale ("overflow") — the CharacterLibrary
    // precedent, bounding the rejected-vector DoS surface.
    static NarrativeLoadResult Load(const std::filesystem::path& dir,
                                    NarrativeLibrary& out);

    // All accepted packs' frames, merged — the object the 6.2
    // renderers (RenderChapterOpen/RenderChapterClose) consume.
    const ChapterConventions& Conventions() const { return merged_; }
    const ChapterFrame* Find(std::string_view chapterId) const {
        return merged_.Find(chapterId);
    }
    const std::vector<NarrativePackInfo>& Packs() const {
        return packs_;
    }
    std::size_t Size() const { return merged_.Size(); }

private:
    ChapterConventions merged_;
    std::vector<NarrativePackInfo> packs_;
};

} // namespace Potato::Campaign
