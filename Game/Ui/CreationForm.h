#pragma once

#include "Campaign/Characters/Character.h"
#include "Campaign/Rivals/RivalDeck.h" // RivalPrior
#include "Gameplay/Result.h"

#include <string>
#include <vector>

namespace Potato::Game {

// CreationForm — the character-creation screen's view-model
// (Story 12.9). Headless: the form is data + validation; the
// shell renders fields (ImGui) and calls BuildCharacter on
// commit.
//
// Validation is the wire gate itself — BuildCharacter fills a
// Character, serializes it, and re-parses through
// Character::FromJson so a created commander obeys exactly the
// potato.character/1 schema a roster file does. No second
// ruleset to drift.

struct RosterRow {
    std::string id;
    std::string name;
    Campaign::RivalPrior prior = Campaign::RivalPrior::Aggressive;
};

// Predefined generals only (created=false) — a creation screen
// does not offer the player's own authored docs as "roster".
// Canonical order: the library's id sort.
std::vector<RosterRow> RosterList(
    const Campaign::CharacterLibrary& lib);

struct CreationForm {
    // Free strings bounded by Character limits; the wire gate
    // enforces the caps.
    std::string name;
    std::string origin;
    Campaign::RivalPrior prior = Campaign::RivalPrior::Aggressive;
    std::uint64_t deckSeed = 0;
    std::vector<std::string> deck; // optional explicit card ids
};

// → created=true Character on success; the error/reason pair
// names the failing field for the screen to echo.
Gameplay::Result<Campaign::Character> BuildCharacter(
    const CreationForm& form);

} // namespace Potato::Game
