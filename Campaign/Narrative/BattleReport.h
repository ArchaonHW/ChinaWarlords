#pragma once

#include "Gameplay/Doctrine/Doctrine.h" // SimEvent

#include <cstdint>
#include <span>
#include <string>

namespace Potato::Campaign {

// Story 6.1 — the post-battle chronicle page. Folds a resolved
// battle's SimEvent list into 史官體 prose: impersonal, third-
// person, numbers banished to elliptical counts ("傷亡甚多", never
// a tally). Clause pools pick by count parity — deterministic, no
// PRNG in the text layer.
//
// Omission is style, not concealment: events beneath chronicle
// notice (doctrine churn, sustain heartbeats) are confessed by
// count — 本報告省略 N 項 always prints, N=0 included.
//
// `historianSide` attributes deeds: our deeds are 王師's, the
// enemy's are 敵's. Side-less events (infiltration, invasion)
// belong to no banner and narrate as portents.
//
// `seedLevels` is the optional carry-in infiltration table
// (MythField::Levels()) — InfiltrationChanged events carry only
// the NEW level, so without the carry-in baseline a pacification
// of seeded haunted ground misreads as a fresh incursion. Pass it
// when rendering a real battle; omit for event-only narratives
// (then each region's first sighting is presumed to rise from 0).
//
// `bcodex`/`blib` (Story 10.5) are optional citation sources:
// when both are non-null a 書吏按 colophon rides after the
// confession line, naming only already-unlocked entries.
// Omit either and output is byte-identical to the unaided
// render — the citation layer is removable by construction.
// (codex first — the unlock state is the gate, the library is
// reference data.)
class BencaoCodex;
class BencaoLibrary;
std::string RenderBattleReport(
    std::span<const Gameplay::SimEvent> events, int historianSide,
    std::span<const std::uint8_t> seedLevels = {},
    const BencaoCodex* bcodex = nullptr,
    const BencaoLibrary* blib = nullptr);

// The elliptical ladder — the only numbers the register permits.
// 0 -> empty (nothing is said rather than "zero"), 1-2 一二,
// 3-9 數, 10-49 數十, 50-199 百餘, 200+ 不可勝計.
std::string_view EllipticalCount(std::size_t n);

} // namespace Potato::Campaign
