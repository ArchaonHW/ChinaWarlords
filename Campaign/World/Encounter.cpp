#include "Campaign/World/Encounter.h"

#include "Campaign/Characters/Character.h"
#include "Campaign/Characters/CommanderBind.h"
#include "Campaign/World/WorldState.h"
#include "Gameplay/Doctrine/Doctrine.h"
#include "Gameplay/Json/Json.h"
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Sim/BattleController.h"
#include "Gameplay/Squad/Squad.h"

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

// Closed field vocabularies — the schema gate convention (12.4):
// authored-but-unknown keys mean the author mistyped, not that the
// file grew a feature.
bool KnownTopKey(std::string_view k) {
    return k == "schema" || k == "id" || k == "node" ||
           k == "trigger" || k == "map" || k == "defenders" ||
           k == "stakes" || k == "seed";
}
bool KnownDefenderKey(std::string_view k) {
    return k == "template" || k == "region" || k == "deck";
}
bool KnownStakeKey(std::string_view k) { return k == "control"; }

bool ReadString(const JsonValue& doc, std::string_view key,
                std::size_t maxLen, std::string& out) {
    const std::string* s = doc.FindString(key);
    if (!s || s->empty() || s->size() > maxLen) return false;
    out = *s;
    return true;
}

// Closed control vocabulary — the DecodeControl table from
// WorldMap.cpp, copied (12 lines; the flag-table duplication
// precedent — the vocabulary is the wire contract of two schemas).
bool ControlFromName(std::string_view s, WorldControl& out) {
    if (s == "neutral") {
        out = WorldControl::Neutral;
        return true;
    }
    if (s == "player") {
        out = WorldControl::Player;
        return true;
    }
    if (s == "rival") {
        out = WorldControl::Rival;
        return true;
    }
    return false;
}

bool TriggerFromName(std::string_view s, EncounterTrigger& out) {
    if (s == "arrival") {
        out = EncounterTrigger::Arrival;
        return true;
    }
    if (s == "proximity") {
        out = EncounterTrigger::Proximity;
        return true;
    }
    if (s == "poi") {
        out = EncounterTrigger::Poi;
        return true;
    }
    return false;
}

// Bitcast the int64 wire seed to the u64 battle seed (MintClaim
// ledger-seal precedent — bit-exact across platforms).
std::uint64_t SeedFromWire(std::int64_t s) {
    std::uint64_t v;
    std::memcpy(&v, &s, sizeof(v));
    return v;
}

Result<EncounterDef> ParseEncounter(const JsonValue& doc) {
    if (!doc.IsObject()) {
        return Gameplay::Fail<EncounterDef>("field",
                                            "encounter not object");
    }
    for (const auto& [k, v] : doc.Members()) {
        (void)v;
        if (!KnownTopKey(k)) {
            return Gameplay::Fail<EncounterDef>(
                "field", "unknown encounter key '" + k + "'");
        }
    }
    EncounterDef e;
    if (!ReadString(doc, "id", EncounterLibrary::MAX_ID_LEN, e.id)) {
        return Gameplay::Fail<EncounterDef>("field",
                                            "missing/bad 'id'");
    }
    if (!ReadString(doc, "node", EncounterLibrary::MAX_ID_LEN,
                    e.node)) {
        return Gameplay::Fail<EncounterDef>("field",
                                            "missing/bad 'node'");
    }
    if (doc.Has("trigger")) {
        const std::string* t = doc.FindString("trigger");
        if (!t || !TriggerFromName(*t, e.trigger)) {
            return Gameplay::Fail<EncounterDef>(
                "field", "trigger must be arrival|proximity|poi");
        }
    }
    if (doc.Has("map")) {
        const std::string* m = doc.FindString("map");
        if (!m || m->empty() ||
            m->size() > EncounterLibrary::MAX_MAP_REF_LEN) {
            return Gameplay::Fail<EncounterDef>("field",
                                                "bad 'map' ref");
        }
        e.map = *m;
    }
    const JsonValue& defs = doc["defenders"];
    if (!defs.IsArray() || defs.Size() == 0 ||
        defs.Size() > EncounterLibrary::MAX_DEFENDERS) {
        return Gameplay::Fail<EncounterDef>(
            "field", "'defenders' must be 1..MAX_DEFENDERS");
    }
    for (const JsonValue& jd : defs.Items()) {
        if (!jd.IsObject()) {
            return Gameplay::Fail<EncounterDef>(
                "field", "defender not object");
        }
        for (const auto& [k, v] : jd.Members()) {
            (void)v;
            if (!KnownDefenderKey(k)) {
                return Gameplay::Fail<EncounterDef>(
                    "field", "unknown defender key '" + k + "'");
            }
        }
        EncounterDefender d;
        if (!ReadString(jd, "template", EncounterLibrary::MAX_ID_LEN,
                        d.tmpl)) {
            return Gameplay::Fail<EncounterDef>(
                "field", "defender missing/bad 'template'");
        }
        const JsonValue& r = jd["region"];
        if (!r.IsInt() || r.AsInt() < 0) {
            return Gameplay::Fail<EncounterDef>(
                "field", "defender 'region' must be int >= 0");
        }
        d.region = r.AsInt();
        const JsonValue& deck = jd["deck"];
        if (!deck.IsArray() ||
            deck.Size() < Gameplay::SquadSheet::MIN_SLOTS ||
            deck.Size() > Gameplay::SquadSheet::MAX_SLOTS) {
            return Gameplay::Fail<EncounterDef>(
                "field", "defender 'deck' must be 3..5 card ids");
        }
        for (const JsonValue& card : deck.Items()) {
            if (!card.IsString() || card.AsString().empty() ||
                card.AsString().size() >
                    EncounterLibrary::MAX_ID_LEN) {
                return Gameplay::Fail<EncounterDef>(
                    "field", "defender deck card id bad");
            }
            d.deck.push_back(card.AsString());
        }
        e.defenders.push_back(std::move(d));
    }
    if (doc.Has("stakes")) {
        const JsonValue& st = doc["stakes"];
        if (!st.IsObject()) {
            return Gameplay::Fail<EncounterDef>(
                "field", "'stakes' must be an object");
        }
        for (const auto& [k, v] : st.Members()) {
            (void)v;
            if (!KnownStakeKey(k)) {
                return Gameplay::Fail<EncounterDef>(
                    "field", "unknown stakes key '" + k + "'");
            }
        }
        if (st.Has("control")) {
            const std::string* c = st.FindString("control");
            if (!c || !ControlFromName(*c, e.controlStake)) {
                return Gameplay::Fail<EncounterDef>(
                    "field", "stakes.control not a control name");
            }
            e.hasControlStake = true;
        }
    }
    if (doc.Has("seed")) {
        const JsonValue& s = doc["seed"];
        if (!s.IsInt()) {
            return Gameplay::Fail<EncounterDef>("field",
                                                "'seed' not int");
        }
        e.hasSeed = true;
        e.seed = SeedFromWire(s.AsInt());
    }
    return Gameplay::Ok(std::move(e));
}

} // namespace

EncounterLoadResult EncounterLibrary::Load(
    const std::filesystem::path& dir, EncounterLibrary& out) {
    EncounterLoadResult res;
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(
             dir, std::filesystem::directory_options::none, ec);
         !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        // Per-entry stat uses its OWN error channel (WorldLibrary
        // precedent).
        std::error_code ec2;
        const bool isFile = it->is_regular_file(ec2);
        if (ec2) {
            res.rejected.push_back({it->path(), "io",
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
        res.reason = "encounter dir unreadable: " + ec.message();
        return res;
    }
    // Deterministic registration order: filename sort.
    std::sort(files.begin(), files.end());
    if (files.size() > MAX_ENCOUNTERS) {
        res.error = "overflow";
        res.reason = "encounter file count exceeds MAX_ENCOUNTERS";
        return res;
    }

    std::vector<EncounterDef> encounters;
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
        Result<EncounterDef> e = ParseEncounter(doc.value);
        if (!e.ok()) {
            res.rejected.push_back({f, e.error, e.reason});
            continue;
        }
        if (ids.count(e.value.id)) {
            res.rejected.push_back(
                {f, "duplicate", "encounter id already registered"});
            continue;
        }
        ids.insert(e.value.id);
        encounters.push_back(std::move(e.value));
    }

    // Canonical order: encounter id.
    std::sort(encounters.begin(), encounters.end(),
              [](const EncounterDef& a, const EncounterDef& b) {
                  return a.id < b.id;
              });
    out.encounters_ = std::move(encounters);
    res.ok = true;
    return res;
}

const EncounterDef* EncounterLibrary::Find(std::string_view id) const {
    for (const EncounterDef& e : encounters_) {
        if (e.id == id) return &e;
    }
    return nullptr;
}

std::vector<const EncounterDef*> PendingEncounters(
    const WorldState& ws, const WorldMap& map,
    const EncounterLibrary& lib) {
    std::vector<const EncounterDef*> out;
    const std::size_t wb = map.NodeIndexOf(ws.WarbandAt());
    for (const EncounterDef& enc : lib.Encounters()) {
        if (ws.IsResolved(enc.id)) continue;
        const std::size_t ni = map.NodeIndexOf(enc.node);
        if (ni == WorldMap::NO_NODE) continue; // node edited out
        bool fire = false;
        switch (enc.trigger) {
        case EncounterTrigger::Arrival:
            fire = (ni == wb);
            break;
        case EncounterTrigger::Proximity:
            // At the warband's own node fires too — you're in the
            // ambush already, not merely near it.
            if (ni == wb) {
                fire = true;
            } else if (wb != WorldMap::NO_NODE) {
                for (const auto& [adj, days] : map.Neighbors(wb)) {
                    (void)days;
                    if (adj == ni) {
                        fire = true;
                        break;
                    }
                }
            }
            break;
        case EncounterTrigger::Poi:
            if (ni == wb) {
                const WorldNode& n = map.NodeAt(ni);
                fire = (n.strategic != 0 || n.myth != 0);
            }
            break;
        }
        if (fire) out.push_back(&enc);
    }
    return out;
}

Result<EncounterAssembly> MarshalEncounter(
    const EncounterDef& enc, const WorldMap& world,
    const Gameplay::BattleMap& battleMap,
    const Gameplay::SquadTemplateLibrary& squads,
    const Gameplay::DoctrineLibrary& cards,
    std::span<const PlayerDeploy> players,
    const Character* commander, std::uint64_t seed) {
    // Every check runs before ANY output row — a marshaled
    // assembly is either complete or it doesn't exist.
    const WorldNode* node = world.FindNode(enc.node);
    if (node == nullptr) {
        return Gameplay::Fail<EncounterAssembly>(
            "marshal", "encounter node not in world");
    }
    std::string mapId = enc.map.empty() ? node->map : enc.map;
    if (mapId.empty()) {
        return Gameplay::Fail<EncounterAssembly>(
            "marshal", "no battle-map ref (enc or node)");
    }
    if (players.empty()) {
        return Gameplay::Fail<EncounterAssembly>(
            "marshal", "no player deployment");
    }

    // Validate + build every sheet first; only then emit rows.
    std::vector<Gameplay::SquadSheet> playerSheets;
    for (const PlayerDeploy& p : players) {
        if (p.tmpl == nullptr) {
            return Gameplay::Fail<EncounterAssembly>(
                "marshal", "player squad template missing");
        }
        if (p.region >= battleMap.RegionCount()) {
            return Gameplay::Fail<EncounterAssembly>(
                "marshal", "player region out of battle map");
        }
        auto sheet = Gameplay::SquadSheet::Build(cards, p.deck);
        if (!sheet.ok()) {
            return Gameplay::Fail<EncounterAssembly>(sheet.error,
                                                     sheet.reason);
        }
        playerSheets.push_back(std::move(sheet.value));
    }
    std::vector<const Gameplay::SquadTemplate*> defTmpls;
    std::vector<Gameplay::SquadSheet> defSheets;
    for (const EncounterDefender& d : enc.defenders) {
        const Gameplay::SquadTemplate* t = squads.Find(d.tmpl);
        if (t == nullptr) {
            return Gameplay::Fail<EncounterAssembly>(
                "marshal", "defender template '" + d.tmpl +
                           "' not in squad library");
        }
        if (d.region < 0 ||
            static_cast<std::size_t>(d.region) >=
                battleMap.RegionCount()) {
            return Gameplay::Fail<EncounterAssembly>(
                "marshal", "defender region out of battle map");
        }
        auto sheet = Gameplay::SquadSheet::Build(cards, d.deck);
        if (!sheet.ok()) {
            return Gameplay::Fail<EncounterAssembly>(sheet.error,
                                                     sheet.reason);
        }
        defTmpls.push_back(t);
        defSheets.push_back(std::move(sheet.value));
    }

    EncounterAssembly a;
    a.enc = &enc;
    a.mapId = std::move(mapId);
    a.seed = enc.hasSeed ? enc.seed : seed;
    if (commander != nullptr) {
        ApplyPrior(a.fog, commander->prior);
    }
    for (std::size_t i = 0; i < players.size(); ++i) {
        DeployRow r;
        r.side = 0;
        r.tmpl = players[i].tmpl;
        r.region = players[i].region;
        r.sheet = std::move(playerSheets[i]);
        r.rosterName = players[i].rosterName;
        a.rows.push_back(std::move(r));
    }
    for (std::size_t i = 0; i < enc.defenders.size(); ++i) {
        DeployRow r;
        r.side = 1;
        r.tmpl = defTmpls[i];
        r.region = static_cast<std::size_t>(enc.defenders[i].region);
        r.sheet = std::move(defSheets[i]);
        a.rows.push_back(std::move(r));
    }
    return Gameplay::Ok(std::move(a));
}

bool DeployAssembly(Gameplay::BattleController& bc,
                    const EncounterAssembly& a) {
    if (bc.Beat() != Gameplay::BattleBeat::Planning) return false;
    for (const DeployRow& r : a.rows) {
        if (!bc.DeploySquad(*r.tmpl, r.region, r.side)) {
            return false;
        }
        if (!bc.SetSheet(bc.Squads().size() - 1, r.sheet)) {
            return false;
        }
    }
    return true;
}

} // namespace Potato::Campaign
