#include "Campaign/Narrative/Marginalia.h"

#include "Campaign/Ledger/Ledger.h"

#include <algorithm>
#include <string_view>

namespace Potato::Campaign {

using Gameplay::JsonValue;

namespace {

// FNV-1a over the note's identity — same deterministic-pick
// convention as Buchao/HistorianReport clause pools.
std::uint64_t Fn1a(std::string_view s) {
    std::uint64_t h = 14695981039346656037ull;
    for (char c : s) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
    return h;
}

bool HasTag(const LedgerEntry& e, std::string_view t) {
    return std::find(e.tags.begin(), e.tags.end(), t) !=
           e.tags.end();
}

bool HasPrefixTag(const LedgerEntry& e, std::string_view p) {
    for (const std::string& t : e.tags) {
        if (t.size() > p.size() &&
            t.compare(0, p.size(), p) == 0) {
            return true;
        }
    }
    return false;
}

// Clause pools — one per salient tag family, in priority order
// (the first matching family owns the note's register).
constexpr std::string_view kPoolAtrocity[] = {
    "是筆酷烈，史官不忍書。",
    "冊上斑斑，皆生靈也。",
};
constexpr std::string_view kPoolMyth[] = {
    "異聞入冊，姑妄存之。",
    "神怪之事，存疑可也。",
};
constexpr std::string_view kPoolMarch[] = {
    "師行糧隨，慎之。",
    "轉進之日，秣馬厲兵。",
};
constexpr std::string_view kPoolRaid[] = {
    "掠商之舉，得財失信。",
    "利歸行伍，怨歸乡里。",
};
constexpr std::string_view kPoolResolution[] = {
    "是役已結，功過兩存。",
    "勝負既定，冊上留名。",
};
constexpr std::string_view kPoolChapter[] = {
    "回目既訖，後事分解。",
    "此回已畢，且翻次頁。",
};

template <std::size_t N>
std::string_view Pick(const std::string_view (&pool)[N],
                      std::uint64_t salt) {
    return pool[salt % N];
}

std::string_view PoolClause(const LedgerEntry& e,
                            std::uint64_t salt) {
    if (HasTag(e, Ledger::TAG_ATROCITY)) {
        return Pick(kPoolAtrocity, salt);
    }
    if (HasTag(e, Ledger::TAG_MYTH)) {
        return Pick(kPoolMyth, salt);
    }
    if (HasTag(e, "march")) {
        return Pick(kPoolMarch, salt);
    }
    if (HasTag(e, "raid")) {
        return Pick(kPoolRaid, salt);
    }
    if (HasPrefixTag(e, Ledger::TAG_RESOLUTION)) {
        return Pick(kPoolResolution, salt);
    }
    return Pick(kPoolChapter, salt); // chapter: prefix
}

// Tag scan — ScribeWorthy's single place of truth for what earns
// a margin note.
std::string_view SalientFamily(const LedgerEntry& e) {
    if (HasTag(e, Ledger::TAG_ATROCITY)) return "atrocity";
    if (HasTag(e, Ledger::TAG_MYTH)) return "myth";
    if (HasTag(e, "march")) return "march";
    if (HasTag(e, "raid")) return "raid";
    if (HasPrefixTag(e, Ledger::TAG_RESOLUTION)) return "resolution";
    if (HasPrefixTag(e, Ledger::TAG_CHAPTER)) return "chapter";
    return {};
}

} // namespace

const MarginaliaNote*
MarginaliaStore::Find(std::uint64_t seq) const {
    for (const MarginaliaNote& n : notes_) {
        if (n.seq == seq) return &n;
    }
    return nullptr;
}

bool MarginaliaStore::SetSuspect(std::uint64_t seq, bool suspect) {
    for (MarginaliaNote& n : notes_) {
        if (n.seq == seq) {
            n.suspect = suspect;
            return true;
        }
    }
    return false;
}

Gameplay::Result<JsonValue> MarginaliaStore::ToJson() const {
    JsonValue::Object o;
    o["schema"] = JsonValue::String(std::string(SCHEMA));
    JsonValue::Array notes;
    for (const MarginaliaNote& n : notes_) {
        JsonValue::Object j;
        j["seq"] = JsonValue::Int(static_cast<std::int64_t>(n.seq));
        j["note"] = JsonValue::String(n.note);
        if (n.suspect) {
            j["suspect"] = JsonValue::Bool(true);
        }
        notes.push_back(JsonValue::MakeObject(std::move(j)));
    }
    o["notes"] = JsonValue::MakeArray(std::move(notes));
    return Gameplay::Ok(JsonValue::MakeObject(std::move(o)));
}

Gameplay::Result<MarginaliaStore>
MarginaliaStore::FromJson(const JsonValue& doc) {
    const std::string* s = doc.FindString("schema");
    if (s == nullptr || *s != SCHEMA) {
        return Gameplay::Fail<MarginaliaStore>("schema",
                                               "expected "
                                               "potato.marginalia/1");
    }
    const JsonValue& notes = doc["notes"];
    if (!notes.IsArray() || notes.Size() > MAX_NOTES) {
        return Gameplay::Fail<MarginaliaStore>("field",
                                               "bad notes");
    }
    MarginaliaStore st;
    for (const JsonValue& j : notes.Items()) {
        const JsonValue& seq = j["seq"];
        const std::string* note = j.FindString("note");
        if (!seq.IsInt() || seq.AsInt() < 0 ||
            note == nullptr || note->size() > MAX_NOTE_LEN) {
            return Gameplay::Fail<MarginaliaStore>("field",
                                                   "bad note");
        }
        bool suspect = false;
        if (j.Has("suspect")) {
            if (!j["suspect"].IsBool()) {
                return Gameplay::Fail<MarginaliaStore>(
                    "field", "bad suspect flag");
            }
            suspect = j["suspect"].AsBool();
        }
        const std::uint64_t sq =
            static_cast<std::uint64_t>(seq.AsInt());
        if (st.Find(sq) != nullptr) {
            continue; // first wins — dedupe, not reject
        }
        MarginaliaNote n;
        n.seq = sq;
        n.note = *note;
        n.suspect = suspect;
        st.notes_.push_back(std::move(n));
    }
    return Gameplay::Ok(std::move(st));
}

bool ScribeWorthy(const LedgerEntry& e) {
    return !SalientFamily(e).empty();
}

std::string ComposeScribeNote(const LedgerEntry& e) {
    // Salt: memo bytes folded with seq LE — same entry, same note.
    std::uint64_t salt = Fn1a(e.memo);
    for (int i = 0; i < 8; ++i) {
        salt = salt * 1099511628211ull ^
               ((e.seq >> (i * 8)) & 0xffull);
    }
    return std::string(PoolClause(e, salt));
}

Gameplay::Result<std::size_t>
AnnotateScribe(const Ledger& l, MarginaliaStore& store,
               std::size_t maxNotes) {
    if (maxNotes == 0) {
        return Gameplay::Ok<std::size_t>(0);
    }
    std::size_t added = 0;
    for (const LedgerEntry& e : l.Entries()) {
        if (added >= maxNotes ||
            store.notes_.size() >= MarginaliaStore::MAX_NOTES) {
            break;
        }
        if (store.Find(e.seq) != nullptr || !ScribeWorthy(e)) {
            continue;
        }
        MarginaliaNote n;
        n.seq = e.seq;
        n.note = ComposeScribeNote(e);
        store.notes_.push_back(std::move(n));
        ++added;
    }
    return Gameplay::Ok(std::move(added));
}

std::string RenderMarginalia(const MarginaliaStore& store,
                             const Ledger& l) {
    std::string out;
    for (const MarginaliaNote& n : store.Notes()) {
        out += "批〔";
        out += std::to_string(n.seq);
        out += "〕";
        out += n.note;
        // A note is tainted by its own flag OR by the row beneath
        // it — a suspect entry poisons the hand that praised it.
        bool suspect = n.suspect;
        if (!suspect && n.seq < l.Entries().size()) {
            suspect = l.Entries()[n.seq].suspect;
        }
        if (suspect) {
            out += "（疑）";
        }
        out += '\n';
    }
    return out;
}

} // namespace Potato::Campaign
