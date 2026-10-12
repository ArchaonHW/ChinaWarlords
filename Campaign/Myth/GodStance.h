#pragma once

#include "Gameplay/Myth/Infiltration.h" // GodStance

#include <string_view>

namespace Potato::Campaign {

// Stance → narrative surface (Story 5.7: "readable in the
// narrative layer — dossier/shrine text variants"). The deity's
// mood renders as shrine text, not a stat: the folk register for
// the sacred ground itself. Epic 8 owns typography; this is the
// variant selector.
//
// The register is observed mood, not measured disposition — a
// shrine never displays "Wrathful(-1)", it says the crows won't
// leave.
inline std::string_view ShrineMoodText(Gameplay::GodStance stance) {
    switch (stance) {
        case Gameplay::GodStance::Favorable:
            return "神悅——廟祝說香火正旺，簽詩屢得吉。";
        case Gameplay::GodStance::Neutral:
            return "不聞——神明沉默，香火如常。";
        case Gameplay::GodStance::Wrathful:
            return "神怒——廟前烏鴉不散，卜者不敢近。";
    }
    return "不聞——神明沉默。";
}

} // namespace Potato::Campaign
