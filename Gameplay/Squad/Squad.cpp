#include "Gameplay/Squad/Squad.h"

#include "Gameplay/Json/Json.h"

#include <cstdint>
#include <set>
#include <utility>

namespace Potato::Gameplay {

namespace {

struct TokenDef {
    const char* token;
    int value;
};

constexpr TokenDef UNIT_TOKENS[] = {
    {"infantry", static_cast<int>(UnitType::Infantry)},
    {"cavalry", static_cast<int>(UnitType::Cavalry)},
    {"artillery", static_cast<int>(UnitType::Artillery)},
    {"engineers", static_cast<int>(UnitType::Engineers)},
    {"scouts", static_cast<int>(UnitType::Scouts)},
    {"militia", static_cast<int>(UnitType::Militia)},
};
constexpr TokenDef SPEED_TOKENS[] = {
    {"slow", static_cast<int>(Speed::Slow)},
    {"medium", static_cast<int>(Speed::Medium)},
    {"fast", static_cast<int>(Speed::Fast)},
    {"very_fast", static_cast<int>(Speed::VeryFast)},
};

template <std::size_t N>
Result<int> DecodeToken(const JsonValue& field,
                        const TokenDef (&table)[N],
                        const char* fieldName,
                        const std::string& squadId) {
    if (field.IsNull() || !field.IsString()) {
        return Fail<int>("squad", "squad '" + squadId + "': '" + fieldName +
                                  "' must be a string token");
    }
    const std::string& tok = field.AsString();
    for (const TokenDef& def : table) {
        if (tok == def.token) return Ok<int>(def.value);
    }
    return Fail<int>("squad", "squad '" + squadId + "': unknown " + fieldName +
                              " token '" + tok + "'");
}

Result<int> ReadInt(const JsonValue& obj, const char* key, int fallback,
                    int min, int max, const std::string& squadId) {
    const JsonValue& f = obj[key];
    if (f.IsNull()) return Ok<int>(fallback);
    if (!f.IsInt()) {
        return Fail<int>("squad", "squad '" + squadId + "': '" + key +
                                  "' must be an integer");
    }
    const std::int64_t v = f.AsInt();
    if (v < min || v > max) {
        return Fail<int>("squad", "squad '" + squadId + "': '" + key +
                                  "' out of range");
    }
    return Ok<int>(static_cast<int>(v));
}

} // namespace

// --- Squad runtime -------------------------------------------------------

bool Squad::ApplyEvent(SquadEvent ev) {
    if (!CanTransition(state, ev)) return false;
    state = TargetOf(ev);
    // Leaving the field (rout or destruction) cancels any in-flight
    // move — every path into these states clears edge state here.
    if (state == SquadState::Routing || state == SquadState::Destroyed) {
        edgeTarget = NO_REGION;
        edgeProgress = 0;
    }
    return true;
}

bool Squad::IssueMove(std::size_t target) {
    if (target == NO_REGION || target == regionIndex) return false;
    if (!ApplyEvent(SquadEvent::MoveOrder)) return false;
    edgeTarget = target;
    edgeProgress = 0;
    return true;
}

void Squad::TickMove() {
    if (state != SquadState::Moving) return;
    if (++edgeProgress >= TicksForEdge(speedMilli)) {
        regionIndex = edgeTarget;
        edgeTarget = NO_REGION;
        edgeProgress = 0;
        ApplyEvent(SquadEvent::Arrived); // Moving -> Holding per the table
    }
}

void Squad::ApplyHit(int hpLoss, int cohesionLoss) {
    if (state == SquadState::Routed || state == SquadState::Destroyed) return;
    if (hpLoss < 0) hpLoss = 0;
    if (cohesionLoss < 0) cohesionLoss = 0;

    hp -= hpLoss;
    if (hp < 0) hp = 0;
    if (hp > maxHp) hp = maxHp;
    cohesion -= cohesionLoss;
    if (cohesion < 0) cohesion = 0;
    if (cohesion > 100) cohesion = 100;

    if (hp == 0) {
        ApplyEvent(SquadEvent::HpZero);
        return;
    }
    if (cohesion < ROUT_THRESHOLD) {
        ApplyEvent(SquadEvent::CohesionBreak);
    }
}

void Squad::RestoreCohesion(int amount) {
    if (amount < 0) return; // damage goes through ApplyHit + the table
    if (state == SquadState::Routed || state == SquadState::Destroyed) return;
    // int64 intermediate: saturation, no signed overflow.
    const std::int64_t sum = static_cast<std::int64_t>(cohesion) + amount;
    cohesion = sum > 100 ? 100 : static_cast<int>(sum);
    // Deliberate: restoring cohesion never un-routs — rout is a morale
    // collapse, sticky for the battle (see header).
}

void Squad::Heal(int amount) {
    if (amount < 0) return; // damage goes through ApplyHit + the table
    if (state == SquadState::Routed || state == SquadState::Destroyed) return;
    const std::int64_t sum = static_cast<std::int64_t>(hp) + amount;
    hp = sum > maxHp ? maxHp : static_cast<int>(sum);
}

Squad Squad::Instantiate(const SquadTemplate& t, std::size_t regionIndex,
                         int side) {
    Squad s;
    s.id = t.id;
    s.name = t.name;
    s.unit = t.unit;
    s.maxHp = t.hp < 1 ? 1 : t.hp;   // hand-built templates aren't gated
    s.hp = s.maxHp;
    s.attack = t.attack < 0 ? 0 : t.attack;
    s.speedMilli = SpeedMilli(t.speed);
    s.cohesion = t.cohesion < 0 ? 0 : (t.cohesion > 100 ? 100 : t.cohesion);
    s.cost = t.cost < 0 ? 0 : t.cost;
    s.side = side;
    s.regionIndex = regionIndex;
    // A template already below the rout threshold spawns broken —
    // Holding with cohesion < 20 would be an incoherent zombie state.
    s.state = s.cohesion < ROUT_THRESHOLD ? SquadState::Routing
                                          : SquadState::Holding;
    return s;
}

// --- SquadTemplateLibrary -------------------------------------------------

Result<SquadTemplateLibrary> SquadTemplateLibrary::Load(std::string_view path) {
    auto doc = Json::Load(path, "potato.squad/1");
    if (!doc.ok()) return Fail<SquadTemplateLibrary>(doc.error, doc.reason);
    return FromJson(doc.value);
}

Result<SquadTemplateLibrary> SquadTemplateLibrary::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Fail<SquadTemplateLibrary>("squad", "root is not an object");
    }
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != "potato.squad/1") {
            return Fail<SquadTemplateLibrary>("schema", "expected potato.squad/1");
        }
    }

    const JsonValue& squads = root["squads"];
    if (!squads.IsArray() || squads.Items().empty()) {
        return Fail<SquadTemplateLibrary>("squad", "'squads' must be a non-empty array");
    }

    SquadTemplateLibrary lib;
    std::set<std::string, std::less<>> seen;
    for (const JsonValue& js : squads.Items()) {
        if (!js.IsObject()) {
            return Fail<SquadTemplateLibrary>("squad", "squad entry is not an object");
        }
        if (js.Has("id") && !js["id"].IsString()) {
            return Fail<SquadTemplateLibrary>("squad", "squad 'id' must be a string");
        }
        const std::string* sid = js.FindString("id");
        if (sid == nullptr || sid->empty()) {
            return Fail<SquadTemplateLibrary>("squad", "squad missing string 'id'");
        }
        if (seen.count(*sid) != 0) {
            return Fail<SquadTemplateLibrary>("squad", "duplicate squad id '" + *sid + "'");
        }
        seen.insert(*sid);

        SquadTemplate t;
        t.id = *sid;
        if (js.Has("name") && !js["name"].IsString()) {
            return Fail<SquadTemplateLibrary>(
                "squad", "squad '" + t.id + "': 'name' must be a string");
        }
        if (const std::string* nm = js.FindString("name")) t.name = *nm;

        auto unit = DecodeToken(js["unit"], UNIT_TOKENS, "unit", t.id);
        if (!unit.ok()) return Fail<SquadTemplateLibrary>(unit.error, unit.reason);
        t.unit = static_cast<UnitType>(unit.value);

        auto speed = DecodeToken(js["speed"], SPEED_TOKENS, "speed", t.id);
        if (!speed.ok()) return Fail<SquadTemplateLibrary>(speed.error, speed.reason);
        t.speed = static_cast<Speed>(speed.value);

        auto hp = ReadInt(js, "hp", -1, 1, 100000, t.id);
        if (!hp.ok()) return Fail<SquadTemplateLibrary>(hp.error, hp.reason);
        if (hp.value < 0) {
            return Fail<SquadTemplateLibrary>("squad", "squad '" + t.id +
                                              "': missing 'hp'");
        }
        t.hp = hp.value;

        auto attack = ReadInt(js, "attack", 0, 0, 100000, t.id);
        if (!attack.ok()) return Fail<SquadTemplateLibrary>(attack.error, attack.reason);
        t.attack = attack.value;

        auto cost = ReadInt(js, "cost", 0, 0, 1000000, t.id);
        if (!cost.ok()) return Fail<SquadTemplateLibrary>(cost.error, cost.reason);
        t.cost = cost.value;

        auto cohesion = ReadInt(js, "cohesion", 100, 0, 100, t.id);
        if (!cohesion.ok()) return Fail<SquadTemplateLibrary>(cohesion.error, cohesion.reason);
        t.cohesion = cohesion.value;

        lib.templates_.push_back(std::move(t));
    }
    return Ok(std::move(lib));
}

const SquadTemplate* SquadTemplateLibrary::Find(std::string_view id) const {
    for (const SquadTemplate& t : templates_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

} // namespace Potato::Gameplay
