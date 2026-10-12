#include "Game/Ui/CreationForm.h"

#include <cctype>
#include <cstdint>

namespace Potato::Game {

using Campaign::Character;
using Campaign::CharacterLibrary;
using Gameplay::Result;

std::vector<RosterRow> RosterList(const CharacterLibrary& lib) {
    std::vector<RosterRow> out;
    for (const Character& c : lib.Characters()) {
        if (c.created) continue; // roster = predefined only
        out.push_back(RosterRow{c.id, c.name, c.prior});
    }
    return out;
}

namespace {

// The form's own cheap pre-checks — everything substantive is
// re-verified by the wire gate below.
const char* Preflight(const CreationForm& f) {
    if (f.name.empty() || f.origin.empty()) {
        return "name and origin are required";
    }
    auto blank = [](const std::string& s) {
        for (const char c : s) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        return true;
    };
    if (blank(f.name) || blank(f.origin)) {
        return "name/origin must not be blank";
    }
    if (f.deck.size() > Character::MAX_DECK) {
        return "deck exceeds the card cap";
    }
    for (const std::string& card : f.deck) {
        if (card.empty() || card.size() > Character::MAX_CARD_ID_LEN) {
            return "deck carries an empty or oversized card id";
        }
    }
    return nullptr;
}

// A created character's id is derived, not typed: slug the
// name so the IntelLedger subject key stays stable ASCII
// ("統帥<name>" claims key on it). Names that slug to nothing
// (CJK-only input) fall back to a deterministic FNV-1a of
// name+seed — still stable, still ASCII.
std::string Slugify(const std::string& name, std::uint64_t seed) {
    static constexpr std::string_view PREFIX = "pc_";
    std::string id{PREFIX};
    for (const char c : name) {
        if (c >= 'a' && c <= 'z') id += c;
        else if (c >= 'A' && c <= 'Z') {
            id += static_cast<char>(c - 'A' + 'a');
        } else if (c >= '0' && c <= '9') {
            id += c;
        } else {
            id += '_';
        }
    }
    // Trim trailing separators; bound to the id cap.
    while (id.size() > PREFIX.size() && id.back() == '_') {
        id.pop_back();
    }
    if (id == PREFIX) {
        std::uint64_t h = 1469598103934665603ull;
        for (const unsigned char b : name) {
            h ^= b;
            h *= 1099511628211ull;
        }
        h ^= seed;
        h *= 1099511628211ull;
        static constexpr char HEX[] = "0123456789abcdef";
        for (int shift = 60; shift >= 0; shift -= 4) {
            id += HEX[(h >> shift) & 0xf];
        }
    }
    if (id.size() > Character::MAX_ID_LEN) {
        id.resize(Character::MAX_ID_LEN);
    }
    return id;
}

} // namespace

Result<Character> BuildCharacter(const CreationForm& form) {
    if (const char* why = Preflight(form)) {
        return Gameplay::Fail<Character>("character", why);
    }
    Character c;
    c.id = Slugify(form.name, form.deckSeed);
    if (c.id == "pc_") {
        return Gameplay::Fail<Character>(
            "character", "name yields no usable id");
    }
    c.name = form.name;
    c.origin = form.origin;
    c.prior = form.prior;
    c.deckSeed = form.deckSeed;
    c.deck = form.deck;
    c.created = true;

    // The wire gate is the validator: serialize then re-parse —
    // a doc that can't round-trip potato.character/1 is not a
    // character.
    const Gameplay::JsonValue wire = c.ToJson();
    return Character::FromJson(wire);
}

} // namespace Potato::Game
