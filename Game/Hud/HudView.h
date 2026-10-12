#pragma once

#include "Game/Render/Framebuffer.h"
#include "Gameplay/Command/Intervention.h"
#include "Gameplay/Result.h"
#include "Gameplay/Sim/BattleController.h" // BattleBeat

#include <cstdint>
#include <span>
#include <vector>

namespace Potato::Gameplay {
class DoctrineLibrary;
struct PlanArrow;
struct SquadSheet;
} // namespace Potato::Gameplay

namespace Potato::Game {

class SpriteAtlas;

// HudView — the chrome around the battle map (Story 8.2).
//
// Two-phase: `BuildHudLayout` is pure (widget model = hit rects
// + enabled flags — the surface the input layer hit-tests),
// `RenderHud` paints it. Beat gating mirrors the sim's
// BattleBeat table exactly: Planning authors (sheet/deck/
// arrows), Execution spends (CP + interventions), Aftermath is
// a marker strip (the report owns the screen, Epic 6).

enum class HudDensity : std::uint8_t { Minimal, Standard, Full };

enum class HudCell : std::uint8_t {
    CpPip,       // filled command point
    CpEmpty,     // hollow point slot
    CpRegen,     // regen progress sliver
    ProbePip,    // remaining probe budget
    Verb,        // intervention palette button
    CardTile,    // doctrine card slot (Planning)
    CooldownBar, // slot cooldown meter (Planning)
    ArrowRow,    // plan-arrow summary (Planning)
    SquadHeader, // per-squad grouping cell (Planning)
};

struct HudWidget {
    HudCell kind;
    int x = 0, y = 0, w = 0, h = 0; // hit rect
    Rgba color;
    bool enabled = true; // cp<cost dims; never removes
    int aux = 0;         // verb cost / card slot / path length
    Gameplay::InterventionKind verb =
        Gameplay::InterventionKind::Redirect; // Verb cells only
};

struct HudLayout {
    int w = 0, h = 0;
    Gameplay::BattleBeat beat = Gameplay::BattleBeat::Planning;
    HudDensity density = HudDensity::Standard;
    std::vector<HudWidget> widgets;
    // Hit test for the shell's input layer.
    const HudWidget* Hit(int x, int y) const;
};

struct HudInput {
    Gameplay::BattleBeat beat = Gameplay::BattleBeat::Planning;
    HudDensity density = HudDensity::Standard;
    int w = 320, h = 240;
    // Execution surface
    int cp = 0;
    int cpCap = Gameplay::CP_CAP;
    int cpRegen = 0;            // ticks toward next point
    int probesLeft = 0;
    int replanCost = 2;         // PlanConfig::replanCost
    // Planning surface (observer's squads only)
    const Gameplay::DoctrineLibrary* cards = nullptr;
    std::span<const Gameplay::SquadSheet> sheets;
    std::span<const Gameplay::PlanArrow> arrows;
};

// Pure layout — the deterministic widget model.
HudLayout BuildHudLayout(const HudInput& in);

// Paint the layout. Tile ids are conventional + optional;
// fallback rects ship the shape:
//   hud.cp / hud.cp_empty / hud.probe / hud.slot
//   verb.<id>   (redirect override retreat probe entangle
//                replan execute myth_pacify myth_possession
//                myth_ghost_army)
//   card.<id>   (the slotted doctrine card)
Gameplay::Result<Framebuffer>
RenderHud(const SpriteAtlas& atlas, const HudInput& in);

// Wire spelling for `verb.<id>` tiles — InterventionKind in
// declaration order (the 1.7 append-only vocabulary).
const char* HudVerbName(Gameplay::InterventionKind k);

} // namespace Potato::Game
