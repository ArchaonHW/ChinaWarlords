#include "Gameplay/Record/ReplayVerifier.h"

#include "Gameplay/Eval/WinEval.h"
#include "Gameplay/Fog/QuantumFog.h"
#include "Gameplay/Json/Json.h"
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Myth/Infiltration.h"
#include "Gameplay/Plan/BattlePlan.h"
#include "Gameplay/Record/BattleRecorder.h"
#include "Gameplay/Sim/BattleController.h"
#include "Gameplay/Squad/Squad.h"

#include <string>
#include <utility>
#include <vector>

namespace Potato::Gameplay::Replay {

namespace {

// Verification verdicts (not errors): the record was parseable but
// failed a check — ok=false carries the reason. Result-level Fail is
// reserved for inputs that can't be read as records at all (VerifyFile
// io/parse errors).
Result<VerifyResult> Reject(std::string reason) {
    VerifyResult r;
    r.reason = std::move(reason);
    return Ok(r);
}

VerifyResult Tampered() {
    VerifyResult r;
    r.tampered = true;
    r.reason = "integrity root mismatch — record altered";
    return r;
}

// Same check on every field of one event pair; -1 = equal, else field
// name for the reason string.
const char* EventDiff(const SimEvent& a, const SimEvent& b) {
    if (a.kind != b.kind)             return "kind";
    if (a.tick != b.tick)             return "tick";
    if (a.squadIndex != b.squadIndex) return "squadIndex";
    if (a.slotIndex != b.slotIndex)   return "slotIndex";
    if (a.param != b.param)           return "param";
    if (a.aux != b.aux)               return "aux";
    if (a.side != b.side)             return "side";
    if (a.path != b.path)             return "path";
    if (a.cardId != b.cardId)         return "cardId";
    return nullptr;
}

// Re-issue a recorded Intervention through the live API — the record's
// commands get the same validation as live play.
bool ReIssue(BattleController& bc, const SimEvent& e) {
    const auto kind = static_cast<InterventionKind>(e.aux);
    Result<bool> r = Fail<bool>("command", "unissued");
    switch (kind) {
        case InterventionKind::Redirect:
            r = bc.IssueRedirect(e.side, e.squadIndex,
                                 static_cast<std::size_t>(e.param));
            break;
        case InterventionKind::Override:
            r = bc.IssueOverride(e.side, e.squadIndex, e.param);
            break;
        case InterventionKind::Retreat:
            r = bc.IssueRetreat(e.side, e.squadIndex);
            break;
        case InterventionKind::Probe:
            r = bc.IssueProbe(e.side, static_cast<std::size_t>(e.param));
            break;
        case InterventionKind::Entangle:
            r = bc.IssueEntangle(e.side, e.squadIndex, e.param);
            break;
        case InterventionKind::Replan:
            r = bc.IssueReplan(e.side, e.squadIndex, e.path);
            break;
        case InterventionKind::Execute:
            r = bc.IssueExecute(e.side, e.squadIndex);
            break;
        case InterventionKind::MythPacify:
            r = bc.IssueMythPacify(e.side,
                                   static_cast<std::size_t>(e.param));
            break;
        case InterventionKind::MythPossession:
            r = bc.IssueMythPossession(e.side, e.squadIndex);
            break;
        case InterventionKind::MythGhostArmy:
            r = bc.IssueGhostArmy(e.side,
                                  static_cast<std::size_t>(e.param));
            break;
    }
    return r.ok() && r.value;
}

// Field must be an int within [lo, hi] — no silent coercion/narrowing.
bool FitInt(const JsonValue& j, std::string_view key,
            std::int64_t lo, std::int64_t hi, int& dst) {
    const JsonValue& v = j[key];
    if (!v.IsInt()) return false;
    const std::int64_t n = v.AsInt();
    if (n < lo || n > hi) return false;
    dst = static_cast<int>(n);
    return true;
}
constexpr std::int64_t I32MAX = 2147483647;

Result<bool> TemplateFromJson(const JsonValue& j, SquadTemplate& t) {
    if (!j.IsObject() || !j["id"].IsString() || !j["name"].IsString() ||
        !j["unit"].IsInt() || !j["hp"].IsInt() || !j["attack"].IsInt() ||
        !j["speed"].IsInt() || !j["cohesion"].IsInt() ||
        !j["cost"].IsInt()) {
        return Fail<bool>("record", "malformed deploy template");
    }
    t.id = j["id"].AsString();
    t.name = j["name"].AsString();
    const auto unit = j["unit"].AsInt();
    const auto speed = j["speed"].AsInt();
    if (unit < 0 || unit > 5 || speed < 0 || speed > 3) {
        return Fail<bool>("record", "template enum out of range");
    }
    int hp, attack, cohesion, cost;
    if (!FitInt(j, "hp", -I32MAX - 1, I32MAX, hp) ||
        !FitInt(j, "attack", -I32MAX - 1, I32MAX, attack) ||
        !FitInt(j, "cohesion", -I32MAX - 1, I32MAX, cohesion) ||
        !FitInt(j, "cost", -I32MAX - 1, I32MAX, cost)) {
        return Fail<bool>("record", "template stat out of range");
    }
    t.unit = static_cast<UnitType>(unit);
    t.speed = static_cast<Speed>(speed);
    t.hp = hp;
    t.attack = attack;
    t.cohesion = cohesion;
    t.cost = cost;
    return Ok(true);
}

} // namespace

Result<VerifyResult> Verify(const JsonValue& doc) {
    if (!doc.IsObject()) return Reject("record root is not an object");

    // 1) Schema gate — the /1 family is versioned by toolVersion.
    const std::string* schema = doc.FindString("schema");
    if (schema == nullptr) return Reject("missing schema");
    if (*schema != std::string(BattleRecorder::SCHEMA)) {
        return Reject("unsupported schema: " + *schema);
    }
    // Strict field typing — accessor fallbacks would silently coerce a
    // forged field ("seed":"potato" → 0) into a verifying record.
    if (!doc["seed"].IsInt() || !doc["toolVersion"].IsInt() ||
        !doc["inputs"].IsArray() || !doc["events"].IsArray() ||
        !doc["endTick"].IsInt() || !doc["endBeat"].IsInt() ||
        !doc["winner"].IsInt() || !doc["closeReason"].IsInt() ||
        !doc["checksum"].IsInt() || !doc["forced"].IsBool() ||
        !doc["stalemate"].IsBool()) {
        return Reject("missing or mistyped record field");
    }
    // Size caps — a forged record can't make verification unbounded in
    // the input/event axes either (tick count is capped below).
    if (doc["inputs"].Size() > 1024 || doc["events"].Size() > 1000000) {
        return Reject("record exceeds size caps");
    }
    VerifyResult result;
    const std::int64_t tool = doc["toolVersion"].AsInt();
    if (tool < 0 || tool > BattleRecorder::TOOL_VERSION) {
        return Reject("unsupported toolVersion");
    }
    if (tool < BattleRecorder::TOOL_VERSION) result.downgrade = true;

    // 2) Integrity: recompute root over everything except `integrity`.
    if (!doc.Has("integrity") || !doc["integrity"].IsObject() ||
        !doc["integrity"]["root"].IsInt()) {
        return Reject("missing integrity root");
    }
    const std::uint64_t storedRoot = static_cast<std::uint64_t>(
        doc["integrity"]["root"].AsInt());
    JsonValue::Object payload = doc.Members();
    payload.erase("integrity");
    const std::uint64_t actual =
        BattleRecorder::ComputeRoot(JsonValue::MakeObject(std::move(payload)));
    if (actual != storedRoot) return Ok(Tampered());

    // 3) Rebuild the sim from embedded docs + seed.
    auto map = BattleMap::FromJson(doc["map"]);
    if (!map.ok()) {
        return Reject("embedded map doc rejected: " + map.reason);
    }
    auto cards = DoctrineLibrary::FromJson(doc["cards"]);
    if (!cards.ok()) {
        return Reject("embedded cards doc rejected: " + cards.reason);
    }
    FogConfig fogCfg;
    PlanConfig planCfg;
    EvalConfig evalCfg;
    if (doc.Has("balance")) {
        auto fc = FogConfig::FromJson(doc["balance"]);
        auto pc = PlanConfig::FromJson(doc["balance"]);
        auto ec = EvalConfig::FromJson(doc["balance"]);
        if (!fc.ok() || !pc.ok() || !ec.ok()) {
            return Reject("embedded balance doc rejected");
        }
        fogCfg = fc.value;
        planCfg = pc.value;
        evalCfg = ec.value;
    }
    const auto seed = doc["seed"].AsInt();
    BattleController bc(static_cast<std::uint64_t>(seed), map.value,
                        cards.value, fogCfg, planCfg, evalCfg);

    // 4) Planning inputs — replay the op stream in order.
    for (const JsonValue& op : doc["inputs"].Items()) {
        const std::string* name = op.FindString("op");
        if (name == nullptr) return Reject("input op missing kind");
        if (*name == "deploy") {
            SquadTemplate t;
            int region, side;
            auto okT = TemplateFromJson(op["template"], t);
            if (!okT.ok()) return Reject(okT.reason);
            if (!FitInt(op, "region", 0, I32MAX, region) ||
                !FitInt(op, "side", 0, 1, side)) {
                return Reject("malformed deploy op");
            }
            if (!bc.DeploySquad(t, static_cast<std::size_t>(region),
                                side)) {
                return Reject("recorded deploy rejected on replay");
            }
        } else if (*name == "sheet") {
            int squad;
            if (!FitInt(op, "squad", 0, I32MAX, squad) ||
                !op["cards"].IsArray()) {
                return Reject("malformed sheet op");
            }
            std::vector<std::string> ids;
            for (const JsonValue& c : op["cards"].Items()) {
                if (!c.IsString()) return Reject("malformed sheet op");
                ids.push_back(c.AsString());
            }
            auto sheet = SquadSheet::Build(cards.value, ids);
            if (!sheet.ok() ||
                !bc.SetSheet(static_cast<std::size_t>(squad),
                             std::move(sheet.value))) {
                return Reject("recorded sheet rejected on replay");
            }
        } else if (*name == "arrow") {
            int squad, side;
            if (!FitInt(op, "squad", 0, I32MAX, squad) ||
                !FitInt(op, "side", 0, 1, side) ||
                !op["path"].IsArray()) {
                return Reject("malformed arrow op");
            }
            std::vector<std::size_t> path;
            for (const JsonValue& r : op["path"].Items()) {
                if (!r.IsInt() || r.AsInt() < 0 || r.AsInt() > I32MAX) {
                    return Reject("malformed arrow path element");
                }
                path.push_back(static_cast<std::size_t>(r.AsInt()));
            }
            if (!bc.DrawArrow(side, squad, path)) {
                return Reject("recorded arrow rejected on replay");
            }
        } else if (*name == "intel") {
            int fogSide, squad, region, certainty;
            if (!FitInt(op, "fogSide", 0, 1, fogSide) ||
                !FitInt(op, "squad", 0, I32MAX, squad) ||
                !FitInt(op, "region", 0, I32MAX, region) ||
                !FitInt(op, "certainty", 0, 100, certainty)) {
                return Reject("malformed intel op");
            }
            if (!bc.SetCloudIntel(fogSide, squad, region, certainty)) {
                return Reject("recorded intel rejected on replay");
            }
        } else if (*name == "cp") {
            int side, cp;
            if (!FitInt(op, "side", 0, 1, side) ||
                !FitInt(op, "cp", 0, CP_CAP, cp)) {
                return Reject("malformed cp op");
            }
            if (!bc.SetCpPool(side, cp)) {
                return Reject("recorded cp op rejected on replay");
            }
        } else if (*name == "convoy") {
            int side;
            if (!FitInt(op, "side", 0, 1, side) ||
                !op["path"].IsArray()) {
                return Reject("malformed convoy op");
            }
            std::vector<std::size_t> path;
            for (const JsonValue& r : op["path"].Items()) {
                if (!r.IsInt() || r.AsInt() < 0 || r.AsInt() > I32MAX) {
                    return Reject("malformed convoy path element");
                }
                path.push_back(static_cast<std::size_t>(r.AsInt()));
            }
            if (!bc.SpawnConvoy(side, std::move(path))) {
                return Reject("recorded convoy rejected on replay");
            }
        } else if (*name == "mythseed") {
            int region, level;
            if (!FitInt(op, "region", 0, I32MAX, region) ||
                !FitInt(op, "level", 0,
                        static_cast<int>(kInfiltrationLevelCount) - 1,
                        level)) {
                return Reject("malformed mythseed op");
            }
            if (!bc.SeedInfiltration(
                    static_cast<std::size_t>(region), level)) {
                return Reject("recorded mythseed rejected on replay");
            }
        } else if (*name == "myth") {
            int region, kind;
            if (!FitInt(op, "region", 0, I32MAX, region) ||
                !FitInt(op, "kind", 0,
                        static_cast<int>(kMythEventKindCount) - 1,
                        kind)) {
                return Reject("malformed myth op");
            }
            MythEventKind mk;
            if (!MythEventKindFromInt(kind, mk) ||
                !bc.ApplyMythEvent(static_cast<std::size_t>(region),
                                   mk)) {
                return Reject("recorded myth rejected on replay");
            }
        } else {
            return Reject("unknown input op: " + *name);
        }
    }

    // 5) Event stream: parse once. Intervention events are the command
    //    journal — extract them in order; everything else is output to
    //    diff against after the run.
    std::vector<SimEvent> recorded;
    std::vector<SimEvent> commands;
    for (const JsonValue& e : doc["events"].Items()) {
        SimEvent parsed;
        if (!EventFromJson(e, parsed)) {
            return Reject("malformed event in record");
        }
        if (parsed.kind == SimEvent::Kind::Intervention) {
            commands.push_back(parsed);
        }
        recorded.push_back(std::move(parsed));
    }
    const std::int64_t endTick = doc["endTick"].AsInt();
    const bool forced = doc["forced"].AsBool();
    if (endTick < 0) return Reject("missing endTick");
    // Records exist for completed battles only — endBeat must be
    // Aftermath(2). An unclosed record can't claim a valid seal.
    if (doc["endBeat"].AsInt() != static_cast<int>(BattleBeat::Aftermath)) {
        return Reject("record does not end in Aftermath");
    }
    // Hard cap — a forged record can't make verification unbounded.
    constexpr std::int64_t MAX_REPLAY_TICKS = 200ll * 60 * TICK_RATE_HZ;
    if (endTick > MAX_REPLAY_TICKS) return Reject("endTick out of range");

    if (!bc.RequestBeat(BattleBeat::Execution)) {
        return Reject("replay could not enter Execution");
    }

    // 6) Tick loop: a command stamped t was issued when TickCount()==t;
    //    re-issue in the same window (between tick t-1's completion and
    //    the Tick() that produces tick-t events — i.e., NOW).
    std::size_t cmdCursor = 0;
    const auto issueDue = [&]() -> Result<bool> {
        while (cmdCursor < commands.size() &&
               commands[cmdCursor].tick <=
                   static_cast<int>(bc.GetSim().TickCount())) {
            if (commands[cmdCursor].tick <
                static_cast<int>(bc.GetSim().TickCount())) {
                return Fail<bool>("record",
                                  "command issued after its tick — "
                                  "replay diverged");
            }
            if (!ReIssue(bc, commands[cmdCursor])) {
                return Fail<bool>("record",
                                  "recorded command rejected on replay");
            }
            ++cmdCursor;
        }
        return Ok(true);
    };

    while (bc.Beat() == BattleBeat::Execution &&
           bc.GetSim().TickCount() < static_cast<std::uint64_t>(endTick)) {
        auto okI = issueDue();
        if (!okI.ok()) return Reject(okI.reason);
        if (!bc.Tick()) break;
    }
    // Commands issued in the close window (TickCount==endTick, before
    // the host requested Aftermath) still replay — issue them now.
    auto okI = issueDue();
    if (!okI.ok()) return Reject(okI.reason);
    if (forced && bc.Beat() == BattleBeat::Execution &&
        !bc.RequestBeat(BattleBeat::Aftermath)) {
        return Reject("forced close did not reproduce");
    }

    // 7) Bit-exact comparison.
    const std::vector<SimEvent>& replayed = bc.Events();
    if (replayed.size() != recorded.size()) {
        return Reject("event count differs (recorded " +
                      std::to_string(recorded.size()) + ", replayed " +
                      std::to_string(replayed.size()) + ")");
    }
    for (std::size_t i = 0; i < recorded.size(); ++i) {
        if (const char* f = EventDiff(recorded[i], replayed[i])) {
            return Reject("event #" + std::to_string(i) +
                          " differs in " + f);
        }
    }
    if (bc.Beat() != BattleBeat::Aftermath) {
        return Reject("replay did not reach Aftermath");
    }
    if (bc.Checksum() !=
        static_cast<std::uint64_t>(doc["checksum"].AsInt())) {
        return Reject("checksum mismatch");
    }
    const BattleResult& o = bc.Outcome();
    if (o.winnerSide != doc["winner"].AsInt(-2) ||
        o.forced != forced || o.stalemate != doc["stalemate"].AsBool() ||
        static_cast<int>(o.closeReason) != doc["closeReason"].AsInt(-1) ||
        static_cast<std::int64_t>(o.elapsedTicks) != endTick) {
        return Reject("outcome mismatch");
    }
    result.ok = true;
    result.checksum = bc.Checksum();
    return Ok(result);
}

Result<VerifyResult> VerifyFile(std::string_view path) {
    auto doc = Json::Load(path, BattleRecorder::SCHEMA);
    if (!doc.ok()) {
        return Fail<VerifyResult>(doc.error, doc.reason);
    }
    return Verify(doc.value);
}

} // namespace Potato::Gameplay::Replay
