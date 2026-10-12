#include "Game/Render/OverworldView.h"

#include "Campaign/World/Encounter.h"
#include "Campaign/World/WorldMap.h"
#include "Campaign/World/WorldState.h"
#include "Game/Render/SpriteAtlas.h"
#include "Game/Render/WorldProjection.h"
#include "Gameplay/Map/BattleMap.h" // TERRAIN_/STRATEGIC_/HIST_/MYTH_* vocab

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

namespace Potato::Game {

using Campaign::EncounterDef;
using Campaign::EncounterLibrary;
using Campaign::WorldControl;
using Campaign::WorldMap;
using Campaign::WorldState;
using Gameplay::Result;

namespace {

// Terrain precedence + tile-id/fallback-color table — same
// visual grammar as BattleView: first flag wins.
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

// Control disposition tints the node — faction is the headline
// fact at world scale.
Rgba ControlTint(WorldControl c) {
    switch (c) {
    case WorldControl::Player: return {70, 140, 220, 255};
    case WorldControl::Rival:  return {200, 70, 60, 255};
    default:                   return {70, 120, 60, 255};
    }
}

struct MarkLook {
    enum class Bits : std::uint8_t { Strategic, Myth, Hist } bits;
    std::uint32_t flag;
    const char* tile;
    Rgba fallback;
};
constexpr MarkLook MARKS[] = {
    {MarkLook::Bits::Strategic, Gameplay::STRATEGIC_VILLAGE,
     "mark.village", {220, 180, 90, 255}},
    {MarkLook::Bits::Strategic, Gameplay::STRATEGIC_FERRY,
     "mark.ferry", {120, 170, 220, 255}},
    {MarkLook::Bits::Strategic, Gameplay::STRATEGIC_DEPOT,
     "mark.depot", {160, 120, 80, 255}},
    {MarkLook::Bits::Hist, Gameplay::HIST_GRAIN_ROUTE,
     "mark.grain_route", {200, 190, 120, 255}},
    {MarkLook::Bits::Hist, Gameplay::HIST_TELEGRAPH,
     "mark.telegraph", {180, 180, 200, 255}},
    {MarkLook::Bits::Hist, Gameplay::HIST_SUPPLY_LINE,
     "mark.supply_line", {170, 150, 100, 255}},
    {MarkLook::Bits::Myth, Gameplay::MYTH_SHRINE,
     "mark.shrine", {240, 220, 140, 255}},
    {MarkLook::Bits::Myth, Gameplay::MYTH_SPIRIT_ROAD,
     "mark.spirit_road", {170, 140, 220, 255}},
    {MarkLook::Bits::Myth, Gameplay::MYTH_HAUNTED,
     "mark.haunted", {140, 90, 160, 255}},
};

void Stamp(Framebuffer& fb, const SpriteAtlas& atlas,
           const char* tile, int cx, int cy, Rgba fallback) {
    if (const SpriteTile* t = atlas.Find(tile)) {
        const int tp = atlas.TilePx();
        fb.DrawTile(atlas, *t, cx - tp / 2, cy - tp / 2, 255);
    } else {
        fb.FillRect(cx - 3, cy - 3, 6, 6, fallback);
    }
}

} // namespace

Result<Framebuffer>
RenderOverworld(const WorldMap& map, const WorldState& ws,
                const SpriteAtlas& atlas,
                const OverworldInput& in) {
    auto proj = WorldProjection::Build(map, in.w, in.h, in.margin);
    if (!proj.ok()) {
        return Gameplay::Fail<Framebuffer>(proj.error, proj.reason);
    }
    const int np = in.nodePx < 4 ? 4 : in.nodePx;
    const int half = np / 2;

    Framebuffer fb(in.w, in.h);
    fb.Clear({18, 16, 22, 255}); // ink-night ground

    // 1. Route edges — the marchable graph.
    for (const auto& [a, b] : proj.value.Edges()) {
        const ProjectedNode& pa = proj.value.At(a);
        const ProjectedNode& pb = proj.value.At(b);
        fb.DrawLine(pa.x, pa.y, pb.x, pb.y, {90, 80, 70, 200});
    }

    // 2. Node tiles — terrain fill tinted by control; a resolved
    //    node draws a subdued check so spent ground reads spent.
    for (std::size_t i = 0; i < map.NodeCount(); ++i) {
        const auto& nd = map.NodeAt(i);
        const ProjectedNode& pn = proj.value.At(i);
        const TerrainLook* look = &TERRAIN[7];
        for (const TerrainLook& t : TERRAIN) {
            if (t.flag != 0 && (nd.terrain & t.flag) != 0) {
                look = &t;
                break;
            }
        }
        const Rgba tint = ControlTint(ws.ControlAt(nd.id));
        if (const SpriteTile* t = atlas.Find(look->tile)) {
            const int tp = atlas.TilePx();
            for (int y = pn.y - half; y < pn.y + half; y += tp) {
                for (int x = pn.x - half; x < pn.x + half;
                     x += tp) {
                    fb.DrawTile(atlas, *t, x, y, 255);
                }
            }
            // Same fold as the fallback — tint veils the tiles so
            // faction reads at a glance, terrain stays legible.
            fb.FillRect(pn.x - half, pn.y - half, np, np,
                        {tint.r, tint.g, tint.b, 140});
        } else {
            Rgba c = look->fallback;
            // Fold control into the fill — tint toward the
            // faction color, terrain stays legible underneath.
            c.r = static_cast<std::uint8_t>(
                (c.r + tint.r) / 2);
            c.g = static_cast<std::uint8_t>(
                (c.g + tint.g) / 2);
            c.b = static_cast<std::uint8_t>(
                (c.b + tint.b) / 2);
            fb.FillRect(pn.x - half, pn.y - half, np, np, c);
        }
        // Faction border — 1px frame, always crisp.
        fb.DrawLine(pn.x - half, pn.y - half,
                    pn.x + half - 1, pn.y - half, tint);
        fb.DrawLine(pn.x - half, pn.y + half - 1,
                    pn.x + half - 1, pn.y + half - 1, tint);
        fb.DrawLine(pn.x - half, pn.y - half,
                    pn.x - half, pn.y + half - 1, tint);
        fb.DrawLine(pn.x + half - 1, pn.y - half,
                    pn.x + half - 1, pn.y + half - 1, tint);
    }

    // 3. POI marks — strategic / historical / myth overlays,
    //    stamped at node centers in table order.
    for (std::size_t i = 0; i < map.NodeCount(); ++i) {
        const auto& nd = map.NodeAt(i);
        const ProjectedNode& pn = proj.value.At(i);
        for (const MarkLook& m : MARKS) {
            const std::uint32_t bits =
                m.bits == MarkLook::Bits::Strategic ? nd.strategic
                : m.bits == MarkLook::Bits::Myth ? nd.myth
                                               : nd.historical;
            if ((bits & m.flag) == 0) continue;
            Stamp(fb, atlas, m.tile, pn.x, pn.y - half - 4,
                  m.fallback);
        }
        if (ws.IsResolved(nd.id)) {
            Stamp(fb, atlas, "resolved", pn.x + half - 2,
                  pn.y - half + 2, {160, 160, 170, 255});
        }
    }

    // 4. Warband — the player's last army on the map. An
    //    in-flight march draws a translucent line to the
    //    destination plus ETA pips (one pip per remaining day,
    //    capped at 6 for readability).
    const std::string& wb = ws.WarbandAt();
    if (!wb.empty()) {
        const std::size_t wi = map.NodeIndexOf(wb);
        if (wi != WorldMap::NO_NODE) {
            const ProjectedNode& pn = proj.value.At(wi);
            Stamp(fb, atlas, "warband", pn.x, pn.y,
                  {250, 240, 210, 255});
        }
        if (ws.Marching()) {
            const std::size_t di =
                map.NodeIndexOf(ws.MarchDest());
            if (wi != WorldMap::NO_NODE &&
                di != WorldMap::NO_NODE) {
                const ProjectedNode& pa = proj.value.At(wi);
                const ProjectedNode& pd = proj.value.At(di);
                // Halftone march line — 3px on / 1px off, opaque:
                // the army's route must read above the faint
                // route edges, not blend into them.
                const int dx = pd.x - pa.x, dy = pd.y - pa.y;
                const int len =
                    std::max(std::abs(dx), std::abs(dy));
                if (len > 0) {
                    for (int i = 0; i <= len; ++i) {
                        if (i % 4 == 3) continue;
                        fb.SetPixel(pa.x + dx * i / len,
                                    pa.y + dy * i / len,
                                    {250, 240, 210, 255});
                    }
                }
                const std::int64_t left =
                    ws.MarchEta() - ws.Day();
                const int pips = static_cast<int>(
                    std::clamp<std::int64_t>(left, 0, 6));
                for (int k = 0; k < pips; ++k) {
                    fb.FillRect(pd.x - 1, pd.y - half - 6 - k * 3,
                                2, 2, {250, 240, 210, 255});
                }
            }
        }
    }

    // 5. Encounters — a banner flies at the node's bottom-right
    //    only while the trigger is firing NOW (armed-but-quiet
    //    encounters draw nothing); once resolved, a subdued
    //    "resolved" stamp takes the corner where the banner was.
    if (in.encounters != nullptr) {
        const auto firing =
            PendingEncounters(ws, map, *in.encounters);
        for (const EncounterDef& enc :
             in.encounters->Encounters()) {
            const std::size_t ni = map.NodeIndexOf(enc.node);
            if (ni == WorldMap::NO_NODE) continue;
            const ProjectedNode& pn = proj.value.At(ni);
            if (ws.IsResolved(enc.id)) {
                Stamp(fb, atlas, "resolved", pn.x + half - 2,
                      pn.y + half - 2, {160, 160, 170, 255});
                continue;
            }
            const bool lit = std::any_of(
                firing.begin(), firing.end(),
                [&](const EncounterDef* e) {
                    return e->id == enc.id;
                });
            if (lit) {
                Stamp(fb, atlas, "banner", pn.x + half - 2,
                      pn.y + half - 2, {220, 60, 60, 255});
            }
        }
    }

    // 6. Sightings — hearsay marks; alpha decays with staleness
    //    but floors at 60: an old rumor fades, never deletes.
    const std::int64_t horizon =
        in.staleDays < 1 ? 1 : in.staleDays;
    for (const auto& [node, mark] : ws.Sightings()) {
        const std::size_t ni = map.NodeIndexOf(node);
        if (ni == WorldMap::NO_NODE) continue;
        const std::int64_t age = ws.Day() - mark.day;
        const int fresh = static_cast<int>(
            std::clamp<std::int64_t>(horizon - age, 0, horizon));
        const int a = 60 + fresh * 195 /
                          static_cast<int>(horizon);
        const ProjectedNode& pn = proj.value.At(ni);
        if (const SpriteTile* t = atlas.Find("sighting")) {
            const int tp = atlas.TilePx();
            fb.DrawTile(atlas, *t, pn.x - half - 2,
                        pn.y + half - tp, 255);
        }
        fb.FillRect(pn.x - half, pn.y + half - 4, 8, 4,
                    {200, 170, 255,
                     static_cast<std::uint8_t>(a)});
    }

    return Gameplay::Ok(std::move(fb));
}

} // namespace Potato::Game
