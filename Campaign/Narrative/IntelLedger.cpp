#include "Campaign/Narrative/IntelLedger.h"

#include <algorithm>

namespace Potato::Campaign {

using Gameplay::JsonValue;

namespace {

const char* kVerdicts[] = {"unresolved", "true", "false"};
const char* kKinds[] = {"rival_temperament", "rival_habit",
                        "enemy_disposition", "terrain_intel"};

template <typename E, std::size_t N>
bool EnumFromName(std::string_view name, const char* (&table)[N],
                  E& out) {
    for (std::size_t i = 0; i < N; ++i) {
        if (name == table[i]) {
            out = static_cast<E>(i);
            return true;
        }
    }
    return false;
}

} // namespace

const char* IntelVerdictName(IntelVerdict v) {
    const auto i = static_cast<std::size_t>(v);
    return i < 3 ? kVerdicts[i] : "unresolved";
}

bool IntelVerdictFromName(std::string_view name,
                          IntelVerdict& out) {
    return EnumFromName(name, kVerdicts, out);
}

const char* IntelKindName(IntelKind k) {
    const auto i = static_cast<std::size_t>(k);
    return i < 4 ? kKinds[i] : "terrain_intel";
}

bool IntelKindFromName(std::string_view name, IntelKind& out) {
    return EnumFromName(name, kKinds, out);
}

const IntelEntry* IntelLedger::Entry(std::uint64_t seq) const {
    if (seq == 0 || seq > entries_.size()) return nullptr;
    return &entries_[static_cast<std::size_t>(seq - 1)];
}

Gameplay::Result<std::uint64_t>
IntelLedger::Record(std::string_view subject, IntelKind kind,
                    std::string_view claim, std::int64_t chapter) {
    if (entries_.size() >= MAX_ENTRIES) {
        return Gameplay::Fail<std::uint64_t>("intel",
                                             "ledger full");
    }
    if (subject.empty() || subject.size() > MAX_SUBJECT_LEN ||
        claim.empty() || claim.size() > MAX_CLAIM_LEN) {
        return Gameplay::Fail<std::uint64_t>("intel",
                                             "bad claim shape");
    }
    IntelEntry e;
    e.seq = entries_.size() + 1;
    e.subject = std::string(subject);
    e.kind = kind;
    e.claim = std::string(claim);
    e.chapter = chapter;
    entries_.push_back(std::move(e));
    return Gameplay::Ok(entries_.back().seq);
}

Gameplay::Result<bool>
IntelLedger::Resolve(std::uint64_t seq, IntelVerdict verdict) {
    IntelEntry* e = nullptr;
    if (seq > 0 && seq <= entries_.size()) {
        e = &entries_[static_cast<std::size_t>(seq - 1)];
    }
    if (e == nullptr) {
        return Gameplay::Fail<bool>("intel", "no such claim");
    }
    if (verdict == IntelVerdict::Unresolved) {
        return Gameplay::Fail<bool>("intel",
                                    "a claim cannot un-resolve");
    }
    if (e->verdict != IntelVerdict::Unresolved) {
        return Gameplay::Fail<bool>(
            "intel", "the ledger does not un-judge");
    }
    e->verdict = verdict;
    return Gameplay::Ok(true);
}

int IntelLedger::DistortionFor(std::string_view subject) const {
    std::size_t resolved = 0, provenFalse = 0;
    for (const IntelEntry& e : entries_) {
        if (e.subject != subject ||
            e.verdict == IntelVerdict::Unresolved) {
            continue;
        }
        ++resolved;
        if (e.verdict == IntelVerdict::False) ++provenFalse;
    }
    if (resolved == 0) return 0;
    return static_cast<int>(provenFalse * 100 / resolved);
}

std::vector<ClaimKind>
IntelLedger::ProvenWrongFor(std::string_view subject) const {
    std::vector<ClaimKind> out;
    for (const IntelEntry& e : entries_) {
        if (e.subject != subject ||
            e.verdict != IntelVerdict::False) {
            continue;
        }
        ClaimKind kind;
        bool map = false;
        if (e.kind == IntelKind::RivalTemperament) {
            kind = ClaimKind::Temperament;
            map = true;
        } else if (e.kind == IntelKind::RivalHabit) {
            kind = ClaimKind::Habit;
            map = true;
        }
        if (map &&
            std::find(out.begin(), out.end(), kind) == out.end()) {
            out.push_back(kind);
        }
    }
    return out;
}

std::vector<const IntelEntry*>
IntelLedger::PendingFor(std::string_view subject) const {
    std::vector<const IntelEntry*> out;
    for (const IntelEntry& e : entries_) {
        if (e.subject == subject &&
            e.verdict == IntelVerdict::Unresolved) {
            out.push_back(&e);
        }
    }
    return out;
}

JsonValue IntelLedger::ToJson() const {
    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    JsonValue::Array list;
    for (const IntelEntry& e : entries_) {
        JsonValue::Object o;
        o["subject"] = JsonValue::String(e.subject);
        o["kind"] = JsonValue::String(IntelKindName(e.kind));
        o["claim"] = JsonValue::String(e.claim);
        o["chapter"] = JsonValue::Int(e.chapter);
        o["verdict"] =
            JsonValue::String(IntelVerdictName(e.verdict));
        list.push_back(JsonValue::MakeObject(std::move(o)));
    }
    root["entries"] = JsonValue::MakeArray(std::move(list));
    return JsonValue::MakeObject(std::move(root));
}

Gameplay::Result<IntelLedger>
IntelLedger::FromJson(const JsonValue& doc) {
    const std::string* s = doc.FindString("schema");
    if (s == nullptr || *s != SCHEMA) {
        return Gameplay::Fail<IntelLedger>("intel", "bad schema");
    }
    const JsonValue& list = doc["entries"];
    if (!list.IsArray() || list.Size() > MAX_ENTRIES) {
        return Gameplay::Fail<IntelLedger>("intel",
                                           "bad entries");
    }
    IntelLedger il;
    for (const JsonValue& v : list.Items()) {
        if (!v.IsObject()) {
            return Gameplay::Fail<IntelLedger>("intel",
                                               "bad entry");
        }
        const std::string* subj = v.FindString("subject");
        const std::string* kindS = v.FindString("kind");
        const std::string* claim = v.FindString("claim");
        const std::string* verdict = v.FindString("verdict");
        IntelKind kind;
        IntelVerdict vd;
        if (subj == nullptr || kindS == nullptr ||
            claim == nullptr || verdict == nullptr ||
            !IntelKindFromName(*kindS, kind) ||
            !IntelVerdictFromName(*verdict, vd)) {
            return Gameplay::Fail<IntelLedger>("intel",
                                               "bad entry field");
        }
        IntelEntry e;
        e.seq = il.entries_.size() + 1;
        e.subject = *subj;
        e.kind = kind;
        e.claim = *claim;
        e.chapter = v["chapter"].AsInt(-1);
        e.verdict = vd;
        if (e.subject.empty() ||
            e.subject.size() > MAX_SUBJECT_LEN || e.claim.empty() ||
            e.claim.size() > MAX_CLAIM_LEN) {
            return Gameplay::Fail<IntelLedger>("intel",
                                               "bad entry bounds");
        }
        il.entries_.push_back(std::move(e));
    }
    return Gameplay::Ok(std::move(il));
}

} // namespace Potato::Campaign
