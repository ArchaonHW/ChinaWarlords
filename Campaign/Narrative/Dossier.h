#pragma once

#include "Campaign/Rivals/RivalDeck.h" // GeneralDossier

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Potato::Campaign {

// Story 6.3 — the hearsay dossier. A rival general renders as
// rumor, never as a stat sheet: every claim opens with a hearsay
// marker (據聞/或曰/傳言), the aggregate numbers stay sealed, and
// each claim carries a confidence tier that later facts can
// overturn — the dossier admits it may be wrong.

enum class ClaimKind : std::uint8_t {
    Temperament = 0, // who the rumors say they are (prior)
    Habit = 1,       // what their scouts say about us (triggers)
    Repute = 2,      // how long the name has circulated
};
constexpr std::size_t kClaimKindCount = 3;

struct DossierClaim {
    ClaimKind kind = ClaimKind::Temperament;
    std::string text; // hearsay-marked sentence
    // 0 = 風聞未確 (thin rumor), 1 = 屢聞 (pattern held across
    // MIN_CHAPTERS_TO_LEARN chapters). The tracked uncertainty —
    // a tier, never a count.
    int tier = 0;
    bool revised = false; // proven-wrong mark (IntelLedger, 6.4)
};

// Derives the claim set from the aggregates. A dossier never
// observed (chaptersObserved == 0) yields a single thin Repute
// claim — the name alone, nothing more.
std::vector<DossierClaim> DossierClaims(const GeneralDossier& d);

// Renders the dossier page. `provenWrong` marks claim kinds whose
// assertions later events refuted — they still render, annotated
// (the rumor is corrected, not erased).
std::string RenderDossier(
    const GeneralDossier& d,
    std::span<const ClaimKind> provenWrong = {});

} // namespace Potato::Campaign
