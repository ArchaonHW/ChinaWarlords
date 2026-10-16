#include "Campaign/World/WorldState.h"

#include "Gameplay/Json/JsonValue.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <tuple>
#include <utility>

namespace Potato::Campaign {
namespace {

using Gameplay::JsonValue;
using Gameplay::Result;

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

const char* ControlName(WorldControl c) {
    switch (c) {
    case WorldControl::Neutral:
        return "neutral";
    case WorldControl::Player:
        return "player";
    case WorldControl::Rival:
        return "rival";
    }
    return "neutral";
}

const char* KindName(WorldEventKind k) {
    switch (k) {
    case WorldEventKind::SetControl:
        return "control";
    case WorldEventKind::Resolve:
        return "resolve";
    case WorldEventKind::March:
        return "march";
    case WorldEventKind::Sight:
        return "sight";
    }
    return "control";
}

bool KindFromName(std::string_view s, WorldEventKind& out) {
    if (s == "control") {
        out = WorldEventKind::SetControl;
        return true;
    }
    if (s == "resolve") {
        out = WorldEventKind::Resolve;
        return true;
    }
    if (s == "march") {
        out = WorldEventKind::March;
        return true;
    }
    if (s == "sight") {
        out = WorldEventKind::Sight;
        return true;
    }
    return false;
}

bool ValidId(std::string_view s) {
    return !s.empty() && s.size() <= WorldState::MAX_ID_LEN;
}

bool ValidLodFactors(const Gameplay::SimulationLodFactors& f) {
    const auto valid = [](std::uint16_t value) {
        return value <= Gameplay::SimulationLodPolicy::MAX_FACTOR;
    };
    return valid(f.distance) && valid(f.combat) && valid(f.narrative) &&
           valid(f.strategic) && valid(f.playerInterest) &&
           valid(f.mythicImportance);
}

const char* LodName(Gameplay::SimulationLod lod) {
    switch (lod) {
    case Gameplay::SimulationLod::Full: return "FULL";
    case Gameplay::SimulationLod::Reduced: return "REDUCED";
    case Gameplay::SimulationLod::Abstract: return "ABSTRACT";
    case Gameplay::SimulationLod::Aggregate: return "AGGREGATE";
    case Gameplay::SimulationLod::Dormant: return "DORMANT";
    }
    return "DORMANT";
}

bool LodFromName(std::string_view name, Gameplay::SimulationLod& out) {
    if (name == "FULL") out = Gameplay::SimulationLod::Full;
    else if (name == "REDUCED") out = Gameplay::SimulationLod::Reduced;
    else if (name == "ABSTRACT") out = Gameplay::SimulationLod::Abstract;
    else if (name == "AGGREGATE") out = Gameplay::SimulationLod::Aggregate;
    else if (name == "DORMANT") out = Gameplay::SimulationLod::Dormant;
    else return false;
    return true;
}

bool ValidLod(Gameplay::SimulationLod lod) {
    return static_cast<std::uint8_t>(lod) <=
           static_cast<std::uint8_t>(Gameplay::SimulationLod::Dormant);
}

// Bitcast a u64 counter to/from an int64 wire field (MintOutbox
// ledger-seal precedent) — bit-exact across platforms.
std::int64_t ToWire(std::uint64_t v) {
    std::int64_t s;
    std::memcpy(&s, &v, sizeof(s));
    return s;
}
std::uint64_t FromWire(std::int64_t s) {
    std::uint64_t v;
    std::memcpy(&v, &s, sizeof(v));
    return v;
}

// Canonical order: (day, seq, insertion-stable).
bool Before(const WorldEvent& a, const WorldEvent& b) {
    if (a.day != b.day) {
        return a.day < b.day;
    }
    return a.seq < b.seq;
}

} // namespace

Result<WorldState> WorldState::Init(const WorldMap& map,
                                    std::uint64_t seed) {
    if (map.NodeCount() == 0) {
        return Gameplay::Fail<WorldState>("world",
                                          "cannot Init on empty map");
    }
    WorldState s;
    s.worldId_ = map.Id();
    s.day_ = 0;
    s.warband_ = map.NodeAt(map.StartIndex()).id;
    for (std::size_t i = 0; i < map.NodeCount(); ++i) {
        const WorldNode& n = map.NodeAt(i);
        if (n.control != WorldControl::Neutral) {
            s.control_[n.id] = n.control;
        }
    }
    s.rng_ = Gameplay::Prng(seed);
    return Gameplay::Ok(std::move(s));
}

WorldControl WorldState::ControlAt(std::string_view node) const {
    const auto it = control_.find(std::string(node));
    return it == control_.end() ? WorldControl::Neutral : it->second;
}

bool WorldState::MarkResolved(std::string_view id) {
    if (!ValidId(id)) return false;
    if (resolved_.count(std::string(id)) != 0) return true;
    if (resolved_.size() >= MAX_RESOLVED) return false;
    resolved_.emplace(id);
    return true;
}

Result<bool> WorldState::Enqueue(const WorldMap& map, WorldEvent ev) {
    if (!ValidId(ev.node)) {
        return Gameplay::Fail<bool>("world", "event node id bad");
    }
    if (map.FindNode(ev.node) == nullptr) {
        return Gameplay::Fail<bool>("world",
                                    "event targets unknown node");
    }
    if (ev.day < day_ || ev.day > MAX_DAY) {
        return Gameplay::Fail<bool>("world",
                                    "event day out of range");
    }
    if (queue_.size() >= MAX_EVENTS) {
        return Gameplay::Fail<bool>("world", "event queue full");
    }
    // Keep nextSeq_ above every queued seq — events authored
    // without NextSeq() must not let the counter replay an id
    // (FromJson's floor is the same rule for the wire).
    if (ev.seq >= nextSeq_) {
        nextSeq_ = ev.seq + 1;
    }
    queue_.push_back(std::move(ev));
    return Gameplay::Ok(true);
}

Result<int> WorldState::ResolveBeats(const WorldMap& map,
                                     std::int64_t days) {
    if (days < 0 || days > MAX_DAY - day_) {
        return Gameplay::Fail<int>("world", "day advance out of range");
    }
    day_ += days;

    // Split due events out, THEN stable-sort the extracted batch —
    // sorting index lists breaks the compaction below (the index
    // order no longer matches queue positions).
    std::vector<WorldEvent> batch;
    std::vector<WorldEvent> keep;
    for (WorldEvent& ev : queue_) {
        if (ev.day <= day_) {
            batch.push_back(std::move(ev));
        } else {
            keep.push_back(std::move(ev));
        }
    }
    std::stable_sort(batch.begin(), batch.end(), Before);
    queue_ = std::move(keep);

    int applied = 0;
    for (const WorldEvent& ev : batch) {
        if (map.FindNode(ev.node) == nullptr) {
            // A March whose destination was edited out resolves
            // into nothing: the warband never arrived (no
            // teleport) but it's no longer walking to a place
            // that doesn't exist — release the in-flight marker.
            if (ev.kind == WorldEventKind::March &&
                marchingTo_ == ev.node) {
                marchingTo_.clear();
            }
            continue; // node edited out of content — skip
        }
        switch (ev.kind) {
        case WorldEventKind::SetControl:
            if (ev.control == WorldControl::Neutral) {
                control_.erase(ev.node);
            } else {
                control_[ev.node] = ev.control;
            }
            break;
        case WorldEventKind::Resolve:
            if (resolved_.size() < MAX_RESOLVED ||
                resolved_.count(ev.node)) {
                resolved_.insert(ev.node);
            }
            break;
        case WorldEventKind::March:
            warband_ = ev.node;
            marchingTo_.clear(); // an arrival concludes the march
            break;
        case WorldEventKind::Sight: {
            SightingMark& m = sightings_[ev.node];
            m.day = day_;
            ++m.count;
            break;
        }
        }
        ++applied;
    }
    return Gameplay::Ok(applied);
}

Result<bool> WorldState::EnableOracle(std::uint64_t seed,
                                      std::uint64_t state) {
    if (seed == 0) {
        return Gameplay::Fail<bool>("world",
                                    "oracle seed must be nonzero");
    }
    oracle_.emplace(seed, state);
    oracleSeed_ = seed;
    return Gameplay::Ok(true);
}

bool WorldState::DrawPermille(int permille) {
    if (permille <= 0) return false;
    if (permille >= 1000) return true;
    if (oracle_) return oracle_->DrawPermille(permille);
    return (rng_.Next() % 1000) < static_cast<std::uint64_t>(permille);
}

int WorldState::DrawCategorical(const int* weights, int count) {
    if (weights == nullptr || count <= 0) return -1;
    if (oracle_) return oracle_->DrawCategorical(weights, count);
    int nonzero = 0;
    int sole = -1;
    std::uint64_t total = 0;
    for (int i = 0; i < count; ++i) {
        if (weights[i] > 0) {
            ++nonzero;
            sole = i;
            total += static_cast<std::uint64_t>(weights[i]);
        }
    }
    if (nonzero == 0) return -1;
    if (nonzero == 1) return sole;
    std::uint64_t roll = rng_.Next() % total;
    for (int i = 0; i < count; ++i) {
        if (weights[i] <= 0) continue;
        if (roll < static_cast<std::uint64_t>(weights[i])) return i;
        roll -= static_cast<std::uint64_t>(weights[i]);
    }
    return sole;
}

bool WorldState::SetWarband(std::string_view node,
                            const WorldMap& map) {
    if (map.FindNode(node) == nullptr) {
        return false;
    }
    warband_ = std::string(node);
    return true;
}

Result<bool> WorldState::SetAgents(
    const WorldMap& map, std::span<const WorldAgentState> agents) {
    if (map.Id() != worldId_) {
        return Gameplay::Fail<bool>("world",
                                    "agent map does not match state");
    }
    if (agents.size() > MAX_AGENTS) {
        return Gameplay::Fail<bool>("world", "agent count exceeds limit");
    }

    std::vector<WorldAgentState> next(agents.begin(), agents.end());
    for (const WorldAgentState& agent : next) {
        if (!ValidId(agent.publicNode) ||
            map.FindNode(agent.publicNode) == nullptr) {
            return Gameplay::Fail<bool>("world",
                                        "agent public node is unknown");
        }
        if (!ValidLodFactors(agent.lodFactors)) {
            return Gameplay::Fail<bool>("validation",
                                        "agent LOD factor out of range");
        }
    }
    std::sort(next.begin(), next.end(),
              [](const WorldAgentState& a, const WorldAgentState& b) {
                  return a.id < b.id;
              });
    for (std::size_t i = 1; i < next.size(); ++i) {
        if (next[i - 1].id == next[i].id) {
            return Gameplay::Fail<bool>("duplicate",
                                        "agent IDs must be unique");
        }
    }

    std::vector<WorldAgentLodState> nextLodStates;
    nextLodStates.reserve(next.size());
    for (const WorldAgentState& agent : next) {
        const auto current = std::lower_bound(
            agentLodStates_.begin(), agentLodStates_.end(), agent.id,
            [](const WorldAgentLodState& state, std::uint64_t id) {
                return state.agentId < id;
            });
        const Gameplay::SimulationLod lod =
            current != agentLodStates_.end() && current->agentId == agent.id
                ? current->lod
                : Gameplay::SimulationLod::Dormant;
        nextLodStates.push_back({agent.id, lod});
    }
    agents_ = std::move(next);
    agentLodStates_ = std::move(nextLodStates);
    return Gameplay::Ok(true);
}

Result<std::vector<Gameplay::SimulationLodAssignment>>
WorldState::PlanAgentLod(const Gameplay::SimulationLodPolicy& policy,
                         const Gameplay::SimulationBudget& budget) const {
    std::vector<Gameplay::SimulationLodSubject> subjects;
    subjects.reserve(agents_.size());
    for (const WorldAgentState& agent : agents_) {
        subjects.push_back({agent.id, agent.lodFactors});
    }
    return policy.Plan(subjects, budget);
}

Result<bool> WorldState::ApplyAgentLodPlan(
    std::span<const Gameplay::SimulationLodAssignment> plan) {
    if (plan.size() != agents_.size() ||
        agentLodStates_.size() != agents_.size()) {
        return Gameplay::Fail<bool>(
            "lod_plan", "LOD plan must cover every current world agent");
    }

    std::vector<WorldAgentLodState> nextStates;
    nextStates.reserve(plan.size());
    std::vector<WorldAgentLodTransition> changes;
    changes.reserve(plan.size());
    for (std::size_t i = 0; i < plan.size(); ++i) {
        const Gameplay::SimulationLodAssignment& assignment = plan[i];
        const WorldAgentState& agent = agents_[i];
        const WorldAgentLodState& current = agentLodStates_[i];
        if (assignment.agentId != agent.id || current.agentId != agent.id ||
            assignment.importance < 0 || !ValidLod(assignment.lod)) {
            return Gameplay::Fail<bool>(
                "lod_plan", "LOD plan IDs, importance, or levels are invalid");
        }
        nextStates.push_back({assignment.agentId, assignment.lod});
        if (current.lod != assignment.lod) {
            changes.push_back(WorldAgentLodTransition{
                day_, static_cast<std::uint64_t>(agentLodHistory_.size() +
                                                 changes.size()),
                agent.id, agent.publicNode, current.lod, assignment.lod});
        }
    }
    if (changes.size() > MAX_LOD_EVENTS - agentLodHistory_.size()) {
        return Gameplay::Fail<bool>("lod_history", "LOD history is full");
    }

    agentLodStates_ = std::move(nextStates);
    agentLodHistory_.insert(agentLodHistory_.end(), changes.begin(),
                            changes.end());
    return Gameplay::Ok(true);
}

std::vector<WorldAgentLodAggregate> WorldState::AgentLodAggregates() const {
    using GroupKey = std::tuple<std::string, bool, std::uint64_t>;
    std::map<GroupKey, WorldAgentLodAggregate> groups;
    const auto addFactor = [](std::uint64_t& total, std::uint16_t factor) {
        total += factor;
    };
    for (std::size_t i = 0; i < agents_.size(); ++i) {
        if (i >= agentLodStates_.size() ||
            agentLodStates_[i].lod != Gameplay::SimulationLod::Aggregate) {
            continue;
        }
        const WorldAgentState& agent = agents_[i];
        const bool isolated = agent.aggregateGroupId == 0;
        const std::uint64_t keyId = isolated ? agent.id : agent.aggregateGroupId;
        const GroupKey key{agent.publicNode, isolated, keyId};
        auto [it, inserted] = groups.try_emplace(key);
        WorldAgentLodAggregate& aggregate = it->second;
        if (inserted) {
            aggregate.publicNode = agent.publicNode;
            aggregate.aggregateGroupId = agent.aggregateGroupId;
        }
        aggregate.agentIds.push_back(agent.id);
        addFactor(aggregate.factorTotals[0], agent.lodFactors.distance);
        addFactor(aggregate.factorTotals[1], agent.lodFactors.combat);
        addFactor(aggregate.factorTotals[2], agent.lodFactors.narrative);
        addFactor(aggregate.factorTotals[3], agent.lodFactors.strategic);
        addFactor(aggregate.factorTotals[4], agent.lodFactors.playerInterest);
        addFactor(aggregate.factorTotals[5], agent.lodFactors.mythicImportance);
    }

    std::vector<WorldAgentLodAggregate> result;
    result.reserve(groups.size());
    for (auto& [key, aggregate] : groups) {
        (void)key;
        result.push_back(std::move(aggregate));
    }
    return result;
}

Result<JsonValue> WorldState::ToJson() const {
    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    root["world"] = JsonValue::String(worldId_);
    root["day"] = JsonValue::Int(day_);
    root["warband"] = JsonValue::String(warband_);
    if (!control_.empty()) {
        JsonValue::Object ctrl;
        for (const auto& [id, c] : control_) {
            ctrl[id] = JsonValue::String(ControlName(c));
        }
        root["control"] = JsonValue::MakeObject(std::move(ctrl));
    }
    if (!resolved_.empty()) {
        JsonValue::Array rs;
        for (const std::string& id : resolved_) {
            rs.push_back(JsonValue::String(id));
        }
        root["resolved"] = JsonValue::MakeArray(std::move(rs));
    }
    root["rng"] = JsonValue::Int(ToWire(rng_.State()));
    root["seq"] = JsonValue::Int(ToWire(nextSeq_));
    if (oracle_) {
        JsonValue::Object o;
        o["seed"] = JsonValue::Int(ToWire(oracleSeed_));
        o["state"] = JsonValue::Int(ToWire(oracle_->StreamState()));
        root["oracle"] = JsonValue::MakeObject(std::move(o));
    }
    if (!marchingTo_.empty()) {
        JsonValue::Object m;
        m["to"] = JsonValue::String(marchingTo_);
        m["eta"] = JsonValue::Int(marchEta_);
        root["marching"] = JsonValue::MakeObject(std::move(m));
    }
    if (!sightings_.empty()) {
        JsonValue::Object s;
        for (const auto& [id, mark] : sightings_) {
            JsonValue::Object jm;
            jm["day"] = JsonValue::Int(mark.day);
            jm["count"] = JsonValue::Int(mark.count);
            s[id] = JsonValue::MakeObject(std::move(jm));
        }
        root["sightings"] = JsonValue::MakeObject(std::move(s));
    }
    if (!agents_.empty()) {
        JsonValue::Array agents;
        agents.reserve(agents_.size());
        for (const WorldAgentState& agent : agents_) {
            JsonValue::Object ja;
            ja["id"] = JsonValue::Int(ToWire(agent.id));
            ja["public_node"] = JsonValue::String(agent.publicNode);
            if (agent.aggregateGroupId != 0) {
                ja["aggregate_group"] =
                    JsonValue::Int(ToWire(agent.aggregateGroupId));
            }
            JsonValue::Object jf;
            jf["distance"] = JsonValue::Int(agent.lodFactors.distance);
            jf["combat"] = JsonValue::Int(agent.lodFactors.combat);
            jf["narrative"] = JsonValue::Int(agent.lodFactors.narrative);
            jf["strategic"] = JsonValue::Int(agent.lodFactors.strategic);
            jf["player_interest"] =
                JsonValue::Int(agent.lodFactors.playerInterest);
            jf["mythic_importance"] =
                JsonValue::Int(agent.lodFactors.mythicImportance);
            ja["lod"] = JsonValue::MakeObject(std::move(jf));
            agents.push_back(JsonValue::MakeObject(std::move(ja)));
        }
        root["agents"] = JsonValue::MakeArray(std::move(agents));
    }
    if (!agentLodStates_.empty() || !agentLodHistory_.empty()) {
        JsonValue::Object runtime;
        JsonValue::Array current;
        current.reserve(agentLodStates_.size());
        for (const WorldAgentLodState& state : agentLodStates_) {
            JsonValue::Object entry;
            entry["id"] = JsonValue::Int(ToWire(state.agentId));
            entry["lod"] = JsonValue::String(LodName(state.lod));
            current.push_back(JsonValue::MakeObject(std::move(entry)));
        }
        runtime["current"] = JsonValue::MakeArray(std::move(current));

        JsonValue::Array events;
        events.reserve(agentLodHistory_.size());
        for (const WorldAgentLodTransition& event : agentLodHistory_) {
            JsonValue::Object entry;
            entry["day"] = JsonValue::Int(event.day);
            entry["seq"] = JsonValue::Int(
                static_cast<std::int64_t>(event.sequence));
            entry["id"] = JsonValue::Int(ToWire(event.agentId));
            entry["public_node"] = JsonValue::String(event.publicNode);
            entry["from"] = JsonValue::String(LodName(event.from));
            entry["to"] = JsonValue::String(LodName(event.to));
            events.push_back(JsonValue::MakeObject(std::move(entry)));
        }
        runtime["events"] = JsonValue::MakeArray(std::move(events));
        root["lod_runtime"] = JsonValue::MakeObject(std::move(runtime));
    }
    if (!queue_.empty()) {
        std::vector<WorldEvent> sorted = queue_;
        std::stable_sort(sorted.begin(), sorted.end(), Before);
        JsonValue::Array q;
        for (const WorldEvent& ev : sorted) {
            JsonValue::Object je;
            je["day"] = JsonValue::Int(ev.day);
            je["seq"] = JsonValue::Int(
                static_cast<std::int64_t>(ev.seq));
            je["kind"] = JsonValue::String(KindName(ev.kind));
            je["node"] = JsonValue::String(ev.node);
            if (ev.kind == WorldEventKind::SetControl) {
                je["control"] =
                    JsonValue::String(ControlName(ev.control));
            }
            q.push_back(JsonValue::MakeObject(std::move(je)));
        }
        root["queue"] = JsonValue::MakeArray(std::move(q));
    }
    return Gameplay::Ok(JsonValue::MakeObject(std::move(root)));
}

Result<WorldState> WorldState::FromJson(const JsonValue& doc) {
    if (!doc.IsObject()) {
        return Gameplay::Fail<WorldState>("world",
                                          "state is not an object");
    }
    {
        const std::string* s = doc.FindString("schema");
        if (s == nullptr || *s != SCHEMA) {
            return Gameplay::Fail<WorldState>(
                "schema", "expected potato.worldstate/1");
        }
    }
    // Two-pass: validate everything into locals, commit at the end.
    WorldState s;
    {
        const std::string* w = doc.FindString("world");
        if (w == nullptr || !ValidId(*w)) {
            return Gameplay::Fail<WorldState>("world",
                                              "missing/bad 'world'");
        }
        s.worldId_ = *w;
    }
    {
        const JsonValue& d = doc["day"];
        if (!d.IsInt() || d.AsInt() < 0 || d.AsInt() > MAX_DAY) {
            return Gameplay::Fail<WorldState>("world", "bad 'day'");
        }
        s.day_ = d.AsInt();
    }
    {
        const std::string* wb = doc.FindString("warband");
        if (wb == nullptr || !ValidId(*wb)) {
            return Gameplay::Fail<WorldState>("world",
                                              "missing/bad 'warband'");
        }
        s.warband_ = *wb;
    }
    if (doc.Has("control")) {
        const JsonValue& c = doc["control"];
        if (!c.IsObject()) {
            return Gameplay::Fail<WorldState>("world",
                                              "'control' not object");
        }
        for (const auto& [k, v] : c.Members()) {
            if (!ValidId(k)) {
                return Gameplay::Fail<WorldState>(
                    "world", "control key out of range");
            }
            WorldControl wc;
            if (!v.IsString() || !ControlFromName(v.AsString(), wc)) {
                return Gameplay::Fail<WorldState>(
                    "world", "control value unknown");
            }
            if (wc != WorldControl::Neutral) {
                s.control_[k] = wc;
            }
        }
    }
    if (doc.Has("resolved")) {
        const JsonValue& r = doc["resolved"];
        if (!r.IsArray() || r.Size() > MAX_RESOLVED) {
            return Gameplay::Fail<WorldState>("world",
                                              "'resolved' bad/over");
        }
        for (const JsonValue& item : r.Items()) {
            if (!item.IsString() || !ValidId(item.AsString())) {
                return Gameplay::Fail<WorldState>(
                    "world", "resolved entry bad");
            }
            s.resolved_.insert(item.AsString());
        }
    }
    if (doc.Has("rng")) {
        const JsonValue& r = doc["rng"];
        if (!r.IsInt()) {
            return Gameplay::Fail<WorldState>("world",
                                              "'rng' not int");
        }
        s.rng_ = Gameplay::Prng(FromWire(r.AsInt()));
    }
    if (doc.Has("oracle")) {
        const JsonValue& o = doc["oracle"];
        if (!o.IsObject()) {
            return Gameplay::Fail<WorldState>("world",
                                              "'oracle' bad");
        }
        const JsonValue& sd = o["seed"];
        const JsonValue& st = o["state"];
        if (!sd.IsInt() || FromWire(sd.AsInt()) == 0) {
            return Gameplay::Fail<WorldState>("world",
                                              "'oracle.seed' bad");
        }
        if (!st.IsInt() || st.AsInt() < 0) {
            return Gameplay::Fail<WorldState>("world",
                                              "'oracle.state' bad");
        }
        s.oracle_.emplace(FromWire(sd.AsInt()), FromWire(st.AsInt()));
        s.oracleSeed_ = FromWire(sd.AsInt());
    }
    if (doc.Has("queue")) {
        const JsonValue& q = doc["queue"];
        if (!q.IsArray() || q.Size() > MAX_EVENTS) {
            return Gameplay::Fail<WorldState>("world",
                                              "'queue' bad/over");
        }
        for (const JsonValue& je : q.Items()) {
            if (!je.IsObject()) {
                return Gameplay::Fail<WorldState>(
                    "world", "queue entry not object");
            }
            WorldEvent ev;
            const JsonValue& d = je["day"];
            const JsonValue& sq = je["seq"];
            if (!d.IsInt() || d.AsInt() < 0 || d.AsInt() > MAX_DAY ||
                !sq.IsInt() || sq.AsInt() < 0) {
                return Gameplay::Fail<WorldState>(
                    "world", "queue day/seq bad");
            }
            ev.day = d.AsInt();
            ev.seq = static_cast<std::uint64_t>(sq.AsInt());
            const std::string* k = je.FindString("kind");
            if (k == nullptr || !KindFromName(*k, ev.kind)) {
                return Gameplay::Fail<WorldState>(
                    "world", "queue kind unknown");
            }
            const std::string* n = je.FindString("node");
            if (n == nullptr || !ValidId(*n)) {
                return Gameplay::Fail<WorldState>(
                    "world", "queue node bad");
            }
            ev.node = *n;
            if (ev.seq >= s.nextSeq_) {
                s.nextSeq_ = ev.seq + 1;
            }
            if (ev.kind == WorldEventKind::SetControl) {
                const std::string* c = je.FindString("control");
                if (c == nullptr ||
                    !ControlFromName(*c, ev.control)) {
                    return Gameplay::Fail<WorldState>(
                        "world", "queue control bad");
                }
            }
            s.queue_.push_back(std::move(ev));
        }
    }
    // Explicit "seq" wins over the queue-derived floor.
    if (doc.Has("seq")) {
        const JsonValue& sq = doc["seq"];
        if (!sq.IsInt()) {
            return Gameplay::Fail<WorldState>("world",
                                              "'seq' not int");
        }
        const std::uint64_t v = FromWire(sq.AsInt());
        if (v > s.nextSeq_) {
            s.nextSeq_ = v;
        }
    }
    if (doc.Has("marching")) {
        const JsonValue& m = doc["marching"];
        if (!m.IsObject()) {
            return Gameplay::Fail<WorldState>("world",
                                              "'marching' bad");
        }
        const std::string* to = m.FindString("to");
        const JsonValue& eta = m["eta"];
        if (to == nullptr || !ValidId(*to) || !eta.IsInt() ||
            eta.AsInt() < 0 || eta.AsInt() > MAX_DAY) {
            return Gameplay::Fail<WorldState>("world",
                                              "marching to/eta bad");
        }
        s.marchingTo_ = *to;
        s.marchEta_ = eta.AsInt();
    }
    if (doc.Has("sightings")) {
        const JsonValue& sv = doc["sightings"];
        if (!sv.IsObject() || sv.Size() > MAX_RESOLVED) {
            return Gameplay::Fail<WorldState>("world",
                                              "'sightings' bad/over");
        }
        for (const auto& [k, v] : sv.Members()) {
            if (!ValidId(k) || !v.IsObject()) {
                return Gameplay::Fail<WorldState>(
                    "world", "sighting entry bad");
            }
            const JsonValue& d = v["day"];
            const JsonValue& c = v["count"];
            if (!d.IsInt() || d.AsInt() < 0 || !c.IsInt() ||
                c.AsInt() < 1 || c.AsInt() > 1000000) {
                return Gameplay::Fail<WorldState>(
                    "world", "sighting day/count bad");
            }
            s.sightings_[k] = {d.AsInt(), c.AsInt()};
        }
    }
    if (doc.Has("agents")) {
        const JsonValue& a = doc["agents"];
        if (!a.IsArray() || a.Size() > MAX_AGENTS) {
            return Gameplay::Fail<WorldState>("world", "'agents' bad/over");
        }
        s.agents_.reserve(a.Size());
        for (const JsonValue& ja : a.Items()) {
            if (!ja.IsObject() || !ja["id"].IsInt()) {
                return Gameplay::Fail<WorldState>("world",
                                                  "agent entry/id bad");
            }
            const std::string* node = ja.FindString("public_node");
            const JsonValue& jf = ja["lod"];
            if (node == nullptr || !ValidId(*node) || !jf.IsObject()) {
                return Gameplay::Fail<WorldState>("world",
                                                  "agent node/LOD missing");
            }
            const auto readFactor = [&jf](std::string_view key,
                                          std::uint16_t& out) {
                const JsonValue& value = jf[key];
                const std::int64_t raw = value.AsInt();
                if (!value.IsInt() || raw < 0 ||
                    raw > Gameplay::SimulationLodPolicy::MAX_FACTOR) {
                    return false;
                }
                out = static_cast<std::uint16_t>(raw);
                return true;
            };
            WorldAgentState agent;
            agent.id = FromWire(ja["id"].AsInt());
            agent.publicNode = *node;
            if (ja.Has("aggregate_group")) {
                const JsonValue& group = ja["aggregate_group"];
                if (!group.IsInt()) {
                    return Gameplay::Fail<WorldState>(
                        "world", "agent aggregate group bad");
                }
                agent.aggregateGroupId = FromWire(group.AsInt());
            }
            if (!readFactor("distance", agent.lodFactors.distance) ||
                !readFactor("combat", agent.lodFactors.combat) ||
                !readFactor("narrative", agent.lodFactors.narrative) ||
                !readFactor("strategic", agent.lodFactors.strategic) ||
                !readFactor("player_interest",
                            agent.lodFactors.playerInterest) ||
                !readFactor("mythic_importance",
                            agent.lodFactors.mythicImportance)) {
                return Gameplay::Fail<WorldState>("world",
                                                  "agent LOD factor bad");
            }
            s.agents_.push_back(std::move(agent));
        }
        std::sort(s.agents_.begin(), s.agents_.end(),
                  [](const WorldAgentState& left,
                     const WorldAgentState& right) {
                      return left.id < right.id;
                  });
        for (std::size_t i = 1; i < s.agents_.size(); ++i) {
            if (s.agents_[i - 1].id == s.agents_[i].id) {
                return Gameplay::Fail<WorldState>("duplicate",
                                                  "agent IDs must be unique");
            }
        }
    }
    s.agentLodStates_.reserve(s.agents_.size());
    for (const WorldAgentState& agent : s.agents_) {
        s.agentLodStates_.push_back(
            {agent.id, Gameplay::SimulationLod::Dormant});
    }
    if (doc.Has("lod_runtime")) {
        const JsonValue& runtime = doc["lod_runtime"];
        if (!runtime.IsObject() || !runtime["current"].IsArray() ||
            !runtime["events"].IsArray() ||
            runtime["current"].Size() != s.agents_.size() ||
            runtime["events"].Size() > MAX_LOD_EVENTS) {
            return Gameplay::Fail<WorldState>(
                "world", "'lod_runtime' shape/count bad");
        }

        std::vector<WorldAgentLodState> current;
        current.reserve(runtime["current"].Size());
        for (std::size_t i = 0; i < runtime["current"].Size(); ++i) {
            const JsonValue& entry = runtime["current"].At(i);
            const std::string* lodName = entry.FindString("lod");
            Gameplay::SimulationLod lod;
            if (!entry.IsObject() || !entry["id"].IsInt() ||
                lodName == nullptr || !LodFromName(*lodName, lod)) {
                return Gameplay::Fail<WorldState>(
                    "world", "LOD current entry bad");
            }
            const std::uint64_t id = FromWire(entry["id"].AsInt());
            if (id != s.agents_[i].id) {
                return Gameplay::Fail<WorldState>(
                    "world", "LOD current IDs must match agents in order");
            }
            current.push_back({id, lod});
        }

        std::vector<WorldAgentLodTransition> history;
        history.reserve(runtime["events"].Size());
        std::int64_t previousDay = 0;
        for (std::size_t i = 0; i < runtime["events"].Size(); ++i) {
            const JsonValue& entry = runtime["events"].At(i);
            const JsonValue& day = entry["day"];
            const JsonValue& sequence = entry["seq"];
            const JsonValue& id = entry["id"];
            const std::string* node = entry.FindString("public_node");
            const std::string* fromName = entry.FindString("from");
            const std::string* toName = entry.FindString("to");
            Gameplay::SimulationLod from;
            Gameplay::SimulationLod to;
            if (!entry.IsObject() || !day.IsInt() || day.AsInt() < 0 ||
                day.AsInt() > s.day_ || !sequence.IsInt() ||
                sequence.AsInt() != static_cast<std::int64_t>(i) ||
                !id.IsInt() || node == nullptr || !ValidId(*node) ||
                fromName == nullptr || toName == nullptr ||
                !LodFromName(*fromName, from) ||
                !LodFromName(*toName, to) || from == to ||
                (i != 0 && day.AsInt() < previousDay)) {
                return Gameplay::Fail<WorldState>(
                    "world", "LOD transition event bad/order invalid");
            }
            history.push_back({day.AsInt(), static_cast<std::uint64_t>(i),
                               FromWire(id.AsInt()), *node, from, to});
            previousDay = day.AsInt();
        }
        s.agentLodStates_ = std::move(current);
        s.agentLodHistory_ = std::move(history);
    }
    return Gameplay::Ok(std::move(s));
}

} // namespace Potato::Campaign
