#include "Campaign/Myth/MythLog.h"

#include "Gameplay/Doctrine/Doctrine.h" // SimEvent

namespace Potato::Campaign {

Gameplay::Result<std::uint64_t> MythLog::Record(std::string_view action,
                                                std::string_view name,
                                                int side, int region,
                                                int squad) {
    if (entries_.size() >= MAX_ENTRIES) {
        return Gameplay::Fail<std::uint64_t>("mythlog",
                                             "log at capacity");
    }
    if (action.empty() || action.size() > MAX_NAME_LEN ||
        name.empty() || name.size() > MAX_NAME_LEN) {
        return Gameplay::Fail<std::uint64_t>("mythlog",
                                             "bad action/name");
    }
    MythLogEntry e;
    e.seq = entries_.size();
    e.action = std::string(action);
    e.name = std::string(name);
    e.side = side;
    e.region = region;
    e.squad = squad;
    entries_.push_back(std::move(e));
    return Gameplay::Ok(entries_.back().seq);
}

Gameplay::Result<Gameplay::JsonValue> MythLog::ToJson() const {
    using Gameplay::JsonValue;
    JsonValue::Array arr;
    for (const MythLogEntry& e : entries_) {
        JsonValue::Object o;
        o["seq"] = JsonValue::Int(static_cast<std::int64_t>(e.seq));
        o["action"] = JsonValue::String(e.action);
        o["name"] = JsonValue::String(e.name);
        o["side"] = JsonValue::Int(e.side);
        o["region"] = JsonValue::Int(e.region);
        o["squad"] = JsonValue::Int(e.squad);
        arr.push_back(JsonValue::MakeObject(std::move(o)));
    }
    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    root["entries"] = JsonValue::MakeArray(std::move(arr));
    return Gameplay::Ok(JsonValue::MakeObject(std::move(root)));
}

Gameplay::Result<MythLog> MythLog::FromJson(
    const Gameplay::JsonValue& doc) {
    using Gameplay::Fail;
    const std::string* s = doc.FindString("schema");
    if (!s || *s != SCHEMA) {
        return Fail<MythLog>("schema", "expected potato.mythlog/1");
    }
    const auto& arr = doc["entries"];
    if (!arr.IsArray()) {
        return Fail<MythLog>("schema", "entries: not an array");
    }
    if (arr.Size() > MAX_ENTRIES) {
        return Fail<MythLog>("mythlog", "too many entries");
    }
    MythLog out;
    for (const Gameplay::JsonValue& j : arr.Items()) {
        if (!j.IsObject() || !j["seq"].IsInt() ||
            !j["action"].IsString() || !j["name"].IsString() ||
            !j["side"].IsInt() || !j["region"].IsInt() ||
            !j["squad"].IsInt()) {
            return Fail<MythLog>("schema", "malformed entry");
        }
        const std::int64_t seq = j["seq"].AsInt();
        if (seq != static_cast<std::int64_t>(out.entries_.size())) {
            return Fail<MythLog>("schema", "seq not contiguous");
        }
        const auto side = j["side"].AsInt();
        const auto region = j["region"].AsInt();
        const auto squad = j["squad"].AsInt();
        if (side < -1 || side > 1 || region < -1 || squad < -1) {
            return Fail<MythLog>("schema",
                                 "entry field out of range");
        }
        auto r = out.Record(j["action"].AsString(),
                            j["name"].AsString(),
                            static_cast<int>(side),
                            static_cast<int>(region),
                            static_cast<int>(squad));
        if (!r.ok()) return Fail<MythLog>("schema", r.reason);
    }
    return Gameplay::Ok(std::move(out));
}

Gameplay::Result<std::size_t>
LogMythEvents(MythLog& log,
              std::span<const Gameplay::SimEvent> events) {
    std::size_t appended = 0;
    for (const Gameplay::SimEvent& e : events) {
        if (e.kind != Gameplay::SimEvent::Kind::MythInvasion) {
            continue; // MythActionInvoked is already logged at
                      // purchase — never double-book the chronicle.
        }
        auto r = log.Record("invasion", "神罰", e.side, e.param, -1);
        if (!r.ok()) {
            return Gameplay::Fail<std::size_t>("mythlog", r.reason);
        }
        ++appended;
    }
    return Gameplay::Ok(appended);
}

namespace {

// Folk place-name: regions are clerk's coordinates; the people
// name ground differently.
std::string FolkPlace(int region) {
    if (region < 0) return "某處";
    return "第" + std::to_string(region) + "里";
}

// One folk line per entry — hearsay-framed, variant chosen by seq
// parity (deterministic; the text layer draws no PRNG).
std::string FolkLine(const MythLogEntry& e) {
    const std::string place = FolkPlace(e.region);
    const bool alt = (e.seq % 2) == 1;
    if (e.action == "pacify_shrine") {
        return alt ? "聽說" + place + "的廟又肯收香火了。"
                   : "據說" + place + "的神明息了怒。";
    }
    if (e.action == "invoke_possession") {
        return alt ? "聽說有兵卒眼裡冒出金光，說話的不是本人。"
                   : "有人發誓看見" + place +
                         "的兵被神明附了身。";
    }
    if (e.action == "ghost_army") {
        // Licensed exaggeration: the sim knows one garrison; the
        // folk telling always says thousands. Contradiction is the
        // register's privilege — nobody corrects a rumor.
        return alt ? "聽說" + place +
                         "夜裡有陰兵過境——數以千計，雞犬不敢吠。"
                   : "據說" + place +
                         "駐著一支看不見的軍隊，數以千計。";
    }
    if (e.action == "invasion") {
        // Licensed misattribution: whichever banner the host truly
        // carried, the folk claim the god's host marched for their
        // side. The ledger records the banner; rumor records the
        // wish — neither is marked wrong.
        return alt ? "聽說" + place +
                         "的老槐樹流了血淚，神明是真動怒了。"
                   : "據說" + place +
                         "遭了神罰——街坊都說神兵是幫咱們的。";
    }
    // Unknown actions still enter folklore — the register outlives
    // the catalog.
    return "據說" + place + "出了怪事，人說是「" + e.name + "」。";
}

} // namespace

std::string RenderMythLog(const MythLog& log) {
    std::string out = "—— 市井傳聞 ——\n";
    if (log.Entries().empty()) {
        out += "市井無傳聞。\n";
        return out;
    }
    for (const MythLogEntry& e : log.Entries()) {
        out += FolkLine(e);
        out += '\n';
    }
    return out;
}

} // namespace Potato::Campaign
