#pragma once

#include "Campaign/Governance/Victory.h" // ChapterResolution
#include "Gameplay/Result.h"

#include <cstdint>

namespace Potato::Campaign {

class ChapterLibrary;
class CampaignState;
class Ledger;

// 嚇阻優於接戰 (Story 7.6) — the deterrence eval: the third
// chapter-victory path's gate for zero-combat chapters
// (ChapterDef.combat == false). An enemy that yields without a
// battle yields TO our standing — deterrence reads 軍威, the
// account that deeds bank when the realm is made to kneel instead
// of bleed.
//
// Pure fold discipline, same as the victory thresholds (4.4): the
// ONLY way 軍威 moves is a posting — there is no stat to write.
// A deterrence success is reported to ConcludeChapter /
// SettleEncounter as battleWon + subverted, sealing
// "resolution:subversion" — the negotiation IS a subversion
// outcome, not a fifth resolution kind.
//
// Initial balance target, not final tuning — deeds credit 軍威
// in single/double digits, so the gate prices a reputation
// roughly one won campaign's standing deep.
constexpr std::int64_t DETERRENCE_PRESTIGE = 50;

// Pure predicate — reads the whole chain, writes nothing.
bool DeterrenceMet(const Ledger& l);

// The negotiated chapter resolution (Epic 7.6) — the driver for
// ChapterDef.combat == false chapters, which have no field to win.
// Gates, in order:
//   1. the shared CanResolveChapter gate (same list ConcludeChapter
//      enforces — mirrored by delegation, not duplicated)
//   2. the chapter in play must be designated non-combat — a combat
//      chapter cannot conclude by parley
//   3. DeterrenceMet — 軍威 is the standing the enemy yields to
//
// A deterrence miss is NOT a defeat: negotiation-pending returns
// Fail with the chapter untouched — no seal posts, progression
// stays in play until the standing is there. On success this calls
// ConcludeChapter(battleWon=true, subverted=true) — the seal reads
// resolution:subversion, and the governance thresholds may still
// override upward exactly as they do for a field resolution.
Gameplay::Result<ChapterResolution> ResolveZeroCombatChapter(
    CampaignState& state, const ChapterLibrary& lib);

} // namespace Potato::Campaign
