#include "Campaign/Narrative/Bencao.h"

#include "Campaign/Chapters/ChapterLibrary.h"
#include "Campaign/Ledger/Ledger.h"
#include "Campaign/Myth/MythActions.h"
#include "Gameplay/Json/Json.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

const char* kCategories[] = {"shancao",   "xicao",     "ducao",
                             "manshui",   "gucai",     "jinshi",
                             "chongshou", "renbu"};
static_assert(std::size(kCategories) == kBencaoCategoryCount,
              "category table out of sync");

const char* kUnlockKinds[] = {"terrain",      "ledger_tag",
                              "governance",   "myth_state",
                              "corruption",   "chapter_close"};

bool InBounds(const std::string& s, std::size_t max) {
    return !s.empty() && s.size() <= max;
}

// The closed vocabularies a trigger param can actually resolve
// against — a manifest naming anything else loads into a dead
// unlock, which is worse than a loud rejection (review D2).
bool IsTerrainFlag(std::string_view s) {
    static constexpr std::string_view kFlags[] = {
        "water", "river", "road", "forest",
        "highland", "chokepoint", "open",
    };
    for (std::string_view f : kFlags) {
        if (s == f) return true;
    }
    return false;
}

// Myth-state ids: the catalog's player verbs (MythActionDefs)
// plus the two non-purchased entries — "invasion" (the god's own
// move, folded by LogMythEvents) and "infiltrated" (MythState
// presence).
bool IsMythStateId(std::string_view s) {
    if (s == "infiltrated" || s == "invasion") return true;
    for (const MythActionDef& d : MythActionDefs()) {
        if (s == d.id) return true;
    }
    return false;
}

// The 批註 field is runtime-bound (10.3) — an authored annotation
// key rejects the file, English AND the CJK spellings an author
// would naturally reach for. Other unknown members stay
// forward-compatible.
bool HasRuntimeBoundKey(const JsonValue& doc) {
    return doc.Has("annotation") || doc.Has("annotations") ||
           doc.Has("marginalia") || doc.Has("批註") ||
           doc.Has("批注");
}

// One file -> BencaoEntry or a reason string. Pure validation —
// unknown members ignored except the runtime-bound guard.
const char* ValidateEntry(const JsonValue& doc, BencaoEntry& out) {
    if (!doc.IsObject()) return "root is not an object";
    if (HasRuntimeBoundKey(doc)) {
        return "annotation slots are runtime-bound";
    }
    const std::string* id = doc.FindString("id");
    const std::string* category = doc.FindString("category");
    const std::string* name = doc.FindString("name");
    const std::string* nature = doc.FindString("nature");
    const std::string* indications = doc.FindString("indications");
    const std::string* source = doc.FindString("source");
    if (id == nullptr || category == nullptr || name == nullptr ||
        nature == nullptr || indications == nullptr ||
        source == nullptr) {
        return "missing required field";
    }
    if (!InBounds(*id, BencaoLibrary::MAX_ID_LEN)) {
        return "id out of range";
    }
    BencaoCategory cat;
    if (!BencaoCategoryFromName(*category, cat)) {
        return "unknown category";
    }
    if (!InBounds(*name, BencaoLibrary::MAX_NAME_LEN)) {
        return "name out of range";
    }
    if (!InBounds(*nature, BencaoLibrary::MAX_TEXT_LEN) ||
        !InBounds(*indications, BencaoLibrary::MAX_TEXT_LEN)) {
        return "nature/indications out of range";
    }
    if (!InBounds(*source, BencaoLibrary::MAX_SOURCE_LEN)) {
        return "source out of range";
    }
    // Optional 釋名/集解 — bounds-checked when present.
    std::vector<std::string> aliases;
    if (doc.Has("aliases")) {
        const JsonValue& a = doc["aliases"];
        if (!a.IsArray() ||
            a.Size() > BencaoLibrary::MAX_ALIASES) {
            return "aliases out of range";
        }
        for (const JsonValue& v : a.Items()) {
            if (!v.IsString() ||
                !InBounds(v.AsString(), BencaoLibrary::MAX_NAME_LEN)) {
                return "bad alias";
            }
            aliases.push_back(v.AsString());
        }
    }
    std::string origin;
    if (doc.Has("origin")) {
        if (!doc["origin"].IsString()) return "bad origin";
        origin = doc["origin"].AsString();
        if (origin.size() > BencaoLibrary::MAX_TEXT_LEN) {
            return "origin out of range";
        }
    }
    // lang block: optional; zh-tw is inherent — asserting it false
    // is a contradiction. `en` is tolerated but ignored (OQ-B3).
    if (doc.Has("lang")) {
        const JsonValue& l = doc["lang"];
        if (!l.IsObject()) return "bad lang";
        const JsonValue& zh = l["zh-tw"];
        if (zh.IsBool() && !zh.AsBool()) {
            return "lang cannot deny zh-tw";
        }
    }
    // Unlock manifest — kind required, params per kind.
    const JsonValue& unlock = doc["unlock"];
    if (!unlock.IsObject()) return "missing unlock";
    const std::string* kindS = unlock.FindString("kind");
    UnlockKind kind;
    if (kindS == nullptr || !UnlockKindFromName(*kindS, kind)) {
        return "unknown unlock kind";
    }
    std::string param;
    std::int64_t pint = 0;
    const char* paramKey = nullptr;
    switch (kind) {
        case UnlockKind::Terrain: paramKey = "terrain"; break;
        case UnlockKind::LedgerTag: paramKey = "tag"; break;
        case UnlockKind::Governance: paramKey = "event"; break;
        case UnlockKind::MythState: paramKey = "state"; break;
        case UnlockKind::Corruption:
            if (!unlock["at_least"].IsInt()) {
                return "corruption needs at_least";
            }
            pint = unlock["at_least"].AsInt();
            // at_least 0 degenerates to "any non-null ledger" —
            // the trigger must name a real threshold. 1023 is the
            // story's bound (covers MAX_CHAPTERS with headroom).
            if (pint < 1 || pint > 1023) {
                return "at_least out of range";
            }
            break;
        case UnlockKind::ChapterClose:
            if (!unlock["chapter"].IsInt()) {
                return "chapter_close needs chapter";
            }
            pint = unlock["chapter"].AsInt();
            // -1 = every chapter close; otherwise a real index.
            if (pint < -1 ||
                pint >= static_cast<std::int64_t>(
                    ChapterLibrary::MAX_CHAPTERS)) {
                return "chapter out of range";
            }
            break;
    }
    if (paramKey != nullptr) {
        const std::string* p = unlock.FindString(paramKey);
        if (p == nullptr) return "unlock param missing";
        param = *p;
        switch (kind) {
        case UnlockKind::Terrain:
            if (!InBounds(param, BencaoLibrary::MAX_UNLOCK_PARAM_LEN) ||
                !IsTerrainFlag(param)) {
                return "unlock.terrain is not a TERRAIN_* flag id";
            }
            break;
        case UnlockKind::LedgerTag:
            // A tag longer than MAX_TAG_LEN can never be posted.
            if (!InBounds(param, Ledger::MAX_TAG_LEN)) {
                return "unlock.tag out of range";
            }
            break;
        case UnlockKind::Governance:
            // The event id must fit a `resolution:<event>` seal
            // tag — longer ids resolve through the bare-tag path
            // only.
            if (!InBounds(param, Ledger::MAX_TAG_LEN -
                                     Ledger::TAG_RESOLUTION.size())) {
                return "unlock.event out of range";
            }
            break;
        case UnlockKind::MythState:
            if (!InBounds(param, BencaoLibrary::MAX_UNLOCK_PARAM_LEN) ||
                !IsMythStateId(param)) {
                return "unlock.state is not a myth action/state id";
            }
            break;
        default:
            break;
        }
    }
    out.id = *id;
    out.category = cat;
    out.name = *name;
    out.aliases = std::move(aliases);
    out.origin = std::move(origin);
    out.nature = *nature;
    out.indications = *indications;
    out.unlockKind = kind;
    out.unlockParam = std::move(param);
    out.unlockInt = pint;
    out.source = *source;
    return nullptr;
}

} // namespace

const char* BencaoCategoryName(BencaoCategory c) {
    const auto i = static_cast<std::size_t>(c);
    return i < kBencaoCategoryCount ? kCategories[i] : "shancao";
}

bool BencaoCategoryFromName(std::string_view name,
                            BencaoCategory& out) {
    for (std::size_t i = 0; i < kBencaoCategoryCount; ++i) {
        if (name == kCategories[i]) {
            out = static_cast<BencaoCategory>(i);
            return true;
        }
    }
    return false;
}

const char* BencaoCategoryTitle(BencaoCategory c) {
    switch (c) {
    case BencaoCategory::Shancao:   return "山草類";
    case BencaoCategory::Xicao:     return "隰草類";
    case BencaoCategory::Ducao:     return "毒草類";
    case BencaoCategory::Manshui:   return "蔓水類";
    case BencaoCategory::Gucai:     return "穀菜類";
    case BencaoCategory::Jinshi:    return "金石類";
    case BencaoCategory::Chongshou: return "蟲獸類";
    case BencaoCategory::Renbu:     return "人部拾遺";
    }
    return "山草類";
}

const char* UnlockKindName(UnlockKind k) {
    const auto i = static_cast<std::size_t>(k);
    return i < 6 ? kUnlockKinds[i] : "ledger_tag";
}

bool UnlockKindFromName(std::string_view name, UnlockKind& out) {
    for (std::size_t i = 0; i < 6; ++i) {
        if (name == kUnlockKinds[i]) {
            out = static_cast<UnlockKind>(i);
            return true;
        }
    }
    return false;
}

BencaoLoadResult BencaoLibrary::Load(const std::filesystem::path& dir,
                                     BencaoLibrary& out) {
    BencaoLoadResult res;

    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(
             dir, std::filesystem::directory_options::none, ec);
         !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        // Per-entry stat uses its OWN error channel (ChapterLibrary
        // precedent — shared `ec` gets cleared by increment()).
        std::error_code ec2;
        const bool isFile = it->is_regular_file(ec2);
        if (ec2) {
            res.rejected.push_back(
                {it->path(), "io",
                 "stat failed: " + ec2.message()});
            continue;
        }
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        if (isFile && ext == ".json") {
            files.push_back(it->path());
        }
    }
    if (ec) {
        res.error = "io";
        res.reason = "bencao dir unreadable: " + ec.message();
        return res;
    }
    std::sort(files.begin(), files.end());

    std::vector<BencaoEntry> entries;
    std::set<std::string> ids;
    for (const std::filesystem::path& f : files) {
        const std::u8string u8 = f.u8string();
        Result<JsonValue> doc = Gameplay::Json::Load(
            std::string_view(
                reinterpret_cast<const char*>(u8.data()),
                u8.size()),
            SCHEMA);
        if (!doc.ok()) {
            res.rejected.push_back({f, doc.error, doc.reason});
            continue;
        }
        BencaoEntry e;
        if (const char* why = ValidateEntry(doc.value, e)) {
            res.rejected.push_back({f, "field", why});
            continue;
        }
        // Capacity and dup checks BEFORE commit — a rejected
        // file must not consume its id.
        if (entries.size() >= MAX_ENTRIES) {
            res.rejected.push_back({f, "field", "library full"});
            continue;
        }
        if (ids.count(e.id)) {
            res.rejected.push_back(
                {f, "duplicate", "entry id already registered"});
            continue;
        }
        ids.insert(e.id);
        entries.push_back(std::move(e));
    }

    // Canonical order: category ordinal, then id.
    std::sort(entries.begin(), entries.end(),
              [](const BencaoEntry& a, const BencaoEntry& b) {
                  if (a.category != b.category) {
                      return static_cast<int>(a.category) <
                             static_cast<int>(b.category);
                  }
                  return a.id < b.id;
              });
    out.entries_ = std::move(entries);
    res.ok = true;
    return res;
}

const BencaoEntry* BencaoLibrary::Find(std::string_view id) const {
    for (const BencaoEntry& e : entries_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

} // namespace Potato::Campaign
