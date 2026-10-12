#pragma once

#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// Story 10.1 — the desk's second book. The Bencao codex is a
// collection-layer narrative system: bounded subset of 8
// categories, one potato.bencao/1 file per entry, loaded at boot
// into a read-only registry. Entries carry their own unlock
// manifest in-file — evaluation is Story 10.2's job; this type
// only parses and holds. Codex content never reaches the sim:
// it lives in Campaign by construction.

enum class BencaoCategory : std::uint8_t {
    Shancao,   // 山草類   "shancao"   — highland terrain
    Xicao,     // 隰草類   "xicao"     — lowland/marsh/field
    Ducao,     // 毒草類   "ducao"     — corruption events
    Manshui,   // 蔓草/水草 "manshui"  — rivers/ferries/convoys
    Gucai,     // 穀菜類   "gucai"     — villages/supply/famine
    Jinshi,    // 金石類   "jinshi"    — shrine layer, forgery
    Chongshou, // 蟲獸類   "chongshou" — myth encounters, wonders
    Renbu,     // 人部拾遺 "renbu"     — darkest class, late beats
};
constexpr std::size_t kBencaoCategoryCount = 8;
// Wire id ("shancao", …) — round-trips with *FromName.
const char* BencaoCategoryName(BencaoCategory c);
// CJK 部類 display name (山草類, …) — the book's section titles.
const char* BencaoCategoryTitle(BencaoCategory c);
bool BencaoCategoryFromName(std::string_view name,
                            BencaoCategory& out);

// Unlock trigger families (worldview §4) — manifest parses and
// validates here; nothing reads campaign/ledger state yet.
enum class UnlockKind : std::uint8_t {
    Terrain,      // {"kind":"terrain","terrain":"<flag id>"}
    LedgerTag,    // {"kind":"ledger_tag","tag":"<tag>"}
    Governance,   // {"kind":"governance","event":"<id>"}
    MythState,    // {"kind":"myth_state","state":"<id>"}
    Corruption,   // {"kind":"corruption","at_least":<int>}
    ChapterClose, // {"kind":"chapter_close","chapter":<int|-1 any>}
};
const char* UnlockKindName(UnlockKind k);
bool UnlockKindFromName(std::string_view name, UnlockKind& out);

// One herb entry — the six-field anatomy (worldview §3). 正名/
// 類/性味歸經/主治 required; 釋名/集解 optional; 批註 is
// runtime-bound (10.3) and never authored — an authored
// annotation key rejects the file.
struct BencaoEntry {
    std::string id;
    BencaoCategory category = BencaoCategory::Shancao;
    std::string name;                 // 正名 — required
    std::vector<std::string> aliases; // 釋名 — optional
    std::string origin;               // 集解 — optional
    std::string nature;               // 性味歸經 — required
    std::string indications;          // 主治 — required
    UnlockKind unlockKind = UnlockKind::LedgerTag;
    std::string unlockParam;          // tag/terrain/event/state id
    std::int64_t unlockInt = 0;       // corruption.at_least / chapter
    std::string source;               // 卷/條 citation — required
};

struct BencaoRejected {
    std::filesystem::path path;
    std::string error;  // "schema" / "field" / "duplicate" / "io"
    std::string reason;
};

struct BencaoLoadResult {
    bool ok = false;
    std::string error;
    std::string reason;
    std::vector<BencaoRejected> rejected; // per-file failures
};

// Boot-time registry — ChapterLibrary precedent: sorted-filename
// iteration, per-file isolation, immutable after Load.
class BencaoLibrary {
public:
    static constexpr std::string_view SCHEMA = "potato.bencao/1";
    static constexpr std::size_t MAX_ENTRIES = 1024;
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::size_t MAX_NAME_LEN = 64;
    static constexpr std::size_t MAX_TEXT_LEN = 2048; // origin/nature/indications
    static constexpr std::size_t MAX_ALIASES = 16;
    static constexpr std::size_t MAX_SOURCE_LEN = 128;
    static constexpr std::size_t MAX_UNLOCK_PARAM_LEN = 128;

    // Unreadable dir -> ok=false,error="io". Empty readable dir ->
    // empty ok. Sorted .json files only; bad file -> rejected[],
    // library continues.
    static BencaoLoadResult Load(const std::filesystem::path& dir,
                                 BencaoLibrary& out);

    const BencaoEntry* Find(std::string_view id) const;
    std::size_t Size() const { return entries_.size(); }
    // Canonical order: category ordinal, then id — the 10.4
    // renderer's stable layout.
    const std::vector<BencaoEntry>& Entries() const {
        return entries_;
    }

private:
    std::vector<BencaoEntry> entries_;
};

} // namespace Potato::Campaign
