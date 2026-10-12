#include "Gameplay/Map/BattleMap.h"

#include "Gameplay/Json/Json.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace Potato::Gameplay {

namespace {

struct FlagDef {
    const char* token;
    std::uint32_t bit;
};

constexpr FlagDef TERRAIN_FLAGS[] = {
    {"water", TERRAIN_WATER},
    {"river", TERRAIN_RIVER},
    {"road", TERRAIN_ROAD},
    {"forest", TERRAIN_FOREST},
    {"highland", TERRAIN_HIGHLAND},
    {"chokepoint", TERRAIN_CHOKEPOINT},
    {"open", TERRAIN_OPEN},
};
constexpr FlagDef STRATEGIC_FLAGS[] = {
    {"ferry", STRATEGIC_FERRY},
    {"depot", STRATEGIC_DEPOT},
    {"village", STRATEGIC_VILLAGE},
};
constexpr FlagDef HIST_FLAGS[] = {
    {"grain_route", HIST_GRAIN_ROUTE},
    {"telegraph", HIST_TELEGRAPH},
    {"supply_line", HIST_SUPPLY_LINE},
};
constexpr FlagDef MYTH_FLAGS[] = {
    {"shrine", MYTH_SHRINE},
    {"spirit_road", MYTH_SPIRIT_ROAD},
    {"haunted", MYTH_HAUNTED},
};

template <std::size_t N>
Result<std::uint32_t> DecodeFlags(const JsonValue& field,
                                  const FlagDef (&table)[N],
                                  const char* fieldName,
                                  const std::string& regionId) {
    if (field.IsNull()) return Ok<std::uint32_t>(0);
    if (!field.IsArray()) {
        return Fail<std::uint32_t>(
            "map", std::string("region '") + regionId + "': '" + fieldName +
                   "' must be an array of flag strings");
    }
    std::uint32_t bits = 0;
    for (const JsonValue& item : field.Items()) {
        if (!item.IsString()) {
            return Fail<std::uint32_t>(
                "map", std::string("region '") + regionId + "': '" + fieldName +
                       "' entries must be strings");
        }
        const std::string& tok = item.AsString();
        bool known = false;
        for (const FlagDef& def : table) {
            if (tok == def.token) { bits |= def.bit; known = true; break; }
        }
        if (!known) {
            return Fail<std::uint32_t>(
                "map", std::string("region '") + regionId + "': unknown " +
                       fieldName + " flag '" + tok + "'");
        }
    }
    return Ok<std::uint32_t>(bits);
}

} // namespace

Result<BattleMap> BattleMap::Load(std::string_view path) {
    auto doc = Json::Load(path, "potato.map/1");
    if (!doc.ok()) return Fail<BattleMap>(doc.error, doc.reason);
    return FromJson(doc.value);
}

Result<BattleMap> BattleMap::FromJson(const JsonValue& root) {
    if (!root.IsObject()) {
        return Fail<BattleMap>("map", "root is not an object");
    }
    // Callers may hand us ungated DOM — when a schema tag IS present it
    // must still match potato.map/1 (absent = pre-gated/test DOM).
    if (root.Has("schema")) {
        const std::string* s = root.FindString("schema");
        if (s == nullptr || *s != "potato.map/1") {
            return Fail<BattleMap>("schema", "expected potato.map/1");
        }
    }

    BattleMap map;

    if (root.Has("id") && !root["id"].IsString()) {
        return Fail<BattleMap>("map", "'id' must be a string");
    }
    const std::string* id = root.FindString("id");
    if (id == nullptr || id->empty()) {
        return Fail<BattleMap>("map", "missing or empty 'id'");
    }
    map.id_ = *id;
    if (root.Has("name") && !root["name"].IsString()) {
        return Fail<BattleMap>("map", "'name' must be a string");
    }
    if (const std::string* name = root.FindString("name")) map.name_ = *name;

    const JsonValue& regions = root["regions"];
    if (!regions.IsArray() || regions.Items().empty()) {
        return Fail<BattleMap>("map", "'regions' must be a non-empty array");
    }
    if (regions.Items().size() > MAX_REGIONS) {
        return Fail<BattleMap>("map", "region count exceeds MAX_REGIONS");
    }

    std::map<std::string, std::size_t, std::less<>> indexOf;
    for (const JsonValue& jr : regions.Items()) {
        if (!jr.IsObject()) {
            return Fail<BattleMap>("map", "region entry is not an object");
        }
        if (jr.Has("id") && !jr["id"].IsString()) {
            return Fail<BattleMap>("map", "region 'id' must be a string");
        }
        const std::string* rid = jr.FindString("id");
        if (rid == nullptr || rid->empty()) {
            return Fail<BattleMap>("map", "region missing string 'id'");
        }
        if (indexOf.count(*rid) != 0) {
            return Fail<BattleMap>("map", "duplicate region id '" + *rid + "'");
        }

        Region r;
        r.id = *rid;
        if (jr.Has("name") && !jr["name"].IsString()) {
            return Fail<BattleMap>(
                "map", "region '" + r.id + "': 'name' must be a string");
        }
        if (const std::string* rn = jr.FindString("name")) r.name = *rn;

        auto terrain = DecodeFlags(jr["terrain"], TERRAIN_FLAGS, "terrain", r.id);
        if (!terrain.ok()) return Fail<BattleMap>(terrain.error, terrain.reason);
        r.terrain = terrain.value;

        auto strategic = DecodeFlags(jr["strategic"], STRATEGIC_FLAGS, "strategic", r.id);
        if (!strategic.ok()) return Fail<BattleMap>(strategic.error, strategic.reason);
        r.strategic = strategic.value;

        auto historical = DecodeFlags(jr["historical"], HIST_FLAGS, "historical", r.id);
        if (!historical.ok()) return Fail<BattleMap>(historical.error, historical.reason);
        r.historical = historical.value;

        auto myth = DecodeFlags(jr["myth"], MYTH_FLAGS, "myth", r.id);
        if (!myth.ok()) return Fail<BattleMap>(myth.error, myth.reason);
        r.myth = myth.value;

        const JsonValue& center = jr["center"];
        if (!center.IsNull()) {
            if (!center.IsArray() || center.Items().size() != 2 ||
                !center.Items()[0].IsInt() || !center.Items()[1].IsInt()) {
                return Fail<BattleMap>(
                    "map", "region '" + r.id + "': 'center' must be [int, int]");
            }
            r.centerX = center.Items()[0].AsInt();
            r.centerY = center.Items()[1].AsInt();
            r.hasCenter = true;
        }

        indexOf.emplace(r.id, map.regions_.size());
        map.regions_.push_back(std::move(r));
    }
    map.adjacency_.resize(map.regions_.size());

    const JsonValue& edges = root["edges"];
    if (!edges.IsNull()) {
        if (!edges.IsArray()) {
            return Fail<BattleMap>("map", "'edges' must be an array of [id, id] pairs");
        }
        std::set<std::pair<std::size_t, std::size_t>> seen;
        for (const JsonValue& je : edges.Items()) {
            if (!je.IsArray() || je.Items().size() != 2 ||
                !je.Items()[0].IsString() || !je.Items()[1].IsString()) {
                return Fail<BattleMap>("map", "edge must be a [id, id] string pair");
            }
            const std::string& a = je.Items()[0].AsString();
            const std::string& b = je.Items()[1].AsString();
            // Resolve endpoints before the self-edge check so ["x","x"]
            // reports the more accurate unknown-region error.
            auto ia = indexOf.find(a);
            auto ib = indexOf.find(b);
            if (ia == indexOf.end() || ib == indexOf.end()) {
                return Fail<BattleMap>(
                    "map", "edge references unknown region ('" + a + "' -> '" + b + "')");
            }
            if (a == b) {
                return Fail<BattleMap>("map", "self-edge on region '" + a + "'");
            }
            const auto key = std::minmax(ia->second, ib->second);
            if (!seen.insert(key).second) {
                return Fail<BattleMap>(
                    "map", "duplicate edge ('" + a + "' -> '" + b + "')");
            }
            map.adjacency_[ia->second].push_back(ib->second);
            map.adjacency_[ib->second].push_back(ia->second);
        }
    }

    return Ok(std::move(map));
}

const Region* BattleMap::FindRegion(std::string_view id) const {
    const std::size_t i = RegionIndexOf(id);
    return i == NO_REGION ? nullptr : &regions_[i];
}

std::size_t BattleMap::RegionIndexOf(std::string_view id) const {
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        if (regions_[i].id == id) return i;
    }
    return NO_REGION;
}

} // namespace Potato::Gameplay
