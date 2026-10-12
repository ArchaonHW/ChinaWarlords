#pragma once

#include "Game/Render/Framebuffer.h"
#include "Gameplay/Result.h"

#include <span>

namespace Potato::Gameplay {
class BattleMap;
class QuantumFog;
struct Squad;
} // namespace Potato::Gameplay

namespace Potato::Game {

class SpriteAtlas;

// BattleView — the frame the player sees (Story 8.1).
//
// TRUTH BOUNDARY: the input is the OBSERVER's slice — public
// terrain (BattleMap), the observer's own squads, and the
// belief layer (QuantumFog). True enemy state is never a
// parameter here; what the shell hands in is what renders.
// Enemy presence draws as superposed clouds at believedRegion
// with alpha ∝ certainty — the fog is the picture, literally.
//
// Render order is canonical and deterministic: region tiles in
// file order → edges → marks → certainty grain → own units →
// clouds. Same inputs produce a byte-identical frame
// (Framebuffer::Hash pins it).

struct BattleViewInput {
    const Gameplay::QuantumFog* fog = nullptr; // may be null
    std::span<const Gameplay::Squad> ownSquads;
    int w = 320, h = 240;         // output dims, clamped
    int regionPx = 40;            // region tile footprint
    int grainBelow = 60;          // certainty < this → grain
    int margin = 8;
};

Gameplay::Result<Framebuffer>
RenderBattleView(const Gameplay::BattleMap& map,
                 const SpriteAtlas& atlas,
                 const BattleViewInput& in);

} // namespace Potato::Game
