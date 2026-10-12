#include "Gameplay/Doctrine/Doctrine.h"

#include "Gameplay/Fog/QuantumFog.h"
#include "Gameplay/Json/Json.h"
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Squad/Squad.h"

#include <cassert>
#include <set>
#include <utility>

namespace Potato::Gameplay {

namespace {

// needsParam: the clause is meaningless without a param — reject if absent.
// pmin/pmax bound the param at load (spec: "params within table-declared
// ranges"); they also keep int64 params inside int-safe territory.
struct TokenDef {
    const char* token;
    int value;
    bool needsParam;
    std::int64_t pmin;
    std::int64_t pmax;
};

constexpr TokenDef TRIGGER_TOKENS[] = {
    {"always", static_cast<int>(TriggerKind::Always), false, 0, 0},
    {"cohesion_below", static_cast<int>(TriggerKind::CohesionBelow), true, 0, 100},
    {"enemy_in_region", static_cast<int>(TriggerKind::EnemyInRegion), false, 0, 0},
    {"enemy_adjacent", static_cast<int>(TriggerKind::EnemyAdjacent), false, 0, 0},
};
constexpr TokenDef CONDITION_TOKENS[] = {
    {"always", static_cast<int>(ConditionKind::Always), false, 0, 0},
    {"cohesion_above", static_cast<int>(ConditionKind::CohesionAbove), true, 0, 100},
    {"cohesion_below", static_cast<int>(ConditionKind::CohesionBelow), true, 0, 100},
    {"cp_at_least", static_cast<int>(ConditionKind::CpAtLeast), true, 0, 100000},
};
constexpr TokenDef ACTION_TOKENS[] = {
    {"hold", static_cast<int>(ActionKind::Hold), false, 0, 0},
    {"brace", static_cast<int>(ActionKind::Brace), true, 1, 100},
    // move param is a region index — non-negative here; bounds+adjacency
    // are checked against the BattleMap at eval time (cards are map-agnostic).
    {"move", static_cast<int>(ActionKind::Move), true, 0, 2147483647ll},
    {"retreat", static_cast<int>(ActionKind::Retreat), false, 0, 0},
    // burn: the authored atrocity — no param; the target is always
    // the village the firing squad stands on.
    {"burn", static_cast<int>(ActionKind::Burn), false, 0, 0},
};
constexpr TokenDef MODIFIER_TOKENS[] = {
    {"none", static_cast<int>(ModifierKind::None), false, 0, 0},
    {"attack_boost", static_cast<int>(ModifierKind::AttackBoost), true, -10000, 10000},
};

const TokenDef* LookupToken(const std::string& tok, const TokenDef* table,
                            std::size_t tableN) {
    for (std::size_t i = 0; i < tableN; ++i) {
        if (tok == table[i].token) return &table[i];
    }
    return nullptr;
}

// Read a {"type": token, "param": int} clause. Absent/null object → the
// default kind (Always/None per table). kindName = "trigger" etc.
Result<bool> ReadClause(const JsonValue& obj, const char* key,
                        const TokenDef* table, std::size_t tableN,
                        int missingValue,
                        const std::string& cardId,
                        int& outKind, std::int64_t& outParam) {
    const JsonValue& clause = obj[key];
    if (clause.IsNull()) {
        outKind = missingValue;
        outParam = 0;
        return Ok(true);
    }
    if (!clause.IsObject()) {
        return Fail<bool>("doctrine", "card '" + cardId + "': '" + key +
                                      "' must be an object");
    }
    const std::string* type = clause.FindString("type");
    if (type == nullptr) {
        return Fail<bool>("doctrine", "card '" + cardId + "': '" + key +
                                      (clause.Has("type")
                                           ? ".type' must be a string"
                                           : ".type' missing"));
    }
    const TokenDef* def = LookupToken(*type, table, tableN);
    if (def == nullptr) {
        return Fail<bool>("doctrine", "card '" + cardId + "': unknown '" +
                                      key + "' type '" + *type + "'");
    }
    outKind = def->value;
    const JsonValue& p = clause["param"];
    if (!p.IsNull()) {
        if (!p.IsInt()) {
            return Fail<bool>("doctrine", "card '" + cardId + "': '" + key +
                                          ".param' must be an integer");
        }
        if (p.AsInt() < def->pmin || p.AsInt() > def->pmax) {
            return Fail<bool>("doctrine",
                              "card '" + cardId + "': '" + key +
                                  ".param' out of range [" +
                                  std::to_string(def->pmin) + "," +
                                  std::to_string(def->pmax) + "]");
        }
        outParam = p.AsInt();
    } else if (def->needsParam) {
        return Fail<bool>("doctrine", "card '" + cardId + "': '" + key +
                                          "' requires an int 'param'");
    } else {
        outParam = 0;
    }
    return Ok(true);
}

// Enemy-detection reads the ACTING SIDE's fog — belief, not truth
// (truth boundary: this file must never scan hostile squads for these
// triggers). A stale cloud can fire a card where no enemy stands, and
// a decayed/misplaced cloud hides a real one — that is the mechanic.
int FogIndex(int side) { return side == 1 ? 1 : 0; }

bool EnemyVisibleInRegion(const QuantumFog& fog, std::size_t region) {
    if (region == Squad::NO_REGION) return false; // off-field co-locate
    return fog.VisibleAt(region) > 0;
}

bool EnemyVisibleAdjacent(const BattleMap& map, const QuantumFog& fog,
                          std::size_t region) {
    if (region >= map.RegionCount()) return false;
    for (std::size_t n : map.Neighbors(region)) {
        if (fog.VisibleAt(n) > 0) return true;
    }
    return false;
}

} // namespace

// --- SquadSheet ------------------------------------------------------------

Result<SquadSheet> SquadSheet::Build(const DoctrineLibrary& cards,
                                     const std::vector<std::string>& cardIds) {
    if (cardIds.size() < MIN_SLOTS || cardIds.size() > MAX_SLOTS) {
        return Fail<SquadSheet>(
            "doctrine", "sheet needs " + std::to_string(MIN_SLOTS) + "-" +
                        std::to_string(MAX_SLOTS) + " cards, got " +
                        std::to_string(cardIds.size()));
    }
    SquadSheet sheet;
    for (const std::string& id : cardIds) {
        const std::size_t idx = cards.IndexOf(id);
        if (idx == cards.Count()) {
            return Fail<SquadSheet>("doctrine", "unknown card id '" + id + "'");
        }
        sheet.slots.push_back(CardSlot{idx, 0});
    }
    return Ok(std::move(sheet));
}

// --- DoctrineLibrary --------------------------------------------------------

Result<DoctrineLibrary> DoctrineLibrary::Load(std::string_view path) {
    auto doc = Json::Load(path, "potato.doctrine_cards/1");
    if (!doc.ok()) return Fail<DoctrineLibrary>(doc.error, doc.reason);
    return FromJson(doc.value);
}

Result<DoctrineLibrary> DoctrineLibrary::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Fail<DoctrineLibrary>("doctrine", "root is not an object");
    }
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != "potato.doctrine_cards/1") {
            return Fail<DoctrineLibrary>("schema",
                                         "expected potato.doctrine_cards/1");
        }
    }
    const JsonValue& cards = root["cards"];
    if (!cards.IsArray() || cards.Items().empty()) {
        return Fail<DoctrineLibrary>("doctrine",
                                     "'cards' must be a non-empty array");
    }

    DoctrineLibrary lib;
    std::set<std::string, std::less<>> seen;
    for (const JsonValue& jc : cards.Items()) {
        if (!jc.IsObject()) {
            return Fail<DoctrineLibrary>("doctrine", "card entry is not an object");
        }
        if (jc.Has("id") && !jc["id"].IsString()) {
            return Fail<DoctrineLibrary>("doctrine", "card 'id' must be a string");
        }
        const std::string* cid = jc.FindString("id");
        if (cid == nullptr || cid->empty()) {
            return Fail<DoctrineLibrary>("doctrine", "card missing string 'id'");
        }
        if (!seen.insert(*cid).second) {
            return Fail<DoctrineLibrary>("doctrine", "duplicate card id '" + *cid + "'");
        }

        DoctrineCard c;
        c.id = *cid;
        if (jc.Has("name") && !jc["name"].IsString()) {
            return Fail<DoctrineLibrary>(
                "doctrine", "card '" + c.id + "': 'name' must be a string");
        }
        if (const std::string* nm = jc.FindString("name")) c.name = *nm;

        int kind = 0; std::int64_t param = 0;
        auto trig = ReadClause(jc, "trigger", TRIGGER_TOKENS,
                               sizeof(TRIGGER_TOKENS) / sizeof(TokenDef),
                               static_cast<int>(TriggerKind::Always),
                               c.id, kind, param);
        if (!trig.ok()) return Fail<DoctrineLibrary>(trig.error, trig.reason);
        c.trigger = static_cast<TriggerKind>(kind);
        c.triggerParam = param;

        auto cond = ReadClause(jc, "condition", CONDITION_TOKENS,
                               sizeof(CONDITION_TOKENS) / sizeof(TokenDef),
                               static_cast<int>(ConditionKind::Always),
                               c.id, kind, param);
        if (!cond.ok()) return Fail<DoctrineLibrary>(cond.error, cond.reason);
        c.condition = static_cast<ConditionKind>(kind);
        c.conditionParam = param;

        auto act = ReadClause(jc, "action", ACTION_TOKENS,
                              sizeof(ACTION_TOKENS) / sizeof(TokenDef),
                              static_cast<int>(ActionKind::Hold),
                              c.id, kind, param);
        if (!act.ok()) return Fail<DoctrineLibrary>(act.error, act.reason);
        c.action = static_cast<ActionKind>(kind);
        c.actionParam = param;

        auto mod = ReadClause(jc, "modifier", MODIFIER_TOKENS,
                              sizeof(MODIFIER_TOKENS) / sizeof(TokenDef),
                              static_cast<int>(ModifierKind::None),
                              c.id, kind, param);
        if (!mod.ok()) return Fail<DoctrineLibrary>(mod.error, mod.reason);
        c.modifier = static_cast<ModifierKind>(kind);
        c.modifierParam = param;

        const JsonValue& cd = jc["cooldown"];
        if (cd.IsNull()) {
            c.cooldownTicks = 5 * TICK_RATE_HZ;
        } else if (!cd.IsInt() || cd.AsInt() < 5 || cd.AsInt() > 60) {
            return Fail<DoctrineLibrary>(
                "doctrine", "card '" + c.id + "': 'cooldown' must be int 5-60 (s)");
        } else {
            c.cooldownTicks = static_cast<int>(cd.AsInt()) * TICK_RATE_HZ;
        }

        lib.cards_.push_back(std::move(c));
    }
    return Ok(std::move(lib));
}

const DoctrineCard* DoctrineLibrary::Find(std::string_view id) const {
    for (const DoctrineCard& c : cards_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

// --- Interpreter ------------------------------------------------------------

namespace Doctrine {

EvalOutcome EvalTick(const BattleMap& map,
                     const std::vector<Squad>& snapshot,
                     std::vector<SquadSheet>& sheets,
                     const DoctrineLibrary& cards,
                     Prng& rng,
                     const std::array<int, 2>& cpPools,
                     const std::array<QuantumFog, 2>& fog,
                     int tick) {
    (void)rng; // reserved for future draws — canonical order contract holds
    assert(sheets.size() == snapshot.size()); // sheets align by squad index
    EvalOutcome out;

    for (std::size_t si = 0; si < snapshot.size() && si < sheets.size(); ++si) {
        SquadSheet& sheet = sheets[si];
        const Squad& sq = snapshot[si];

        for (std::size_t sl = 0; sl < sheet.slots.size(); ++sl) {
            CardSlot& slot = sheet.slots[sl];
            if (slot.cooldownRemaining > 0) --slot.cooldownRemaining;
            // CP override flag: cleared on visit even if the squad can't
            // act — a stale flag must never fire out of turn.
            const bool forced = slot.forceNext;
            slot.forceNext = false;
            if (!sq.IsEffective() || sq.state == SquadState::Routing) continue;
            if (slot.cooldownRemaining > 0 && !forced) continue;
            if (slot.cardIndex >= cards.Count()) continue;

            const DoctrineCard& card = cards.At(slot.cardIndex);

            // Trigger — reads the tick-start snapshot only; an override
            // bypasses the check entirely.
            bool fired = forced;
            if (!forced) switch (card.trigger) {
                case TriggerKind::Always:
                    fired = true;
                    break;
                case TriggerKind::CohesionBelow:
                    fired = sq.cohesion < card.triggerParam;
                    break;
                case TriggerKind::EnemyInRegion:
                    fired = EnemyVisibleInRegion(fog[FogIndex(sq.side)],
                                                 sq.regionIndex);
                    break;
                case TriggerKind::EnemyAdjacent:
                    fired = EnemyVisibleAdjacent(map,
                                                 fog[FogIndex(sq.side)],
                                                 sq.regionIndex);
                    break;
            }
            if (!fired) continue;

            // Condition gate — an override bypasses trigger AND condition.
            bool pass = true;
            if (!forced) switch (card.condition) {
                case ConditionKind::Always: break;
                case ConditionKind::CohesionAbove:
                    pass = sq.cohesion > card.conditionParam;
                    break;
                case ConditionKind::CohesionBelow:
                    pass = sq.cohesion < card.conditionParam;
                    break;
                case ConditionKind::CpAtLeast: {
                    const int sideIdx = (sq.side == 1) ? 1 : 0;
                    pass = cpPools[sideIdx] >= card.conditionParam;
                    break;
                }
            }
            if (!pass) continue;

            // Action + modifier → pending deltas.
            switch (card.action) {
                case ActionKind::Hold:
                    break;
                case ActionKind::Brace:
                    out.deltas.push_back({PendingDelta::Kind::Cohesion,
                                          static_cast<int>(si),
                                          static_cast<int>(card.actionParam),
                                          Squad::NO_REGION});
                    break;
                case ActionKind::Move: {
                    // Card target must be a real, adjacent region — an
                    // authored index that isn't a neighbor would teleport
                    // or corrupt regionIndex downstream (IssueMove treats
                    // adjacency as the caller's job; the interpreter is it).
                    const std::size_t target =
                        static_cast<std::size_t>(card.actionParam);
                    if (target < map.RegionCount() &&
                        sq.regionIndex < map.RegionCount()) {
                        bool adjacent = false;
                        for (std::size_t n : map.Neighbors(sq.regionIndex)) {
                            if (n == target) { adjacent = true; break; }
                        }
                        if (adjacent) {
                            out.deltas.push_back({PendingDelta::Kind::Move,
                                                  static_cast<int>(si), 0,
                                                  target});
                        }
                    }
                    break;
                }
                case ActionKind::Retreat: {
                    std::size_t dest = Squad::NO_REGION;
                    if (sq.regionIndex < map.RegionCount()) {
                        // lowest neighbor INDEX (canonical "map edge" proxy)
                        for (std::size_t n : map.Neighbors(sq.regionIndex)) {
                            if (dest == Squad::NO_REGION || n < dest) dest = n;
                        }
                    }
                    if (dest != Squad::NO_REGION) {
                        out.deltas.push_back({PendingDelta::Kind::Move,
                                              static_cast<int>(si), 0, dest});
                    }
                    break;
                }
                case ActionKind::Burn:
                    // Only emit on village ground — a burn card on open
                    // field is a no-op; the GovernanceField re-validates
                    // at apply (post-delta state, burned flag).
                    if (sq.regionIndex < map.RegionCount() &&
                        (map.RegionAt(sq.regionIndex).strategic &
                         STRATEGIC_VILLAGE) != 0) {
                        out.deltas.push_back({PendingDelta::Kind::Burn,
                                              static_cast<int>(si), 0,
                                              Squad::NO_REGION});
                    }
                    break;
            }
            switch (card.modifier) {
                case ModifierKind::None: break;
                case ModifierKind::AttackBoost:
                    out.deltas.push_back({PendingDelta::Kind::Attack,
                                          static_cast<int>(si),
                                          static_cast<int>(card.modifierParam),
                                          Squad::NO_REGION});
                    break;
            }

            slot.cooldownRemaining = card.cooldownTicks;
            out.events.push_back({SimEvent::Kind::CardFired, tick,
                                  static_cast<int>(si),
                                  static_cast<int>(sl), -1, 0,
                                  snapshot[si].side, {}, card.id});
        }
    }
    return out;
}

void ApplyDeltas(std::vector<Squad>& squads,
                 std::span<const PendingDelta> deltas) {
    for (const PendingDelta& d : deltas) {
        if (d.squadIndex < 0 ||
            static_cast<std::size_t>(d.squadIndex) >= squads.size()) continue;
        Squad& sq = squads[static_cast<std::size_t>(d.squadIndex)];
        if (!sq.IsEffective()) continue; // deltas never mutate terminal squads
        switch (d.kind) {
            case PendingDelta::Kind::Cohesion:
                sq.RestoreCohesion(d.amount);
                break;
            case PendingDelta::Kind::Move:
                sq.IssueMove(d.region);
                break;
            case PendingDelta::Kind::Attack: {
                const std::int64_t sum =
                    static_cast<std::int64_t>(sq.attack) + d.amount;
                sq.attack = sum < 0 ? 0
                    : (sum > 2147483647ll ? 2147483647 : static_cast<int>(sum));
                break;
            }
            case PendingDelta::Kind::Burn:
                break; // village state — the GovernanceField owns it
        }
    }
}

} // namespace Doctrine

} // namespace Potato::Gameplay
