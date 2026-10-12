#include "Campaign/Characters/Character.h"

#include "Gameplay/Json/Json.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>
#include <system_error>
#include <utility>

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

// Bitcast u64 deck_seed to/from an int64 wire field (WorldState
// seq/PRNG precedent — MintOutbox ledger-seal lineage): bit-exact
// across platforms, negative wire ints are valid seeds.
std::int64_t SeedToWire(std::uint64_t v) {
    std::int64_t s;
    std::memcpy(&s, &v, sizeof(s));
    return s;
}
std::uint64_t SeedFromWire(std::int64_t s) {
    std::uint64_t v;
    std::memcpy(&v, &s, sizeof(v));
    return v;
}

// Closed field set — unknown top-level keys reject (schema-gate
// convention; a typo'd field must not load silently).
bool KnownField(std::string_view key) {
    return key == "schema" || key == "id" || key == "name" ||
           key == "origin" || key == "prior" ||
           key == "deck_seed" || key == "deck" || key == "created";
}

Result<std::string> BoundString(const JsonValue& doc,
                                std::string_view key,
                                std::size_t maxLen) {
    if (doc.Has(key) && !doc[key].IsString()) {
        return Gameplay::Fail<std::string>(
            "character", "'" + std::string(key) +
                         "' must be a string");
    }
    const std::string* s = doc.FindString(key);
    if (s == nullptr || s->empty() || s->size() > maxLen) {
        return Gameplay::Fail<std::string>(
            "character", "missing/empty/out-of-range '" +
                         std::string(key) + "'");
    }
    return Gameplay::Ok(*s);
}

} // namespace

Result<Character> Character::Load(std::string_view path) {
    Result<JsonValue> doc = Gameplay::Json::Load(path, SCHEMA);
    if (!doc.ok()) {
        return Gameplay::Fail<Character>(doc.error, doc.reason);
    }
    return FromJson(doc.value);
}

Result<Character> Character::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Gameplay::Fail<Character>("character",
                                         "root is not an object");
    }
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != "potato.character/1") {
            return Gameplay::Fail<Character>(
                "schema", "expected potato.character/1");
        }
    }
    for (const auto& kv : root.Members()) {
        if (!KnownField(kv.first)) {
            return Gameplay::Fail<Character>(
                "character", "unknown field '" + kv.first + "'");
        }
    }

    Character c;

    auto id = BoundString(root, "id", MAX_ID_LEN);
    if (!id.ok()) {
        return Gameplay::Fail<Character>(id.error, id.reason);
    }
    c.id = std::move(id.value);

    auto name = BoundString(root, "name", MAX_NAME_LEN);
    if (!name.ok()) {
        return Gameplay::Fail<Character>(name.error, name.reason);
    }
    c.name = std::move(name.value);

    auto origin = BoundString(root, "origin", MAX_ORIGIN_LEN);
    if (!origin.ok()) {
        return Gameplay::Fail<Character>(origin.error,
                                         origin.reason);
    }
    c.origin = std::move(origin.value);

    if (!root.Has("prior") || !root["prior"].IsString()) {
        return Gameplay::Fail<Character>(
            "character", "'prior' must be a string");
    }
    if (!RivalPriorFromName(root["prior"].AsString(), c.prior)) {
        return Gameplay::Fail<Character>(
            "character", "unknown prior '" +
                         root["prior"].AsString() + "'");
    }

    const JsonValue& seed = root["deck_seed"];
    if (!seed.IsInt()) {
        return Gameplay::Fail<Character>(
            "character", "'deck_seed' must be an int");
    }
    c.deckSeed = SeedFromWire(seed.AsInt());

    const JsonValue& deck = root["deck"];
    if (!deck.IsNull()) {
        if (!deck.IsArray() || deck.Items().size() > MAX_DECK) {
            return Gameplay::Fail<Character>(
                "character",
                "'deck' must be an array within bound");
        }
        for (const JsonValue& card : deck.Items()) {
            if (!card.IsString() || card.AsString().empty() ||
                card.AsString().size() > MAX_CARD_ID_LEN) {
                return Gameplay::Fail<Character>(
                    "character",
                    "'deck' entries must be non-empty "
                    "bounded strings");
            }
            c.deck.push_back(card.AsString());
        }
    }

    if (root.Has("created")) {
        if (!root["created"].IsBool()) {
            return Gameplay::Fail<Character>(
                "character", "'created' must be a bool");
        }
        c.created = root["created"].AsBool();
    }

    return Gameplay::Ok(std::move(c));
}

JsonValue Character::ToJson() const {
    JsonValue::Object o;
    o["schema"] = JsonValue::String(std::string(SCHEMA));
    o["id"] = JsonValue::String(id);
    o["name"] = JsonValue::String(name);
    o["origin"] = JsonValue::String(origin);
    o["prior"] = JsonValue::String(RivalPriorName(prior));
    o["deck_seed"] = JsonValue::Int(SeedToWire(deckSeed));
    if (!deck.empty()) {
        JsonValue::Array cards;
        for (const std::string& card : deck) {
            cards.push_back(JsonValue::String(card));
        }
        o["deck"] = JsonValue::MakeArray(std::move(cards));
    }
    if (created) {
        o["created"] = JsonValue::Bool(true);
    }
    return JsonValue::MakeObject(std::move(o));
}

CharacterLoadResult
CharacterLibrary::Load(const std::filesystem::path& dir,
                       CharacterLibrary& out) {
    CharacterLoadResult res;

    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(
             dir, std::filesystem::directory_options::none, ec);
         !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        // Per-entry stat uses its OWN error channel — sharing `ec`
        // would let a stat failure be cleared by the next
        // increment() and silently drop the file.
        std::error_code ec2;
        const bool isFile = it->is_regular_file(ec2);
        if (ec2) {
            res.rejected.push_back(
                {it->path(), "io", "stat failed: " + ec2.message()});
            continue;
        }
        std::string ext = it->path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(
                               std::tolower(c));
                       });
        if (isFile && ext == ".json") {
            files.push_back(it->path());
        }
    }
    if (ec) {
        res.error = "io";
        res.reason = "character dir unreadable: " + ec.message();
        return res;
    }
    // Deterministic registration order: filename sort.
    std::sort(files.begin(), files.end());
    if (files.size() > MAX_CHARACTERS) {
        res.error = "overflow";
        res.reason =
            "character file count exceeds MAX_CHARACTERS";
        return res;
    }

    std::vector<Character> chars;
    std::set<std::string> ids;
    for (const std::filesystem::path& f : files) {
        const std::u8string u8 = f.u8string();
        Result<JsonValue> doc = Gameplay::Json::Load(
            std::string_view(
                reinterpret_cast<const char*>(u8.data()),
                u8.size()),
            std::string(SCHEMA));
        if (!doc.ok()) {
            res.rejected.push_back({f, doc.error, doc.reason});
            continue;
        }
        Result<Character> c = Character::FromJson(doc.value);
        if (!c.ok()) {
            res.rejected.push_back({f, c.error, c.reason});
            continue;
        }
        if (ids.count(c.value.id)) {
            res.rejected.push_back(
                {f, "duplicate",
                 "character id already registered"});
            continue;
        }
        ids.insert(c.value.id);
        chars.push_back(std::move(c.value));
    }

    // Canonical order: character id.
    std::sort(chars.begin(), chars.end(),
              [](const Character& a, const Character& b) {
                  return a.id < b.id;
              });
    out.chars_ = std::move(chars);
    res.ok = true;
    return res;
}

const Character* CharacterLibrary::Find(std::string_view id) const {
    for (const Character& c : chars_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

} // namespace Potato::Campaign
