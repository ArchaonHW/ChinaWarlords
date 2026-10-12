#include "Game/Render/BattleView.h"

#include "Game/Render/MapProjection.h"
#include "Game/Render/SpriteAtlas.h"
#include "Gameplay/Fog/QuantumFog.h"
#include "Gameplay/Map/BattleMap.h"
#include "Gameplay/Squad/Squad.h"

#include <cstdint>
#include <map>
#include <string>

namespace Potato::Game {

using Gameplay::BattleMap;
using Gameplay::CloudEntity;
using Gameplay::QuantumFog;
using Gameplay::Region;
using Gameplay::Result;
using Gameplay::Squad;

namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

// Terrain precedence + tile-id/fallback-color table. First
// flag wins — a river-through-forest reads as river.
struct TerrainLook {
    std::uint32_t flag;
    const char* tile;
    Rgba fallback;
};
constexpr TerrainLook TERRAIN[] = {
    {Gameplay::TERRAIN_WATER, "terrain.water", {40, 80, 140, 255}},
    {Gameplay::TERRAIN_RIVER, "terrain.river", {60, 110, 160, 255}},
    {Gameplay::TERRAIN_FOREST, "terrain.forest", {34, 90, 40, 255}},
    {Gameplay::TERRAIN_HIGHLAND, "terrain.highland",
     {120, 100, 70, 255}},
    {Gameplay::TERRAIN_CHOKEPOINT, "terrain.chokepoint",
     {90, 80, 60, 255}},
    {Gameplay::TERRAIN_ROAD, "terrain.road", {110, 90, 60, 255}},
    {Gameplay::TERRAIN_OPEN, "terrain.open", {70, 120, 60, 255}},
    {0, "terrain.none", {50, 50, 50, 255}},
};

struct MarkLook {
    std::uint32_t flag;
    bool myth;                  // read r.myth vs r.strategic
    const char* tile;
    Rgba fallback;
};
constexpr MarkLook MARKS[] = {
    {Gameplay::STRATEGIC_VILLAGE, false, "mark.village",
     {220, 180, 90, 255}},
    {Gameplay::STRATEGIC_FERRY, false, "mark.ferry",
     {120, 170, 220, 255}},
    {Gameplay::STRATEGIC_DEPOT, false, "mark.depot",
     {160, 120, 80, 255}},
    {Gameplay::MYTH_SHRINE, true, "mark.shrine",
     {240, 220, 140, 255}},
    {Gameplay::MYTH_HAUNTED, true, "mark.haunted",
     {140, 90, 160, 255}},
    {Gameplay::MYTH_SPIRIT_ROAD, true, "mark.spirit_road",
     {170, 140, 220, 255}},
};

std::uint64_t SpeckleKey(std::string_view id, int x, int y) {
    std::uint64_t h = kFnvOffset;
    for (const char c : id) {
        h ^= static_cast<std::uint8_t>(c);
        h *= kFnvPrime;
    }
    h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(x));
    h *= kFnvPrime;
    h ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(y));
    h *= kFnvPrime;
    return h;
}

} // namespace

Result<Framebuffer>
RenderBattleView(const BattleMap& map, const SpriteAtlas& atlas,
                 const BattleViewInput& in) {
    auto proj = MapProjection::Build(map, in.w, in.h, in.margin);
    if (!proj.ok()) {
        return Gameplay::Fail<Framebuffer>(proj.error, proj.reason);
    }
    const int rp = in.regionPx < 4 ? 4 : in.regionPx;
    const int half = rp / 2;

    Framebuffer fb(in.w, in.h);
    fb.Clear({18, 16, 22, 255}); // ink-night ground

    // 1. Region terrain tiles, canonical file order.
    for (std::size_t i = 0; i < map.RegionCount(); ++i) {
        const Region& r = map.RegionAt(i);
        const ProjectedRegion& pr = proj.value.At(i);
        const TerrainLook* look = &TERRAIN[7];
        for (const TerrainLook& t : TERRAIN) {
            if (t.flag != 0 && (r.terrain & t.flag) != 0) {
                look = &t;
                break;
            }
        }
        int shade = 255;
        if (in.fog != nullptr) {
            const int c = in.fog->CertaintyAt(i);
            // Dim by certainty — floor at half-brightness, never
            // black: the map stays readable, uncertainty grays.
            shade = 128 + c * 127 / 100;
        }
        if (const SpriteTile* t = atlas.Find(look->tile)) {
            // The tile repeats across the region footprint —
            // pixel-art tiles tile; a single centered stamp
            // would read as a point, not a field.
            const int tp = atlas.TilePx();
            for (int y = pr.y - half; y < pr.y + half; y += tp) {
                for (int x = pr.x - half; x < pr.x + half;
                     x += tp) {
                    fb.DrawTile(atlas, *t, x, y, shade);
                }
            }
        } else {
            Rgba c = look->fallback;
            c.r = static_cast<std::uint8_t>(c.r * shade / 255);
            c.g = static_cast<std::uint8_t>(c.g * shade / 255);
            c.b = static_cast<std::uint8_t>(c.b * shade / 255);
            fb.FillRect(pr.x - half, pr.y - half, rp, rp, c);
        }
    }

    // 2. Route edges — under units, over terrain.
    for (const auto& [a, b] : proj.value.Edges()) {
        const ProjectedRegion& pa = proj.value.At(a);
        const ProjectedRegion& pb = proj.value.At(b);
        fb.DrawLine(pa.x, pa.y, pb.x, pb.y, {90, 80, 70, 200});
    }

    // 3. Marks: strategic/myth overlays stamped at center.
    for (std::size_t i = 0; i < map.RegionCount(); ++i) {
        const Region& r = map.RegionAt(i);
        const ProjectedRegion& pr = proj.value.At(i);
        for (const MarkLook& m : MARKS) {
            const bool on =
                m.myth ? (r.myth & m.flag) != 0
                       : (r.strategic & m.flag) != 0;
            if (!on) continue;
            if (const SpriteTile* t = atlas.Find(m.tile)) {
                const int tp = atlas.TilePx();
                fb.DrawTile(atlas, *t, pr.x - tp / 2,
                            pr.y - tp / 2, 255);
            } else {
                fb.FillRect(pr.x - 3, pr.y - 3, 6, 6, m.fallback);
            }
        }
    }

    // 4. Certainty grain — speckle, not blackout.
    if (in.fog != nullptr) {
        for (std::size_t i = 0; i < map.RegionCount(); ++i) {
            const int c = in.fog->CertaintyAt(i);
            if (c >= in.grainBelow) continue;
            const Region& r = map.RegionAt(i);
            const ProjectedRegion& pr = proj.value.At(i);
            for (int y = pr.y - half; y < pr.y + half; ++y) {
                for (int x = pr.x - half; x < pr.x + half; ++x) {
                    if ((SpeckleKey(r.id, x, y) & 7) == 0) {
                        fb.BlendPixel(x, y, {10, 10, 14, 110});
                    }
                }
            }
        }
    }

    // 5. Own squads — the observer's truth. Deterministic
    //    stacking offset per region-occupancy order.
    std::map<std::size_t, int> stack;
    for (const Squad& s : in.ownSquads) {
        if (s.regionIndex == Squad::NO_REGION ||
            s.regionIndex >= map.RegionCount()) {
            continue;
        }
        const ProjectedRegion& pr =
            proj.value.At(s.regionIndex);
        const int n = stack[s.regionIndex]++;
        const int ox = n * 8;
        if (const SpriteTile* t = atlas.Find("unit")) {
            const int tp = atlas.TilePx();
            fb.DrawTile(atlas, *t, pr.x - tp / 2 + ox,
                        pr.y - tp / 2 - rp / 4, 255);
        } else {
            fb.FillRect(pr.x - 4 + ox, pr.y - 8, 8, 8,
                        {230, 230, 240, 255});
        }
    }

    // 6. Clouds — superposed belief markers, alpha ∝ certainty.
    if (in.fog != nullptr) {
        for (const CloudEntity& cl : in.fog->Clouds()) {
            if (cl.believedRegion < 0 ||
                static_cast<std::size_t>(cl.believedRegion) >=
                    map.RegionCount()) {
                continue;
            }
            const ProjectedRegion& pr =
                proj.value.At(
                    static_cast<std::size_t>(cl.believedRegion));
            const int a = (cl.certainty < 0 ? 0
                           : (cl.certainty > 100 ? 100
                                                 : cl.certainty)) *
                          255 / 100;
            if (const SpriteTile* t = atlas.Find("cloud")) {
                const int tp = atlas.TilePx();
                fb.DrawTile(atlas, *t, pr.x - tp / 2 + 6,
                            pr.y - tp / 2 + 6, 255);
            }
            fb.FillRect(pr.x + 2, pr.y + 2, 10, 10,
                        {200, 170, 255,
                         static_cast<std::uint8_t>(a)});
        }
    }

    return Gameplay::Ok(std::move(fb));
}

} // namespace Potato::Game
