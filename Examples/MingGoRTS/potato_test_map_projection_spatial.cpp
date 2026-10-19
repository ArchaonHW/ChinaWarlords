#include "Game/Render/MapProjection.h"
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Map/BattleMap.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* name) {
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
}

Potato::Gameplay::BattleMap MakeMap() {
    std::string doc =
        R"({"schema":"potato.map/1","id":"spatial_hit_test","regions":[)";
    for (int i = 0; i < 36; ++i) {
        if (i != 0) doc += ',';
        const int x = i == 0 ? 0 : i == 1 ? 7 : i == 2 ? 9 : 52;
        doc += "{\"id\":\"r" + std::to_string(i) +
               "\",\"center\":[" + std::to_string(x) + ",0]}";
    }
    doc += R"(],"edges":[]})";
    const auto parsed = Potato::Gameplay::JsonValue::Parse(doc);
    if (!parsed.ok()) return {};
    const auto result = Potato::Gameplay::BattleMap::FromJson(parsed.value);
    return result.ok() ? result.value : Potato::Gameplay::BattleMap{};
}

Potato::Gameplay::BattleMap MakeExtremeHintMap() {
    const std::int64_t min = std::numeric_limits<std::int64_t>::min();
    const std::string doc =
        "{\"schema\":\"potato.map/1\",\"id\":\"extreme_hints\",\"regions\":["
        "{\"id\":\"min\",\"center\":[" + std::to_string(min) + ",0]},"
        "{\"id\":\"quarter\",\"center\":[" +
        std::to_string(min / 2 - 1) + ",0]},"
        "{\"id\":\"middle\",\"center\":[0,0]},"
        "{\"id\":\"three_quarters\",\"center\":[" +
        std::to_string(std::numeric_limits<std::int64_t>::max() / 2) + ",0]},"
        "{\"id\":\"max\",\"center\":[" +
        std::to_string(std::numeric_limits<std::int64_t>::max()) +
        ",0]}],\"edges\":[]}";
    const auto parsed = Potato::Gameplay::JsonValue::Parse(doc);
    if (!parsed.ok()) return {};
    const auto result = Potato::Gameplay::BattleMap::FromJson(parsed.value);
    return result.ok() ? result.value : Potato::Gameplay::BattleMap{};
}

int ReferenceHit(const std::vector<Potato::Game::ProjectedRegion>& regions,
                 int x, int y, int radius) {
    if (radius < 0) return -1;
    const std::int64_t r = radius;
    const std::int64_t rr = r * r;
    std::int64_t bestDistance = rr + 1;
    int best = -1;
    for (const auto& region : regions) {
        const std::int64_t dx = static_cast<std::int64_t>(x) - region.x;
        const std::int64_t dy = static_cast<std::int64_t>(y) - region.y;
        if (dx < -r || dx > r || dy < -r || dy > r) continue;
        const std::int64_t distance = dx * dx + dy * dy;
        if (distance <= rr &&
            (distance < bestDistance ||
             (distance == bestDistance &&
              (best < 0 || region.index <
                               static_cast<std::size_t>(best))))) {
            bestDistance = distance;
            best = static_cast<int>(region.index);
        }
    }
    return best;
}

} // namespace

int main() {
    const auto extremeMap = MakeExtremeHintMap();
    const auto extremeProjection =
        Potato::Game::MapProjection::Build(extremeMap, 100, 60, 2);
    Check(extremeProjection.ok(),
          "full-range integer map hints project safely");
    if (extremeProjection.ok()) {
        const auto& projected = extremeProjection.value;
        Check(projected.At(0).x == 2 && projected.At(1).x == 25 &&
                  projected.At(2).x == 50 && projected.At(3).x == 73 &&
                  projected.At(4).x == 98,
              "full-range hints map fractional boundaries with integer-floor rounding");
    }

    const Potato::Gameplay::BattleMap map = MakeMap();
    Check(map.RegionCount() == 36, "fixture has enough regions to build grid");
    const auto projection = Potato::Game::MapProjection::Build(map, 100, 60, 24);
    Check(projection.ok(), "projection builds indexed region centers");
    Check(!Potato::Game::MapProjection::Build(
               map, 100, 60, std::numeric_limits<int>::max())
               .ok(),
          "margin too large for view rejects without integer overflow");
    if (projection.ok()) {
        Check(projection.value.At(1).x == 31 &&
                  projection.value.At(2).x == 33,
              "fixture puts equal-distance nodes across cell boundary");
        Check(projection.value.Hit(32, 30, 2) == 1,
              "equal-distance hit selects lowest region index");
        Check(projection.value.Hit(-100, -100, 0) == -1 &&
                  projection.value.Hit(31, 30, -1) == -1,
              "outside position and negative radius reject");
        Check(projection.value.Hit(31, 30,
                                  std::numeric_limits<int>::max()) == 1,
              "broad radius falls back without huge cell scan");

        for (int x = -16; x <= 116; x += 7) {
            for (int y = -16; y <= 76; y += 11) {
                for (const int radius : {0, 8, 24, 80, 700}) {
                    Check(projection.value.Hit(x, y, radius) ==
                              ReferenceHit(projection.value.Regions(), x, y,
                                           radius),
                          "indexed hit matches linear reference");
                }
            }
        }
    }
    return failures == 0 ? 0 : 1;
}
