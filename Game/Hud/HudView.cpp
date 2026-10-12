#include "Game/Hud/HudView.h"

#include "Game/Render/SpriteAtlas.h"
#include "Gameplay/Doctrine/Doctrine.h"
#include "Gameplay/Plan/BattlePlan.h"

#include <string>

namespace Potato::Game {

using Gameplay::BattleBeat;
using Gameplay::InterventionKind;
using Gameplay::Result;

const char* HudVerbName(InterventionKind k) {
    switch (k) {
        case InterventionKind::Redirect:       return "redirect";
        case InterventionKind::Override:       return "override";
        case InterventionKind::Retreat:        return "retreat";
        case InterventionKind::Probe:          return "probe";
        case InterventionKind::Entangle:       return "entangle";
        case InterventionKind::Replan:         return "replan";
        case InterventionKind::Execute:        return "execute";
        case InterventionKind::MythPacify:     return "myth_pacify";
        case InterventionKind::MythPossession:
            return "myth_possession";
        case InterventionKind::MythGhostArmy:
            return "myth_ghost_army";
    }
    return "unknown";
}

const HudWidget* HudLayout::Hit(int x, int y) const {
    // Last widget wins — paint order = z order.
    for (auto it = widgets.rbegin(); it != widgets.rend(); ++it) {
        const HudWidget& wgt = *it;
        if (x >= wgt.x && x < wgt.x + wgt.w && y >= wgt.y &&
            y < wgt.y + wgt.h) {
            return &wgt;
        }
    }
    return nullptr;
}

namespace {

// The 1.7 vocabulary, in declaration order — append-only.
constexpr InterventionKind VERBS[] = {
    InterventionKind::Redirect,  InterventionKind::Override,
    InterventionKind::Retreat,   InterventionKind::Probe,
    InterventionKind::Entangle,  InterventionKind::Replan,
    InterventionKind::Execute,   InterventionKind::MythPacify,
    InterventionKind::MythPossession,
    InterventionKind::MythGhostArmy,
};

bool IsMythVerb(InterventionKind k) {
    return k == InterventionKind::MythPacify ||
           k == InterventionKind::MythPossession ||
           k == InterventionKind::MythGhostArmy;
}

constexpr Rgba kCpFill{240, 210, 90, 255};   // brass
constexpr Rgba kCpEmpty{70, 66, 58, 255};
constexpr Rgba kProbe{120, 170, 220, 255};
constexpr Rgba kVerbOn{180, 170, 150, 255};
constexpr Rgba kVerbOff{70, 66, 60, 255};
constexpr Rgba kMythTint{200, 160, 255, 255}; // mandate violet
constexpr Rgba kCard{150, 130, 100, 255};
constexpr Rgba kCool{60, 60, 70, 255};
constexpr Rgba kArrow{150, 190, 130, 255};
constexpr Rgba kHeader{110, 100, 90, 255};

} // namespace

HudLayout BuildHudLayout(const HudInput& in) {
    HudLayout lay;
    lay.w = in.w;
    lay.h = in.h;
    lay.beat = in.beat;
    lay.density = in.density;

    const int bar = in.h - 22; // bottom bar row
    switch (in.beat) {
        case BattleBeat::Execution: {
            // CP pips: filled ≤ cp, hollow ≤ cap.
            for (int i = 0; i < in.cpCap && i < 16; ++i) {
                lay.widgets.push_back(
                    {i < in.cp ? HudCell::CpPip
                               : HudCell::CpEmpty,
                     4 + i * 12, bar, 10, 14,
                     i < in.cp ? kCpFill : kCpEmpty, true, 0});
            }
            // Regen sliver under the pips (0..CP_REGEN_TICKS).
            if (in.density != HudDensity::Minimal) {
                const int frac =
                    Gameplay::CP_REGEN_TICKS > 0
                        ? in.cpRegen * (in.cpCap * 12 - 4) /
                              Gameplay::CP_REGEN_TICKS
                        : 0;
                lay.widgets.push_back(
                    {HudCell::CpRegen, 4, bar + 16,
                     frac < 0 ? 0 : frac, 3,
                     {240, 210, 90, 140}, true, in.cpRegen});
            }
            // Probe budget pips.
            for (int i = 0; i < in.probesLeft && i < 8; ++i) {
                lay.widgets.push_back(
                    {HudCell::ProbePip, 4 + i * 10, bar - 16, 8,
                     8, kProbe, true, 0});
            }
            // Verb palette — all kinds, cp<cost dims not drops.
            int vx = in.w - 12;
            for (std::size_t i =
                     sizeof(VERBS) / sizeof(VERBS[0]);
                 i-- > 0;) { // right-to-left palette
                const InterventionKind k = VERBS[i];
                const int cost =
                    k == InterventionKind::Replan
                        ? in.replanCost
                        : Gameplay::CostOf(k);
                const bool afford = cost <= in.cp;
                const bool myth = IsMythVerb(k);
                vx -= 18;
                lay.widgets.push_back(
                    {HudCell::Verb, vx, bar - 2, 16, 20,
                     myth ? kMythTint
                          : (afford ? kVerbOn : kVerbOff),
                     afford || myth, cost, k});
            }
            break;
        }
        case BattleBeat::Planning: {
            // Sheet panel: per-squad header + card slots.
            int y = 8;
            for (std::size_t s = 0; s < in.sheets.size(); ++s) {
                const auto& sheet = in.sheets[s];
                lay.widgets.push_back(
                    {HudCell::SquadHeader, 4, y, 60, 12, kHeader,
                     true, static_cast<int>(s)});
                y += 14;
                for (std::size_t sl = 0; sl < sheet.slots.size();
                     ++sl) {
                    lay.widgets.push_back(
                        {HudCell::CardTile, 8 + 0, y + 2,
                         14, 18, kCard, true,
                         static_cast<int>(sl)});
                    if (in.density == HudDensity::Full) {
                        // Cooldown meter under the card.
                        const int cd =
                            sheet.slots[sl].cooldownRemaining;
                        lay.widgets.push_back(
                            {HudCell::CooldownBar,
                             8 + 16, y + 2, cd > 60 ? 6 : 1 + cd / 10,
                             18, kCool, true, cd});
                    }
                    y += 22;
                }
            }
            // Arrow rows at right edge (Standard+).
            if (in.density != HudDensity::Minimal) {
                int ay = 8;
                for (std::size_t i = 0; i < in.arrows.size();
                     ++i) {
                    const auto& a = in.arrows[i];
                    if (!a.active) continue;
                    lay.widgets.push_back(
                        {HudCell::ArrowRow, in.w - 70, ay, 66, 10,
                         kArrow, true,
                         static_cast<int>(a.path.size())});
                    ay += 12;
                }
            }
            break;
        }
        case BattleBeat::Aftermath:
            // Marker strip only — the report owns the screen.
            lay.widgets.push_back({HudCell::SquadHeader, 4, bar,
                                   80, 14, kHeader, true, 0});
            break;
    }
    return lay;
}

Result<Framebuffer> RenderHud(const SpriteAtlas& atlas,
                              const HudInput& in) {
    if (in.w <= 0 || in.h <= 0) {
        return Gameplay::Fail<Framebuffer>(
            "hud", "view bounds too small");
    }
    const HudLayout lay = BuildHudLayout(in);
    Framebuffer fb(in.w, in.h);
    for (const HudWidget& wgt : lay.widgets) {
        // Optional tile override; the rect is the fallback
        // shape regardless.
        const char* tileId = nullptr;
        switch (wgt.kind) {
            case HudCell::CpPip:    tileId = "hud.cp"; break;
            case HudCell::CpEmpty:  tileId = "hud.cp_empty";
                break;
            case HudCell::ProbePip: tileId = "hud.probe"; break;
            case HudCell::CardTile: tileId = "hud.slot"; break;
            case HudCell::Verb:
                tileId = nullptr; // verb.<id> looked up below
                break;
            default: break;
        }
        if (wgt.kind == HudCell::Verb) {
            const std::string id =
                std::string("verb.") + HudVerbName(wgt.verb);
            if (const SpriteTile* t = atlas.Find(id)) {
                fb.DrawTile(atlas, *t, wgt.x, wgt.y,
                            wgt.enabled ? 255 : 110);
                continue;
            }
        } else if (tileId != nullptr) {
            if (const SpriteTile* t = atlas.Find(tileId)) {
                fb.DrawTile(atlas, *t, wgt.x, wgt.y, 255);
                continue;
            }
        }
        fb.FillRect(wgt.x, wgt.y, wgt.w, wgt.h, wgt.color);
    }
    return Gameplay::Ok(std::move(fb));
}

} // namespace Potato::Game
