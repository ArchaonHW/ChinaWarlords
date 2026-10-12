#pragma once

#include "Game/Render/Framebuffer.h"
#include "Gameplay/Result.h"

namespace Potato::Campaign {
class EncounterLibrary;
class WorldMap;
class WorldState;
} // namespace Potato::Campaign

namespace Potato::Game {

class SpriteAtlas;

// OverworldView — the campaign map the player sees (Story 12.9).
// WorldMap + WorldState → Framebuffer: region tiles, route
// edges, POI marks, the warband, armed-encounter banners, and
// hearsay sightings.
//
// TRUTH BOUNDARY: rival presence renders ONLY from
// WorldState::Sightings — hearsay is the data; there is no
// enemy-warband truth at world scale to leak.
//
// The view never calls WorldState::Draw() — draw order is the
// sim's canonical stream, not presentation's.
//
// Render order is canonical and deterministic: route edges →
// node tiles → POI marks → warband/march → encounter banners →
// sightings. Same inputs produce a byte-identical frame
// (Framebuffer::Hash pins it).

struct OverworldInput {
    const Campaign::EncounterLibrary* encounters = nullptr;
    int w = 640, h = 400;       // output dims, clamped
    int margin = 24;
    int nodePx = 20;            // node tile footprint
    int staleDays = 7;          // sighting alpha fade horizon
};

Gameplay::Result<Framebuffer>
RenderOverworld(const Campaign::WorldMap& map,
                const Campaign::WorldState& ws,
                const SpriteAtlas& atlas,
                const OverworldInput& in);

} // namespace Potato::Game
