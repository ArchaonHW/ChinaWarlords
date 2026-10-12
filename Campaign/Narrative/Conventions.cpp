#include "Campaign/Narrative/Conventions.h"

#include "Campaign/Governance/Accumulators.h" // FoldGovernance
#include "Campaign/Ledger/Ledger.h"          // Account::Mandate

namespace Potato::Campaign {

using Gameplay::JsonValue;

namespace {

// Variant lookup — requested key, then "default", then the map's
// first entry (deterministic: std::map ordering), then empty.
std::string_view Variant(const std::map<std::string, std::string, std::less<>>& m,
                         std::string_view key) {
    auto it = m.find(key);
    if (it == m.end()) it = m.find("default");
    if (it == m.end() && !m.empty()) it = m.begin();
    return it == m.end() ? std::string_view()
                         : std::string_view(it->second);
}

// Variant map field — object of short string variants, or a bare
// string treated as a single "default" variant (content shorthand).
Gameplay::Result<std::map<std::string, std::string, std::less<>>>
ParseVariants(const JsonValue& v, const char* field) {
    std::map<std::string, std::string, std::less<>> m;
    if (v.IsString()) {
        if (v.AsString().size() > ChapterConventions::MAX_TEXT_LEN) {
            return Gameplay::Fail<std::map<std::string, std::string, std::less<>>>(
                "narrative", "variant text too long");
        }
        m.emplace("default", v.AsString());
        return Gameplay::Ok(std::move(m));
    }
    if (!v.IsObject()) {
        return Gameplay::Fail<std::map<std::string, std::string, std::less<>>>(
            "narrative", field);
    }
    if (v.Size() > ChapterConventions::MAX_VARIANTS) {
        return Gameplay::Fail<std::map<std::string, std::string, std::less<>>>(
            "narrative", "too many variants");
    }
    for (const auto& [key, val] : v.Members()) {
        if (key.empty() || key.size() > 64 || !val.IsString() ||
            val.AsString().size() >
                ChapterConventions::MAX_TEXT_LEN) {
            return Gameplay::Fail<std::map<std::string, std::string, std::less<>>>(
                "narrative", "bad variant");
        }
        m.emplace(key, val.AsString());
    }
    return Gameplay::Ok(std::move(m));
}

JsonValue VariantsToJson(const std::map<std::string, std::string, std::less<>>& m) {
    JsonValue::Object o;
    for (const auto& [k, v] : m) {
        o.emplace(k, JsonValue::String(v));
    }
    return JsonValue::MakeObject(std::move(o));
}

// FNV-1a — same hash family as the ledger chain, the record
// root, and the Buchao/Marginalia picks.
std::uint64_t Fnv1a(std::string_view s, std::uint64_t h) {
    constexpr std::uint64_t kPrime = 1099511628211ull;
    for (const char c : s) {
        h = (h ^ static_cast<unsigned char>(c)) * kPrime;
    }
    return h;
}

// 6.7 clause pool — array of clause strings, or a bare string
// as single-clause shorthand (same sugar as variant maps).
Gameplay::Result<ClausePool>
ParseClausePool(const JsonValue& v, const char* field) {
    ClausePool pool;
    if (v.IsString()) {
        if (v.AsString().size() >
            ChapterConventions::MAX_TEXT_LEN) {
            return Gameplay::Fail<ClausePool>("narrative",
                                              "clause too long");
        }
        pool.push_back(v.AsString());
        return Gameplay::Ok(std::move(pool));
    }
    if (!v.IsArray() || v.Size() == 0 ||
        v.Size() > ChapterConventions::MAX_CLAUSES) {
        return Gameplay::Fail<ClausePool>("narrative", field);
    }
    for (std::size_t i = 0; i < v.Size(); ++i) {
        const JsonValue& c = v.At(i);
        if (!c.IsString() || c.AsString().empty() ||
            c.AsString().size() >
                ChapterConventions::MAX_TEXT_LEN) {
            return Gameplay::Fail<ClausePool>("narrative",
                                              "bad clause");
        }
        pool.push_back(c.AsString());
    }
    return Gameplay::Ok(std::move(pool));
}

// One axis map inside a slot: object of key → clause pool.
Gameplay::Result<std::map<std::string, ClausePool, std::less<>>>
ParseAxis(const JsonValue& v, const char* field) {
    std::map<std::string, ClausePool, std::less<>> m;
    if (!v.IsObject() ||
        v.Size() > ChapterConventions::MAX_VARIANTS) {
        return Gameplay::Fail<
            std::map<std::string, ClausePool, std::less<>>>(
            "narrative", field);
    }
    for (const auto& [key, pv] : v.Members()) {
        if (key.empty() || key.size() > 64) {
            return Gameplay::Fail<
                std::map<std::string, ClausePool, std::less<>>>(
                "narrative", "bad variant key");
        }
        auto pool = ParseClausePool(pv, "clause pool");
        if (!pool.ok()) {
            return Gameplay::Fail<
                std::map<std::string, ClausePool,
                         std::less<>>>("narrative", pool.reason);
        }
        m.emplace(key, std::move(pool.value));
    }
    return Gameplay::Ok(std::move(m));
}

// Pool lookup — requested key, then "default", else silence.
// Deliberately NOT the 6.2 first-entry fallback: a chapter
// verdict must render something, but a variant clause is
// color — a state the pack doesn't cover stays silent
// rather than borrowing the wrong key's voice.
const ClausePool* PoolAt(
    const std::map<std::string, ClausePool, std::less<>>& m,
    std::string_view key) {
    auto it = m.find(key);
    if (it == m.end()) it = m.find("default");
    return it == m.end() ? nullptr : &it->second;
}

JsonValue AxisToJson(
    const std::map<std::string, ClausePool, std::less<>>& m) {
    JsonValue::Object o;
    for (const auto& [k, pool] : m) {
        JsonValue::Array a;
        for (const std::string& c : pool) {
            a.push_back(JsonValue::String(c));
        }
        o.emplace(k, JsonValue::MakeArray(std::move(a)));
    }
    return JsonValue::MakeObject(std::move(o));
}

} // namespace

FrameMood LedgerMood(const Ledger& l) {
    const std::int64_t corruption = FoldGovernance(l).corruption;
    if (corruption >= MOOD_CORRUPT_AT) return FrameMood::Corrupt;
    if (corruption >= MOOD_TAINTED_AT) return FrameMood::Tainted;
    return FrameMood::Clean;
}

FrameOmen LedgerOmen(const Ledger& l) {
    const std::int64_t mandate = l.Balance(Account::Mandate);
    if (mandate >= OMEN_BLESSED_AT) return FrameOmen::Blessed;
    if (mandate >= OMEN_FADING_AT) return FrameOmen::Fading;
    return FrameOmen::Barren;
}

std::string_view MoodKey(FrameMood m) {
    switch (m) {
        case FrameMood::Clean: return "clean";
        case FrameMood::Tainted: return "tainted";
        case FrameMood::Corrupt: return "corrupt";
    }
    return "clean";
}

std::string_view OmenKey(FrameOmen o) {
    switch (o) {
        case FrameOmen::Blessed: return "blessed";
        case FrameOmen::Fading: return "fading";
        case FrameOmen::Barren: return "barren";
    }
    return "fading";
}

std::string_view StanceKey(Gameplay::GodStance s) {
    switch (s) {
        case Gameplay::GodStance::Favorable: return "favorable";
        case Gameplay::GodStance::Neutral: return "neutral";
        case Gameplay::GodStance::Wrathful: return "wrathful";
    }
    return "neutral";
}

Gameplay::Result<ChapterConventions>
ChapterConventions::FromJson(const JsonValue& doc) {
    const std::string* s = doc.FindString("schema");
    if (s == nullptr || *s != SCHEMA) {
        return Gameplay::Fail<ChapterConventions>("narrative",
                                                  "bad schema");
    }
    const JsonValue& chapters = doc["chapters"];
    if (!chapters.IsObject() || chapters.Size() > MAX_CHAPTERS) {
        return Gameplay::Fail<ChapterConventions>("narrative",
                                                  "bad chapters");
    }
    ChapterConventions conv;
    for (const auto& [id, cv] : chapters.Members()) {
        if (id.empty() || id.size() > MAX_ID_LEN || !cv.IsObject()) {
            return Gameplay::Fail<ChapterConventions>("narrative",
                                                      "bad chapter id");
        }
        ChapterFrame f;
        const std::string* fp = cv.FindString("frontispiece");
        if (fp != nullptr) {
            if (fp->size() > MAX_TEXT_LEN) {
                return Gameplay::Fail<ChapterConventions>(
                    "narrative", "frontispiece too long");
            }
            f.frontispiece = *fp;
        }
        if (cv.Has("judgment")) {
            auto j = ParseVariants(cv["judgment"], "judgment");
            if (!j.ok()) {
                return Gameplay::Fail<ChapterConventions>(
                    "narrative", j.reason);
            }
            f.judgment = std::move(j.value);
        }
        if (cv.Has("cliffhanger")) {
            auto ch = ParseVariants(cv["cliffhanger"], "cliffhanger");
            if (!ch.ok()) {
                return Gameplay::Fail<ChapterConventions>(
                    "narrative", ch.reason);
            }
            f.cliffhanger = std::move(ch.value);
        }
        conv.frames_[id] = std::move(f);
    }

    // 6.7 — optional document-variant pools. Absent = empty;
    // present must be variants.<doc>.<slot>.<axis>.<key> with a
    // strict axis vocabulary.
    if (doc.Has("variants")) {
        const JsonValue& vs = doc["variants"];
        if (!vs.IsObject() || vs.Size() > MAX_DOCS) {
            return Gameplay::Fail<ChapterConventions>(
                "narrative", "bad variants");
        }
        for (const auto& [docName, dv] : vs.Members()) {
            if (docName.empty() || docName.size() > MAX_ID_LEN ||
                !dv.IsObject() ||
                dv.Size() > MAX_VARIANTS) {
                return Gameplay::Fail<ChapterConventions>(
                    "narrative", "bad variant doc");
            }
            DocVariants doc;
            for (const auto& [slotName, sv] : dv.Members()) {
                if (slotName.empty() ||
                    slotName.size() > MAX_ID_LEN ||
                    !sv.IsObject()) {
                    return Gameplay::Fail<ChapterConventions>(
                        "narrative", "bad variant slot");
                }
                VariantSlot slot;
                for (const auto& [axisName, av] : sv.Members()) {
                    std::map<std::string, ClausePool,
                             std::less<>>* target = nullptr;
                    if (axisName == "mood") {
                        target = &slot.mood;
                    } else if (axisName == "omen") {
                        target = &slot.omen;
                    } else if (axisName == "stance") {
                        target = &slot.stance;
                    } else {
                        return Gameplay::Fail<
                            ChapterConventions>(
                            "narrative", "bad variant axis");
                    }
                    auto axis = ParseAxis(av, "variant axis");
                    if (!axis.ok()) {
                        return Gameplay::Fail<ChapterConventions>(
                            "narrative", axis.reason);
                    }
                    *target = std::move(axis.value);
                }
                doc.slots[slotName] = std::move(slot);
            }
            conv.variants_[docName] = std::move(doc);
        }
    }
    return Gameplay::Ok(std::move(conv));
}

JsonValue ChapterConventions::ToJson() const {
    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    JsonValue::Object chapters;
    for (const auto& [id, f] : frames_) {
        JsonValue::Object c;
        if (!f.frontispiece.empty()) {
            c["frontispiece"] = JsonValue::String(f.frontispiece);
        }
        if (!f.judgment.empty()) {
            c["judgment"] = VariantsToJson(f.judgment);
        }
        if (!f.cliffhanger.empty()) {
            c["cliffhanger"] = VariantsToJson(f.cliffhanger);
        }
        chapters[id] = JsonValue::MakeObject(std::move(c));
    }
    root["chapters"] = JsonValue::MakeObject(std::move(chapters));
    if (!variants_.empty()) {
        JsonValue::Object docs;
        for (const auto& [docName, dv] : variants_) {
            JsonValue::Object slots;
            for (const auto& [slotName, sv] : dv.slots) {
                JsonValue::Object axes;
                if (!sv.mood.empty()) {
                    axes["mood"] = AxisToJson(sv.mood);
                }
                if (!sv.omen.empty()) {
                    axes["omen"] = AxisToJson(sv.omen);
                }
                if (!sv.stance.empty()) {
                    axes["stance"] = AxisToJson(sv.stance);
                }
                slots[slotName] =
                    JsonValue::MakeObject(std::move(axes));
            }
            docs[docName] = JsonValue::MakeObject(std::move(slots));
        }
        root["variants"] = JsonValue::MakeObject(std::move(docs));
    }
    return JsonValue::MakeObject(std::move(root));
}

const ChapterFrame*
ChapterConventions::Find(std::string_view chapterId) const {
    auto it = frames_.find(chapterId);
    return it == frames_.end() ? nullptr : &it->second;
}

const DocVariants*
ChapterConventions::Variants(std::string_view doc) const {
    auto it = variants_.find(doc);
    return it == variants_.end() ? nullptr : &it->second;
}

std::string RenderChapterOpen(const ChapterConventions& conv,
                              std::string_view chapterId) {
    const ChapterFrame* f = conv.Find(chapterId);
    // Court commission register — imperative, formulaic; the header
    // names the mandate's target, the body is the sealed text.
    std::string s = "敕命·";
    s += chapterId;
    s += "\n奉敕：";
    if (f != nullptr && !f->frontispiece.empty()) {
        s += f->frontispiece;
    } else {
        s += "爾其整飭師旅，克復疆土，毋怠。"; // generic mandate
    }
    s += "\n";
    return s;
}

std::string RenderChapterClose(const ChapterConventions& conv,
                               std::string_view chapterId,
                               const Ledger& ledger) {
    const ChapterFrame* f = conv.Find(chapterId);
    std::string s = "評曰：";
    std::string_view j =
        f ? Variant(f->judgment, MoodKey(LedgerMood(ledger)))
          : std::string_view();
    s += j.empty() ? "是役也，勝負已判，功過在冊。" : j;
    s += "\n";
    std::string_view c =
        f ? Variant(f->cliffhanger, OmenKey(LedgerOmen(ledger)))
          : std::string_view();
    if (!c.empty()) {
        s += c;
    } else {
        s += "後事如何";
    }
    s += "——且聽下回分解。\n";
    return s;
}

std::string RenderDocVariant(const ChapterConventions& conv,
                             std::string_view doc,
                             std::string_view slot,
                             const Ledger& ledger,
                             Gameplay::GodStance stance,
                             std::uint64_t salt) {
    const DocVariants* dv = conv.Variants(doc);
    if (dv == nullptr) return {};
    auto sit = dv->slots.find(slot);
    if (sit == dv->slots.end()) return {};
    const VariantSlot& vs = sit->second;

    // Pick identity: FNV-1a(salt LE ‖ doc ‖ slot ‖ axis) — the
    // same entry always composes the same clause, no PRNG.
    std::uint64_t h0 = 14695981039346656037ull;
    for (int i = 0; i < 8; ++i) {
        const char b = static_cast<char>(
            (salt >> (i * 8)) & 0xff);
        h0 = Fnv1a(std::string_view(&b, 1), h0);
    }
    h0 = Fnv1a(doc, h0);
    h0 = Fnv1a(slot, h0);

    std::string out;
    const auto emit = [&](const std::map<std::string, ClausePool,
                                         std::less<>>& axisMap,
                          std::string_view axisName,
                          std::string_view key) {
        const ClausePool* pool = PoolAt(axisMap, key);
        if (pool == nullptr || pool->empty()) return;
        const std::uint64_t h = Fnv1a(axisName, h0);
        out += (*pool)[h % pool->size()];
        out += '\n';
    };
    emit(vs.mood, "mood", MoodKey(LedgerMood(ledger)));
    emit(vs.omen, "omen", OmenKey(LedgerOmen(ledger)));
    emit(vs.stance, "stance", StanceKey(stance));
    return out;
}

} // namespace Potato::Campaign
