#pragma once

#include "Campaign/Characters/Character.h"
#include "Campaign/Rivals/RivalDeck.h" // RivalPrior
#include "Gameplay/Result.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay {
class DoctrineLibrary;
}

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

// ---- Story 13.5: S1 normalization ------------------------------
//
// Two more headless surfaces on the same screen: per-field error
// echo (the gate stays authoritative; the view-model only names
// the field a reason points at) and the starting-deck preview —
// the player picks a person by what they'll play, not just a
// name.

enum class CreationField : std::uint8_t {
    Name = 0,
    Origin,
    Prior,
    DeckSeed,
    Deck,
};

struct FieldError {
    CreationField field = CreationField::Name;
    std::string message;
};

// Per-field validation echoing the same bounds the wire gate
// enforces — cheap pre-checks only; BuildCharacter remains the
// authority (a form passing ValidateForm can still fail the
// gate, and the commit echoes the gate's reason).
std::vector<FieldError> ValidateForm(const CreationForm& form);

// Map an error reason (Preflight's or the wire gate's) onto the
// field the shell should underline. Unrecognized → Deck: a
// commit-level fault with no field to point at.
CreationField FieldForReason(std::string_view reason);

// Deterministic starting-deck roll — what `deckSeed` means.
// Draws a count in [3,5] then that many DISTINCT card indices
// (draw % Count) from a dedicated SplitMix64 stream seeded by
// the seed alone — same-stream convention as the world oracle:
// the roll never touches sim/world streams. Emitted in library
// index order (canonical, not draw order). Empty library →
// empty roll.
std::vector<std::string> RollDeck(
    std::uint64_t seed, const Gameplay::DoctrineLibrary& lib);

struct DeckPreviewRow {
    std::string id;
    std::string name;    // card display name; id fallback
    bool resolved = false; // explicit id resolved in the library
};

// 起始牌組預覽: an explicit form.deck lists its ids verbatim
// (unresolved entries flag — the gate would reject them); an
// empty deck previews the deckSeed roll so the player sees what
// the seed buys before committing.
std::vector<DeckPreviewRow> PreviewDeck(
    const CreationForm& form,
    const Gameplay::DoctrineLibrary& lib);

} // namespace Potato::Game
