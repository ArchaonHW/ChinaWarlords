#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Myth/Infiltration.h" // GodStance
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

class Ledger;

// Story 6.2 — the 章回 frame around every chapter. Each chapter
// opens on a court 敕命 (frontispiece — imperative, formulaic,
// the institution's voice) and closes with a 評斷 (the historian's
// verdict on the enemy/ourselves) plus an 懸念 cliffhanger
// (且聽下回分解 — the storyteller's hook).
//
// Content lives in potato.narrative/1 documents; the *loader* and
// registry are Story 6.5's — this type parses an already-parsed
// JsonValue document and renders from it.
//
// Frames vary by ledger state — the court cannot hide what the
// books show:
//   - judgment keys on the corruption ratchet (clean/tainted/
//     corrupt): the verdict reads what cruelty was posted.
//   - cliffhanger keys on the mandate balance (blessed/fading/
//     barren): heaven's account decides how hopeful the hook is.
// The frontispiece does NOT vary — the commission is sealed before
// the facts arrive; that distance is the fiction.

// Condition axes, resolved from the ledger.
enum class FrameMood { Clean, Tainted, Corrupt };
enum class FrameOmen { Blessed, Fading, Barren };

// Thresholds — content-facing contract, pinned by tests.
inline constexpr std::int64_t MOOD_TAINTED_AT = 10;
inline constexpr std::int64_t MOOD_CORRUPT_AT = 40;
inline constexpr std::int64_t OMEN_BLESSED_AT = 30;
inline constexpr std::int64_t OMEN_FADING_AT = 5;

FrameMood LedgerMood(const Ledger& l); // from the corruption ratchet
FrameOmen LedgerOmen(const Ledger& l); // from the 天命 balance
std::string_view MoodKey(FrameMood m); // "clean"/"tainted"/"corrupt"
std::string_view OmenKey(FrameOmen o); // "blessed"/"fading"/"barren"

// Story 6.7's third axis — the deity's observed mood.
std::string_view StanceKey(Gameplay::GodStance s);
// "favorable"/"neutral"/"wrathful"

// Story 6.7 — document-variant pools. `variants.<doc>.<slot>.
// <axis>.<key>` maps to a pool of modular clauses (fragments the
// caller composes, never whole documents). Axis names are a
// strict vocabulary ("mood"/"omen"/"stance") — an unknown axis
// rejects the pack. Key lookup falls back requested →
// "default" → first map entry, the 6.2 Variant() convention
// lifted to pools.
using ClausePool = std::vector<std::string>;
struct VariantSlot {
    std::map<std::string, ClausePool, std::less<>> mood;
    std::map<std::string, ClausePool, std::less<>> omen;
    std::map<std::string, ClausePool, std::less<>> stance;
};
struct DocVariants {
    std::map<std::string, VariantSlot, std::less<>> slots;
};

// One chapter's frame. `frontispiece` is the 敕命 body; `judgment`
// and `cliffhanger` are condition-keyed variant maps (lookup falls
// back requested-key → "default" → first entry).
struct ChapterFrame {
    std::string frontispiece;
    // std::less<> gives transparent string_view lookup — same as
    // JsonValue::Object.
    std::map<std::string, std::string, std::less<>> judgment;
    std::map<std::string, std::string, std::less<>> cliffhanger;
};

class ChapterConventions {
public:
    static constexpr std::string_view SCHEMA = "potato.narrative/1";
    static constexpr std::size_t MAX_CHAPTERS = 64; // chapter space
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::size_t MAX_TEXT_LEN = 4096;
    static constexpr std::size_t MAX_VARIANTS = 16; // per frame map
    static constexpr std::size_t MAX_DOCS = 16; // variant doc names
    static constexpr std::size_t MAX_CLAUSES = 8; // per pool

    // Parse a potato.narrative/1 document body. Bad schema or bad
    // shape fails without mutating `out` — per-file rejection is
    // the loader's job (6.5), not the schema's.
    static Gameplay::Result<ChapterConventions> FromJson(
        const Gameplay::JsonValue& doc);
    Gameplay::JsonValue ToJson() const;

    const ChapterFrame* Find(std::string_view chapterId) const;
    const DocVariants* Variants(std::string_view doc) const;
    std::size_t Size() const { return frames_.size(); }

private:
    std::map<std::string, ChapterFrame, std::less<>> frames_;
    std::map<std::string, DocVariants, std::less<>> variants_;
};

// The commission page — reign-title header + sealed mandate body.
// Missing chapter renders a generic commission (the court always
// has an order to give).
std::string RenderChapterOpen(const ChapterConventions& conv,
                              std::string_view chapterId);

// The closing frame — 評曰 judgment picked by ledger mood +
// cliffhanger picked by ledger omen, closed by the storyteller's
// 且聽下回分解.
std::string RenderChapterClose(const ChapterConventions& conv,
                               std::string_view chapterId,
                               const Ledger& ledger);

// Story 6.7 — document-variant rendering. For each axis present
// in the slot, in fixed order mood → omen → stance: resolve the
// pool by state key (fallback → "default" → first), pick one
// clause deterministically — FNV-1a(salt LE ‖ doc ‖ slot ‖
// axis) % pool size, no PRNG, no clock. One clause per line;
// "" when the doc, slot, or all its pools are absent.
// Composed by the caller beside the document's own render —
// the 10.5 removability precedent: no renderer signature
// changes, the layer lifts out whole.
std::string RenderDocVariant(const ChapterConventions& conv,
                             std::string_view doc,
                             std::string_view slot,
                             const Ledger& ledger,
                             Gameplay::GodStance stance,
                             std::uint64_t salt = 0);

} // namespace Potato::Campaign
