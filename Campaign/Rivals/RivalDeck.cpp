#include "Campaign/Rivals/RivalDeck.h"

#include "Campaign/Narrative/IntelLedger.h" // DistortionFor

#include <algorithm>
#include <unordered_set>

namespace Potato::Campaign {

// Wire-format tripwire: adding a TriggerKind without updating
// RivalDeck's wire names breaks the build, not a save.
static_assert(
    kTriggerKindCount ==
        static_cast<std::size_t>(
            Gameplay::TriggerKind::EnemyAdjacent) +
            1,
    "TriggerKind grew — update TriggerKindWire and bump "
    "potato.rivals");

namespace {

const char* kPriorNames[] = {"aggressive", "defensive",
                             "cunning"};

} // namespace

const char* RivalPriorName(RivalPrior p) {
    const auto i = static_cast<std::size_t>(p);
    return i < 3 ? kPriorNames[i] : "unknown";
}

bool RivalPriorFromName(std::string_view name,
                        RivalPrior& out) {
    for (std::size_t i = 0; i < 3; ++i) {
        if (name == kPriorNames[i]) {
            out = static_cast<RivalPrior>(i);
            return true;
        }
    }
    return false;
}

const char* TriggerKindWire(Gameplay::TriggerKind t) {
    switch (t) {
    case Gameplay::TriggerKind::Always:
        return "always";
    case Gameplay::TriggerKind::CohesionBelow:
        return "cohesion_below";
    case Gameplay::TriggerKind::EnemyInRegion:
        return "enemy_in_region";
    case Gameplay::TriggerKind::EnemyAdjacent:
        return "enemy_adjacent";
    }
    return "unknown";
}

const GeneralDossier* RivalBook::Find(std::string_view id) const {
    for (const auto& d : dossiers_) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

Gameplay::Result<int> RivalBook::RecordChapter(
    std::string_view id, RivalPrior prior,
    const TriggerHistogram& usage) {
    if (id.empty() || id.size() > MAX_ID_LEN) {
        return Gameplay::Fail<int>("rivals", "bad id");
    }
    for (std::int64_t c : usage) {
        if (c < 0 || c > MAX_COUNT) {
            return Gameplay::Fail<int>("rivals",
                           "usage count out of range");
        }
    }
    // ALL fallible checks before ANY mutation: overflow is
    // evaluated against the existing histogram (or zeros), so a
    // rejected call can never strand a half-created dossier.
    std::size_t idx = dossiers_.size();
    for (std::size_t i = 0; i < dossiers_.size(); ++i) {
        if (dossiers_[i].id == id) {
            idx = i;
            break;
        }
    }
    const bool fresh = (idx == dossiers_.size());
    if (fresh && dossiers_.size() >= MAX_RIVALS) {
        return Gameplay::Fail<int>("rivals", "rival book full");
    }
    if (!fresh &&
        dossiers_[idx].chaptersObserved >= MAX_COUNT) {
        return Gameplay::Fail<int>("rivals", "observed out of range");
    }
    for (std::size_t i = 0; i < kTriggerKindCount; ++i) {
        const std::int64_t base =
            fresh ? 0 : dossiers_[idx].triggers[i];
        if (usage[i] > MAX_COUNT - base) {
            return Gameplay::Fail<int>("rivals",
                           "usage would overflow bound");
        }
    }
    if (fresh) {
        dossiers_.push_back(
            GeneralDossier{std::string(id), prior, 0, {}});
    }
    auto& d = dossiers_[idx];
    for (std::size_t i = 0; i < kTriggerKindCount; ++i) {
        d.triggers[i] += usage[i];
    }
    ++d.chaptersObserved;
    return Gameplay::Ok(static_cast<int>(idx));
}

std::vector<std::string> RivalBook::PrepareCounterDeck(
    std::string_view id,
    const Gameplay::DoctrineLibrary& counters,
    int depth, const IntelLedger* intel) const {
    std::vector<std::string> deck;
    const GeneralDossier* d = Find(id);
    if (!d || depth <= 0 ||
        d->chaptersObserved < MIN_CHAPTERS_TO_LEARN) {
        return deck;
    }
    // Story 6.4: the rival's own intel quality caps its counter
    // precision — every resolved-false claim it believed erodes
    // the deck proportionally (integer math, deterministic).
    if (intel != nullptr) {
        const int distortion = intel->DistortionFor(d->id);
        depth = depth * (100 - distortion) / 100;
        if (depth <= 0) return deck;
    }
    // Rank triggers by usage desc, ties by ordinal —
    // deterministic under replay and across toolchains.
    std::array<std::size_t, kTriggerKindCount> order{};
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) {
                  if (d->triggers[a] != d->triggers[b]) {
                      return d->triggers[a] > d->triggers[b];
                  }
                  return a < b;
              });
    for (std::size_t ord : order) {
        if (deck.size() >= static_cast<std::size_t>(depth)) {
            break;
        }
        if (d->triggers[ord] <= 0) {
            continue; // never observed — nothing to counter
        }
        const char* wire = TriggerKindWire(
            static_cast<Gameplay::TriggerKind>(ord));
        if (std::string_view(wire) == "unknown") {
            continue; // enum grew without a wire name — skip,
                      // never emit counter.unknown
        }
        const std::string cardId =
            std::string("counter.") + wire;
        if (counters.Find(cardId)) {
            deck.push_back(cardId);
        }
    }
    return deck;
}

Gameplay::Result<Gameplay::JsonValue> RivalBook::ToJson()
    const {
    using Gameplay::JsonValue;
    JsonValue::Array gens;
    gens.reserve(dossiers_.size());
    for (const auto& d : dossiers_) {
        JsonValue::Object o;
        o["id"] = JsonValue::String(d.id);
        o["prior"] =
            JsonValue::String(RivalPriorName(d.prior));
        o["observed"] = JsonValue::Int(d.chaptersObserved);
        // Name-keyed, not positional: a TriggerKind insert or
        // reorder can't silently misattribute saved counts.
        JsonValue::Object trig;
        for (std::size_t i = 0; i < kTriggerKindCount; ++i) {
            trig[TriggerKindWire(
                static_cast<Gameplay::TriggerKind>(i))] =
                JsonValue::Int(d.triggers[i]);
        }
        o["triggers"] = JsonValue::MakeObject(std::move(trig));
        gens.push_back(JsonValue::MakeObject(std::move(o)));
    }
    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    root["generals"] = JsonValue::MakeArray(std::move(gens));
    return Gameplay::Ok(JsonValue::MakeObject(std::move(root)));
}

Gameplay::Result<RivalBook> RivalBook::FromJson(
    const Gameplay::JsonValue& doc) {
    const std::string* s = doc.FindString("schema");
    if (!s || *s != SCHEMA) {
        return Gameplay::Fail<RivalBook>("schema", "expected potato.rivals/1");
    }
    const auto& gens = doc["generals"];
    if (!gens.IsArray()) {
        return Gameplay::Fail<RivalBook>("schema", "generals: not an array");
    }
    if (gens.Size() > MAX_RIVALS) {
        return Gameplay::Fail<RivalBook>("rivals", "rival book full");
    }
    RivalBook book;
    std::unordered_set<std::string> seen;
    for (const auto& g : gens.Items()) {
        if (!g.IsObject() || !g["id"].IsString() ||
            !g["prior"].IsString() || !g["observed"].IsInt() ||
            !g["triggers"].IsObject()) {
            return Gameplay::Fail<RivalBook>("schema",
                           "general: missing/mistyped field");
        }
        const std::string& id = g["id"].AsString();
        if (id.empty() || id.size() > MAX_ID_LEN) {
            return Gameplay::Fail<RivalBook>("rivals", "bad id");
        }
        if (!seen.insert(id).second) {
            return Gameplay::Fail<RivalBook>("rivals", "duplicate rival id");
        }
        RivalPrior prior;
        if (!RivalPriorFromName(g["prior"].AsString(),
                                prior)) {
            return Gameplay::Fail<RivalBook>("schema", "unknown prior");
        }
        const std::int64_t obs = g["observed"].AsInt();
        if (obs < 0 || obs > MAX_COUNT) {
            return Gameplay::Fail<RivalBook>("rivals", "observed out of range");
        }
        // Name-keyed histogram: each expected wire name reads
        // its count; a missing key means 0 (forward-compat for
        // fields a newer build added), a mistyped key rejects.
        const auto& trig = g["triggers"];
        GeneralDossier d;
        d.id = id;
        d.prior = prior;
        d.chaptersObserved = obs;
        for (std::size_t i = 0; i < kTriggerKindCount; ++i) {
            const auto& v = trig[TriggerKindWire(
                static_cast<Gameplay::TriggerKind>(i))];
            if (v.IsNull()) {
                continue; // absent -> zero
            }
            if (!v.IsInt()) {
                return Gameplay::Fail<RivalBook>("schema",
                               "triggers: non-int value");
            }
            const std::int64_t c = v.AsInt();
            if (c < 0 || c > MAX_COUNT) {
                return Gameplay::Fail<RivalBook>("rivals",
                               "usage count out of range");
            }
            d.triggers[i] = c;
        }
        book.dossiers_.push_back(std::move(d));
    }
    return Gameplay::Ok(std::move(book));
}

} // namespace Potato::Campaign
