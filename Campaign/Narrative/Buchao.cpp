#include "Campaign/Narrative/Buchao.h"

#include <algorithm>

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

// FNV-1a — same hash family as the ledger chain and the record
// integrity root (deterministic clause pick, never PRNG).
std::uint64_t Fnv1a(std::string_view s, std::uint64_t h) {
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

// "%N" → entry name, "%P" → provenance detail (the evidence that
// claimed the page). The ONLY substitution slots — templates may
// cite, never fabricate (efficacy rule).
std::string Subst(std::string_view tpl, const BencaoEntry& e,
                  const PendingPage& p) {
    std::string out;
    for (std::size_t i = 0; i < tpl.size();) {
        if (tpl[i] == '%' && i + 1 < tpl.size()) {
            if (tpl[i + 1] == 'N') {
                out += e.name;
                i += 2;
                continue;
            }
            if (tpl[i + 1] == 'P') {
                out += p.detail;
                i += 2;
                continue;
            }
        }
        out += tpl[i++];
    }
    return out;
}

// 史官體 clause pools — one per UnlockKind. Pick is FNV-1a(entry.id
// ‖ page.detail ‖ chapterIndex LE) % size: the same unlock always
// composes the same note. Content authoring at scale is Story
// 10.6's job.
const std::string_view kTerrainPool[] = {
    "%N，%P之地所產；是役親履其土，士卒識之。",
    "%N，聞出%P中，土人攜以獻。",
    "%N，行軍過%P，隨營採得。",
};
const std::string_view kLedgerTagPool[] = {
    "%N，因「%P」一記入冊，書吏錄之。",
    "%N，冊中有「%P」事，故附此條。",
};
const std::string_view kGovernancePool[] = {
    "%N，%P之治既成，民間獻其方。",
    "%N，%P之後，物產漸復，採訪得此。",
};
const std::string_view kMythStatePool[] = {
    "%N，聞%P之異，土人指以為證。",
    "%N，%P事見於神祠，錄之以誌。",
};
const std::string_view kCorruptionPool[] = {
    "%N，兵燹既深，惡疾隨之，姑錄以戒。",
    "%N，瘡痍之後，民爭識此物。",
};
const std::string_view kChapterClosePool[] = {
    "%N，本回既畢，照例補鈔。",
    "%N，卷帙將合，書吏補錄此條。",
};

struct Pool {
    const std::string_view* items;
    std::size_t size;
};

Pool PoolOf(UnlockKind k) {
    switch (k) {
    case UnlockKind::Terrain:
        return {kTerrainPool, 3};
    case UnlockKind::LedgerTag:
        return {kLedgerTagPool, 2};
    case UnlockKind::Governance:
        return {kGovernancePool, 2};
    case UnlockKind::MythState:
        return {kMythStatePool, 2};
    case UnlockKind::Corruption:
        return {kCorruptionPool, 2};
    case UnlockKind::ChapterClose:
        return {kChapterClosePool, 2};
    }
    return {kChapterClosePool, 2};
}

// Required string field: present, non-empty, within bound.
bool ReadField(const JsonValue& doc, std::string_view key,
               std::size_t maxLen, std::string& out) {
    const std::string* s = doc.FindString(key);
    if (!s || s->empty() || s->size() > maxLen) return false;
    out = *s;
    return true;
}

} // namespace

std::string ComposeMarginalia(const BencaoEntry& entry,
                              const PendingPage& page,
                              std::int64_t chapterIndex) {
    std::uint64_t h = 14695981039346656037ull; // offset basis
    h = Fnv1a(entry.id, h);
    h = Fnv1a(page.detail, h);
    const std::uint64_t ch = static_cast<std::uint64_t>(chapterIndex);
    for (int i = 0; i < 8; ++i) {
        const char b = static_cast<char>((ch >> (i * 8)) & 0xff);
        h = Fnv1a(std::string_view(&b, 1), h);
    }
    // Pool keys on the recorded provenance kind — what actually
    // fired — falling back to the entry's manifest kind when a
    // persisted spelling won't parse.
    UnlockKind kind = entry.unlockKind;
    UnlockKindFromName(page.kind, kind);
    const Pool pool = PoolOf(kind);
    return Subst(pool.items[h % pool.size], entry, page);
}

const BuchaoPage* BuchaoStore::Find(std::string_view entryId) const {
    for (const BuchaoPage& p : pages_) {
        if (p.entry == entryId) return &p;
    }
    return nullptr;
}

bool BuchaoStore::SetSuspect(std::string_view entryId, bool suspect) {
    for (BuchaoPage& p : pages_) {
        if (p.entry == entryId) {
            p.suspect = suspect;
            return true;
        }
    }
    return false;
}

Result<JsonValue> BuchaoStore::ToJson() const {
    JsonValue::Array pages;
    pages.reserve(pages_.size());
    for (const BuchaoPage& p : pages_) {
        JsonValue::Object o;
        o["entry"] = JsonValue::String(p.entry);
        o["note"] = JsonValue::String(p.note);
        o["suspect"] = JsonValue::Bool(p.suspect);
        pages.push_back(JsonValue::MakeObject(std::move(o)));
    }
    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    root["pages"] = JsonValue::MakeArray(std::move(pages));
    return Gameplay::Ok(JsonValue::MakeObject(std::move(root)));
}

Result<BuchaoStore> BuchaoStore::FromJson(const JsonValue& doc) {
    if (!doc.IsObject()) {
        return Gameplay::Fail<BuchaoStore>("schema",
                                           "root is not an object");
    }
    const std::string* schema = doc.FindString("schema");
    if (!schema || *schema != SCHEMA) {
        return Gameplay::Fail<BuchaoStore>(
            "schema", "expected potato.buchao/1");
    }
    if (!doc.Has("pages") || !doc["pages"].IsArray()) {
        return Gameplay::Fail<BuchaoStore>("schema",
                                           "pages: not an array");
    }
    BuchaoStore store;
    for (const JsonValue& v : doc["pages"].Items()) {
        if (!v.IsObject()) {
            return Gameplay::Fail<BuchaoStore>("field",
                                               "page is not an object");
        }
        if (store.pages_.size() >= MAX_PAGES) {
            return Gameplay::Fail<BuchaoStore>("overflow",
                                               "pages exceed MAX_PAGES");
        }
        BuchaoPage p;
        if (!ReadField(v, "entry", MAX_ENTRY_LEN, p.entry) ||
            !ReadField(v, "note", MAX_NOTE_LEN, p.note)) {
            return Gameplay::Fail<BuchaoStore>(
                "field", "page entry/note missing or out of range");
        }
        const JsonValue& s = v["suspect"];
        p.suspect = s.IsBool() && s.AsBool();
        // Dedupe by entry — first wins (the queue delivers once).
        if (!store.Find(p.entry)) {
            store.pages_.push_back(std::move(p));
        }
    }
    return Gameplay::Ok(std::move(store));
}

Result<std::vector<BuchaoPage>>
DeliverBuchao(const BencaoLibrary& lib, BencaoCodex& codex,
              BuchaoStore& store, std::size_t maxPages,
              std::int64_t chapterIndex) {
    // Preflight BEFORE the drain mutates anything: capacity plus
    // every queued id must resolve in the library — a pending id
    // with no entry is corrupt state, not a skippable row. The
    // delivery either lands whole or nothing moves.
    const std::size_t n =
        std::min(maxPages, codex.Pending().size());
    if (store.pages_.size() + n > BuchaoStore::MAX_PAGES) {
        return Gameplay::Fail<std::vector<BuchaoPage>>(
            "overflow", "buchao pages would exceed MAX_PAGES");
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (!lib.Find(codex.Pending()[i].id)) {
            return Gameplay::Fail<std::vector<BuchaoPage>>(
                "field", "pending id missing from library");
        }
    }
    const std::vector<PendingPage> drained = codex.TakePending(n);
    std::vector<BuchaoPage> out;
    out.reserve(drained.size());
    for (const PendingPage& pp : drained) {
        const BencaoEntry* e = lib.Find(pp.id);
        if (store.Find(pp.id)) {
            continue; // already delivered (pre-seeded store)
        }
        BuchaoPage page;
        page.entry = pp.id;
        page.note = ComposeMarginalia(*e, pp, chapterIndex);
        store.pages_.push_back(page);
        out.push_back(std::move(page));
    }
    return Gameplay::Ok(std::move(out));
}

} // namespace Potato::Campaign
