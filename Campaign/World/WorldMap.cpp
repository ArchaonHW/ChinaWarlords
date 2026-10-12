#include "Campaign/World/WorldMap.h"

#include "Gameplay/Json/Json.h"
#include "Gameplay/Map/BattleMap.h" // flag bit constants

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <set>
#include <system_error>
#include <utility>

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

// Token→bit tables mirror BattleMap.cpp's anonymous-namespace
// tables — the vocabularies are the wire contract of two schemas
// (potato.map/1 and potato.world/1) and MUST stay byte-identical.
// (Bencao.cpp's IsTerrainFlag is the existing duplication
// precedent; the bit values themselves come from BattleMap.h.)
struct FlagDef {
    const char* token;
    std::uint32_t bit;
};

constexpr FlagDef TERRAIN_FLAGS[] = {
    {"water", Gameplay::TERRAIN_WATER},
    {"river", Gameplay::TERRAIN_RIVER},
    {"road", Gameplay::TERRAIN_ROAD},
    {"forest", Gameplay::TERRAIN_FOREST},
    {"highland", Gameplay::TERRAIN_HIGHLAND},
    {"chokepoint", Gameplay::TERRAIN_CHOKEPOINT},
    {"open", Gameplay::TERRAIN_OPEN},
};
constexpr FlagDef STRATEGIC_FLAGS[] = {
    {"ferry", Gameplay::STRATEGIC_FERRY},
    {"depot", Gameplay::STRATEGIC_DEPOT},
    {"village", Gameplay::STRATEGIC_VILLAGE},
};
constexpr FlagDef HIST_FLAGS[] = {
    {"grain_route", Gameplay::HIST_GRAIN_ROUTE},
    {"telegraph", Gameplay::HIST_TELEGRAPH},
    {"supply_line", Gameplay::HIST_SUPPLY_LINE},
};
constexpr FlagDef MYTH_FLAGS[] = {
    {"shrine", Gameplay::MYTH_SHRINE},
    {"spirit_road", Gameplay::MYTH_SPIRIT_ROAD},
    {"haunted", Gameplay::MYTH_HAUNTED},
};

// Wire bound — a load must not admit content no write path could
// produce; the routes array is not otherwise bounded.
constexpr std::size_t kMaxRoutes = 4096;

template <std::size_t N>
Result<std::uint32_t> DecodeFlags(const JsonValue& field,
                                  const FlagDef (&table)[N],
                                  const char* fieldName,
                                  const std::string& nodeId) {
    if (field.IsNull()) return Gameplay::Ok<std::uint32_t>(0);
    if (!field.IsArray()) {
        return Gameplay::Fail<std::uint32_t>(
            "world", std::string("node '") + nodeId + "': '" + fieldName +
                   "' must be an array of flag strings");
    }
    std::uint32_t bits = 0;
    for (const JsonValue& item : field.Items()) {
        if (!item.IsString()) {
            return Gameplay::Fail<std::uint32_t>(
                "world", std::string("node '") + nodeId + "': '" +
                       fieldName + "' entries must be strings");
        }
        const std::string& tok = item.AsString();
        bool known = false;
        for (const FlagDef& def : table) {
            if (tok == def.token) { bits |= def.bit; known = true; break; }
        }
        if (!known) {
            return Gameplay::Fail<std::uint32_t>(
                "world", std::string("node '") + nodeId + "': unknown " +
                       fieldName + " flag '" + tok + "'");
        }
    }
    return Gameplay::Ok<std::uint32_t>(bits);
}

// Closed control vocabulary; absent field defaults to Neutral.
Result<WorldControl> DecodeControl(const JsonValue& field,
                                   const std::string& nodeId) {
    if (field.IsNull()) {
        return Gameplay::Ok(WorldControl::Neutral);
    }
    if (!field.IsString()) {
        return Gameplay::Fail<WorldControl>(
            "world", "node '" + nodeId + "': 'control' must be a string");
    }
    const std::string& s = field.AsString();
    if (s == "neutral") return Gameplay::Ok(WorldControl::Neutral);
    if (s == "player") return Gameplay::Ok(WorldControl::Player);
    if (s == "rival") return Gameplay::Ok(WorldControl::Rival);
    return Gameplay::Fail<WorldControl>(
        "world", "node '" + nodeId + "': unknown control '" + s + "'");
}

} // namespace

Result<WorldMap> WorldMap::Load(std::string_view path) {
    Result<JsonValue> doc =
        Gameplay::Json::Load(path, WorldLibrary::SCHEMA);
    if (!doc.ok()) {
        return Gameplay::Fail<WorldMap>(doc.error, doc.reason);
    }
    return FromJson(doc.value);
}

Result<WorldMap> WorldMap::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Gameplay::Fail<WorldMap>("world", "root is not an object");
    }
    // Callers may hand us ungated DOM — when a schema tag IS present
    // it must still match potato.world/1 (absent = pre-gated/test
    // DOM, BattleMap convention).
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != "potato.world/1") {
            return Gameplay::Fail<WorldMap>("schema",
                                          "expected potato.world/1");
        }
    }

    WorldMap map;

    if (root.Has("id") && !root["id"].IsString()) {
        return Gameplay::Fail<WorldMap>("world", "'id' must be a string");
    }
    const std::string* id = root.FindString("id");
    if (id == nullptr || id->empty() ||
        id->size() > WorldLibrary::MAX_ID_LEN) {
        return Gameplay::Fail<WorldMap>("world",
                                        "missing/empty/out-of-range 'id'");
    }
    map.id_ = *id;
    if (root.Has("name") && !root["name"].IsString()) {
        return Gameplay::Fail<WorldMap>("world",
                                        "'name' must be a string");
    }
    if (const std::string* name = root.FindString("name")) {
        if (name->size() > WorldLibrary::MAX_NAME_LEN) {
            return Gameplay::Fail<WorldMap>("world", "'name' too long");
        }
        map.name_ = *name;
    }

    const JsonValue& nodes = root["nodes"];
    if (!nodes.IsArray() || nodes.Items().empty()) {
        return Gameplay::Fail<WorldMap>(
            "world", "'nodes' must be a non-empty array");
    }
    if (nodes.Items().size() > WorldLibrary::MAX_NODES) {
        return Gameplay::Fail<WorldMap>("world",
                                        "node count exceeds MAX_NODES");
    }

    std::map<std::string, std::size_t, std::less<>> indexOf;
    for (const JsonValue& jn : nodes.Items()) {
        if (!jn.IsObject()) {
            return Gameplay::Fail<WorldMap>(
                "world", "node entry is not an object");
        }
        if (jn.Has("id") && !jn["id"].IsString()) {
            return Gameplay::Fail<WorldMap>(
                "world", "node 'id' must be a string");
        }
        const std::string* nid = jn.FindString("id");
        if (nid == nullptr || nid->empty() ||
            nid->size() > WorldLibrary::MAX_ID_LEN) {
            return Gameplay::Fail<WorldMap>(
                "world", "node missing/empty/out-of-range 'id'");
        }
        if (indexOf.count(*nid) != 0) {
            return Gameplay::Fail<WorldMap>(
                "world", "duplicate node id '" + *nid + "'");
        }

        WorldNode n;
        n.id = *nid;
        if (jn.Has("name") && !jn["name"].IsString()) {
            return Gameplay::Fail<WorldMap>(
                "world", "node '" + n.id + "': 'name' must be a string");
        }
        if (const std::string* nn = jn.FindString("name")) {
            if (nn->size() > WorldLibrary::MAX_NAME_LEN) {
                return Gameplay::Fail<WorldMap>(
                    "world", "node '" + n.id + "': 'name' too long");
            }
            n.name = *nn;
        }

        auto terrain = DecodeFlags(jn["terrain"], TERRAIN_FLAGS,
                                   "terrain", n.id);
        if (!terrain.ok()) {
            return Gameplay::Fail<WorldMap>(terrain.error,
                                            terrain.reason);
        }
        n.terrain = terrain.value;

        auto strategic = DecodeFlags(jn["strategic"], STRATEGIC_FLAGS,
                                     "strategic", n.id);
        if (!strategic.ok()) {
            return Gameplay::Fail<WorldMap>(strategic.error,
                                            strategic.reason);
        }
        n.strategic = strategic.value;

        auto historical = DecodeFlags(jn["historical"], HIST_FLAGS,
                                      "historical", n.id);
        if (!historical.ok()) {
            return Gameplay::Fail<WorldMap>(historical.error,
                                            historical.reason);
        }
        n.historical = historical.value;

        auto myth = DecodeFlags(jn["myth"], MYTH_FLAGS, "myth", n.id);
        if (!myth.ok()) {
            return Gameplay::Fail<WorldMap>(myth.error, myth.reason);
        }
        n.myth = myth.value;

        auto control = DecodeControl(jn["control"], n.id);
        if (!control.ok()) {
            return Gameplay::Fail<WorldMap>(control.error,
                                            control.reason);
        }
        n.control = control.value;

        if (jn.Has("map") && !jn["map"].IsString()) {
            return Gameplay::Fail<WorldMap>(
                "world", "node '" + n.id + "': 'map' must be a string");
        }
        if (const std::string* mref = jn.FindString("map")) {
            if (mref->size() > WorldLibrary::MAX_REF_LEN) {
                return Gameplay::Fail<WorldMap>(
                    "world", "node '" + n.id + "': 'map' too long");
            }
            n.map = *mref;
        }

        const JsonValue& center = jn["center"];
        if (!center.IsNull()) {
            if (!center.IsArray() || center.Items().size() != 2 ||
                !center.Items()[0].IsInt() || !center.Items()[1].IsInt()) {
                return Gameplay::Fail<WorldMap>(
                    "world",
                    "node '" + n.id + "': 'center' must be [int, int]");
            }
            n.centerX = center.Items()[0].AsInt();
            n.centerY = center.Items()[1].AsInt();
            n.hasCenter = true;
        }

        indexOf.emplace(n.id, map.nodes_.size());
        map.nodes_.push_back(std::move(n));
    }
    map.adjacency_.resize(map.nodes_.size());

    const JsonValue& routes = root["routes"];
    if (!routes.IsNull()) {
        if (!routes.IsArray() || routes.Size() > kMaxRoutes) {
            return Gameplay::Fail<WorldMap>(
                "world", "'routes' must be an array of {a,b,days} "
                         "within bound");
        }
        std::set<std::pair<std::size_t, std::size_t>> seen;
        for (const JsonValue& jr : routes.Items()) {
            if (!jr.IsObject()) {
                return Gameplay::Fail<WorldMap>(
                    "world", "route entry is not an object");
            }
            const std::string* a = jr.FindString("a");
            const std::string* b = jr.FindString("b");
            if (a == nullptr || b == nullptr) {
                return Gameplay::Fail<WorldMap>(
                    "world", "route needs string 'a' and 'b'");
            }
            // Resolve endpoints before the self-route check so
            // {"x","x"} on an unknown node reports the more accurate
            // unknown-node error (BattleMap convention).
            auto ia = indexOf.find(*a);
            auto ib = indexOf.find(*b);
            if (ia == indexOf.end() || ib == indexOf.end()) {
                return Gameplay::Fail<WorldMap>(
                    "world", "route references unknown node ('" + *a +
                             "' -> '" + *b + "')");
            }
            if (*a == *b) {
                return Gameplay::Fail<WorldMap>(
                    "world", "self-route on node '" + *a + "'");
            }
            if (!jr["days"].IsInt() || jr["days"].AsInt() < 1 ||
                jr["days"].AsInt() > WorldLibrary::MAX_ROUTE_DAYS) {
                return Gameplay::Fail<WorldMap>(
                    "world", "route '" + *a + "' -> '" + *b +
                             "': 'days' out of range");
            }
            const std::int64_t days = jr["days"].AsInt();
            const auto key = std::minmax(ia->second, ib->second);
            if (!seen.insert(key).second) {
                return Gameplay::Fail<WorldMap>(
                    "world", "duplicate route ('" + *a + "' -> '" + *b +
                             "')");
            }
            map.adjacency_[ia->second].emplace_back(ib->second, days);
            map.adjacency_[ib->second].emplace_back(ia->second, days);
        }
    }

    // `start` is required and must name a known node.
    if (root.Has("start") && !root["start"].IsString()) {
        return Gameplay::Fail<WorldMap>("world",
                                        "'start' must be a string");
    }
    const std::string* start = root.FindString("start");
    if (start == nullptr || start->empty()) {
        return Gameplay::Fail<WorldMap>("world", "missing 'start'");
    }
    const auto si = indexOf.find(*start);
    if (si == indexOf.end()) {
        return Gameplay::Fail<WorldMap>(
            "world", "'start' names unknown node '" + *start + "'");
    }
    map.start_ = si->second;

    return Gameplay::Ok(std::move(map));
}

const WorldNode* WorldMap::FindNode(std::string_view id) const {
    const std::size_t i = NodeIndexOf(id);
    return i == NO_NODE ? nullptr : &nodes_[i];
}

std::size_t WorldMap::NodeIndexOf(std::string_view id) const {
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].id == id) return i;
    }
    return NO_NODE;
}

WorldLoadResult WorldLibrary::Load(const std::filesystem::path& dir,
                                   WorldLibrary& out) {
    WorldLoadResult res;

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
        // Case-folded extension: X.JSON/X.Json are still content.
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
        res.reason = "world dir unreadable: " + ec.message();
        return res;
    }
    // Deterministic registration order: filename sort.
    std::sort(files.begin(), files.end());
    // Wholesale overflow: committed worlds can't be rolled back
    // per-file (BencaoLibrary precedent).
    if (files.size() > MAX_WORLDS) {
        res.error = "overflow";
        res.reason = "world file count exceeds MAX_WORLDS";
        return res;
    }

    std::vector<WorldMap> worlds;
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
        Result<WorldMap> w = WorldMap::FromJson(doc.value);
        if (!w.ok()) {
            res.rejected.push_back({f, w.error, w.reason});
            continue;
        }
        if (ids.count(w.value.Id())) {
            res.rejected.push_back(
                {f, "duplicate", "world id already registered"});
            continue;
        }
        ids.insert(w.value.Id());
        worlds.push_back(std::move(w.value));
    }

    // Canonical order: world id.
    std::sort(worlds.begin(), worlds.end(),
              [](const WorldMap& a, const WorldMap& b) {
                  return a.Id() < b.Id();
              });
    out.worlds_ = std::move(worlds);
    res.ok = true;
    return res;
}

const WorldMap* WorldLibrary::Find(std::string_view id) const {
    for (const WorldMap& w : worlds_) {
        if (w.Id() == id) return &w;
    }
    return nullptr;
}

} // namespace Potato::Campaign
