#include "Gameplay/Record/BattleRecorder.h"

#include "Gameplay/Sim/BattleController.h"
#include "Gameplay/Squad/Squad.h"

#include <utility>

namespace Potato::Gameplay {

namespace {

JsonValue PathJson(const std::vector<std::size_t>& path) {
    JsonValue::Array a;
    a.reserve(path.size());
    for (std::size_t r : path) {
        a.push_back(JsonValue::Int(static_cast<std::int64_t>(r)));
    }
    return JsonValue::MakeArray(std::move(a));
}

} // namespace

JsonValue EventToJson(const SimEvent& e) {
    JsonValue::Object o;
    o.emplace("aux", JsonValue::Int(e.aux));
    o.emplace("card", JsonValue::String(e.cardId));
    o.emplace("kind", JsonValue::Int(static_cast<std::int64_t>(e.kind)));
    o.emplace("param", JsonValue::Int(e.param));
    o.emplace("path", PathJson(e.path));
    o.emplace("side", JsonValue::Int(e.side));
    o.emplace("slot", JsonValue::Int(e.slotIndex));
    o.emplace("squad", JsonValue::Int(e.squadIndex));
    o.emplace("tick", JsonValue::Int(e.tick));
    return JsonValue::MakeObject(std::move(o));
}

bool EventFromJson(const JsonValue& j, SimEvent& e) {
    if (!j.IsObject()) return false;
    if (!j["kind"].IsInt() || !j["tick"].IsInt() || !j["squad"].IsInt() ||
        !j["slot"].IsInt() || !j["param"].IsInt() || !j["aux"].IsInt() ||
        !j["side"].IsInt() || !j["path"].IsArray() || !j["card"].IsString()) {
        return false;
    }
    const std::int64_t kind = j["kind"].AsInt();
    // CardFired..MythInvasion — ordinals are append-only, so the
    // last enumerator IS the bound (v3 added governance 4-7 and
    // SquadExecuted 8; v4 adds InfiltrationChanged 9 and
    // ShrineCaptured 10; v5 adds MythActionInvoked 11; v6 adds
    // MythInvasion 12).
    if (kind < 0 ||
        kind > static_cast<std::int64_t>(
                   SimEvent::Kind::MythInvasion)) {
        return false;
    }
    // Every field must fit int32 — silent narrowing would let a forged
    // value verify as a different number than the wire claimed.
    const auto fitInt = [&](const char* key, std::int64_t lo,
                            std::int64_t hi, int& dst) {
        const JsonValue& v = j[key];
        if (!v.IsInt()) return false;
        const std::int64_t n = v.AsInt();
        if (n < lo || n > hi) return false;
        dst = static_cast<int>(n);
        return true;
    };
    constexpr std::int64_t I32MAX = 2147483647;
    SimEvent parsed;
    parsed.kind = static_cast<SimEvent::Kind>(kind);
    if (!fitInt("tick", 0, I32MAX, parsed.tick)) return false;
    if (!fitInt("squad", -1, I32MAX, parsed.squadIndex)) return false;
    if (!fitInt("slot", -1, I32MAX, parsed.slotIndex)) return false;
    if (!fitInt("param", -2147483648, I32MAX, parsed.param)) return false;
    // aux multiplexes by kind: BattleBeat ordinal (BeatChanged,
    // 0..2), InterventionKind ordinal (Intervention, 0..9 — the
    // myth kinds ride 7-9), MythActionKind ordinal
    // (MythActionInvoked, 0..2), CloseReason ordinal
    // (ResultDeclared, 0..2), convoy index (governance kinds,
    // 0..3), InfiltrationLevel (InfiltrationChanged, 0..3),
    // invasion form (MythInvasion, 0..2).
    // Replan's path length rides param, not aux. 9 is the max
    // legitimate value; the replay diff catches any
    // kind-inconsistent value anyway.
    if (!fitInt("aux", 0, 9, parsed.aux)) return false;
    if (!fitInt("side", -1, 1, parsed.side)) return false;
    for (const JsonValue& r : j["path"].Items()) {
        if (!r.IsInt() || r.AsInt() < 0 || r.AsInt() > I32MAX) return false;
        parsed.path.push_back(static_cast<std::size_t>(r.AsInt()));
    }
    parsed.cardId = j["card"].AsString();
    e = std::move(parsed);
    return true;
}

void BattleRecorder::Bind(std::uint64_t seed, const JsonValue& mapDoc,
                          const JsonValue& cardsDoc) {
    *this = BattleRecorder{}; // single-use: reset all per-battle state
    seed_ = seed;
    mapDoc_ = mapDoc;
    cardsDoc_ = cardsDoc;
    bound_ = true;
}

void BattleRecorder::BindBalance(const JsonValue& doc) {
    if (sealed_) return;
    balanceDoc_ = doc;
}

void BattleRecorder::RecordDeploy(const SquadTemplate& t,
                                  std::size_t region, int side) {
    if (sealed_) return;
    JsonValue::Object tmpl;
    tmpl.emplace("attack", JsonValue::Int(t.attack));
    tmpl.emplace("cohesion", JsonValue::Int(t.cohesion));
    tmpl.emplace("cost", JsonValue::Int(t.cost));
    tmpl.emplace("hp", JsonValue::Int(t.hp));
    tmpl.emplace("id", JsonValue::String(t.id));
    tmpl.emplace("name", JsonValue::String(t.name));
    tmpl.emplace("speed",
                 JsonValue::Int(static_cast<std::int64_t>(t.speed)));
    tmpl.emplace("unit", JsonValue::Int(static_cast<std::int64_t>(t.unit)));
    JsonValue::Object op;
    op.emplace("op", JsonValue::String("deploy"));
    op.emplace("region", JsonValue::Int(static_cast<std::int64_t>(region)));
    op.emplace("side", JsonValue::Int(side));
    op.emplace("template", JsonValue::MakeObject(std::move(tmpl)));
    inputs_.push_back(JsonValue::MakeObject(std::move(op)));
}

void BattleRecorder::RecordSheet(std::size_t squadIndex,
                                 const std::vector<std::string>& cardIds) {
    if (sealed_) return;
    JsonValue::Array ids;
    ids.reserve(cardIds.size());
    for (const std::string& id : cardIds) ids.push_back(JsonValue::String(id));
    JsonValue::Object op;
    op.emplace("cards", JsonValue::MakeArray(std::move(ids)));
    op.emplace("op", JsonValue::String("sheet"));
    op.emplace("squad", JsonValue::Int(static_cast<std::int64_t>(squadIndex)));
    inputs_.push_back(JsonValue::MakeObject(std::move(op)));
}

void BattleRecorder::RecordArrow(int side, std::size_t squadIndex,
                                 const std::vector<std::size_t>& path) {
    if (sealed_) return;
    JsonValue::Object op;
    op.emplace("op", JsonValue::String("arrow"));
    op.emplace("path", PathJson(path));
    op.emplace("side", JsonValue::Int(side));
    op.emplace("squad", JsonValue::Int(static_cast<std::int64_t>(squadIndex)));
    inputs_.push_back(JsonValue::MakeObject(std::move(op)));
}

void BattleRecorder::RecordIntel(int fogSide, int squadIndex, int region,
                                 int certainty) {
    if (sealed_) return;
    JsonValue::Object op;
    op.emplace("certainty", JsonValue::Int(certainty));
    op.emplace("fogSide", JsonValue::Int(fogSide));
    op.emplace("op", JsonValue::String("intel"));
    op.emplace("region", JsonValue::Int(region));
    op.emplace("squad", JsonValue::Int(squadIndex));
    inputs_.push_back(JsonValue::MakeObject(std::move(op)));
}

void BattleRecorder::RecordCpPool(int side, int cp) {
    if (sealed_) return;
    JsonValue::Object op;
    op.emplace("cp", JsonValue::Int(cp));
    op.emplace("op", JsonValue::String("cp"));
    op.emplace("side", JsonValue::Int(side));
    inputs_.push_back(JsonValue::MakeObject(std::move(op)));
}

void BattleRecorder::RecordConvoy(
    int side, const std::vector<std::size_t>& path) {
    if (sealed_) return;
    JsonValue::Object op;
    op.emplace("op", JsonValue::String("convoy"));
    op.emplace("path", PathJson(path));
    op.emplace("side", JsonValue::Int(side));
    inputs_.push_back(JsonValue::MakeObject(std::move(op)));
}

void BattleRecorder::RecordMythSeed(std::size_t region, int level) {
    if (sealed_) return;
    JsonValue::Object op;
    op.emplace("level", JsonValue::Int(level));
    op.emplace("op", JsonValue::String("mythseed"));
    op.emplace("region", JsonValue::Int(static_cast<std::int64_t>(region)));
    inputs_.push_back(JsonValue::MakeObject(std::move(op)));
}

void BattleRecorder::RecordMyth(std::size_t region, int kind) {
    if (sealed_) return;
    JsonValue::Object op;
    op.emplace("kind", JsonValue::Int(kind));
    op.emplace("op", JsonValue::String("myth"));
    op.emplace("region", JsonValue::Int(static_cast<std::int64_t>(region)));
    inputs_.push_back(JsonValue::MakeObject(std::move(op)));
}

void BattleRecorder::Seal(const BattleController& bc) {
    // Records exist for completed battles only — sealing mid-battle
    // would fabricate a terminal-looking artifact from partial truth.
    if (bc.Beat() != BattleBeat::Aftermath) return;
    events_ = bc.Events();
    checksum_ = bc.Checksum();
    endTick_ = static_cast<int>(bc.GetSim().TickCount());
    endBeat_ = static_cast<int>(bc.Beat());
    winner_ = bc.Outcome().winnerSide;
    closeReason_ = static_cast<int>(bc.Outcome().closeReason);
    forced_ = bc.Outcome().forced;
    stalemate_ = bc.Outcome().stalemate;
    sealed_ = true;
}

std::uint64_t BattleRecorder::ComputeRoot(const JsonValue& payload) {
    // FNV-1a over the canonical emit — stable across runs/platforms,
    // folded over every byte so any field edit flips the root.
    const std::string text = payload.Emit();
    std::uint64_t h = 14695981039346656037ull;
    for (const unsigned char c : text) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

JsonValue BattleRecorder::PayloadJson() const {
    JsonValue::Object o;
    o.emplace("schema", JsonValue::String(std::string(SCHEMA)));
    if (bound_) {
        o.emplace("map", mapDoc_);
        o.emplace("cards", cardsDoc_);
        if (!balanceDoc_.IsNull()) o.emplace("balance", balanceDoc_);
    }
    o.emplace("seed", JsonValue::Int(static_cast<std::int64_t>(seed_)));
    o.emplace("toolVersion", JsonValue::Int(toolVersion_));
    JsonValue::Array in;
    in.reserve(inputs_.size());
    for (const JsonValue& v : inputs_) in.push_back(v);
    o.emplace("inputs", JsonValue::MakeArray(std::move(in)));
    if (sealed_) {
        JsonValue::Array ev;
        ev.reserve(events_.size());
        for (const SimEvent& e : events_) ev.push_back(EventToJson(e));
        o.emplace("events", JsonValue::MakeArray(std::move(ev)));
        o.emplace("checksum",
                  JsonValue::Int(static_cast<std::int64_t>(checksum_)));
        o.emplace("endTick", JsonValue::Int(endTick_));
        o.emplace("endBeat", JsonValue::Int(endBeat_));
        o.emplace("winner", JsonValue::Int(winner_));
        o.emplace("closeReason", JsonValue::Int(closeReason_));
        o.emplace("forced", JsonValue::Bool(forced_));
        o.emplace("stalemate", JsonValue::Bool(stalemate_));
    }
    return JsonValue::MakeObject(std::move(o));
}

Result<JsonValue> BattleRecorder::ToJson() const {
    if (!bound_ || !sealed_) {
        return Fail<JsonValue>("record", "recorder not bound+sealed");
    }
    JsonValue payload = PayloadJson();
    JsonValue::Object integrity;
    integrity.emplace("root",
                      JsonValue::Int(static_cast<std::int64_t>(
                          ComputeRoot(payload))));
    JsonValue::Object o = payload.Members();
    o.emplace("integrity", JsonValue::MakeObject(std::move(integrity)));
    return Ok(JsonValue::MakeObject(std::move(o)));
}

} // namespace Potato::Gameplay
