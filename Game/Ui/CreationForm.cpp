#include "Game/Ui/CreationForm.h"

#include "Gameplay/Doctrine/Doctrine.h"
#include "Gameplay/Sim/Sim.h" // Prng

#include <cctype>
#include <cstdint>
#include <set>

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

// ---- Story 13.5 ----

std::vector<FieldError> ValidateForm(const CreationForm& f) {
    std::vector<FieldError> errs;
    auto blank = [](const std::string& s) {
        for (const char c : s) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        return true;
    };
    auto field = [&errs, &blank](CreationField cf,
                                 const std::string& v,
                                 std::size_t maxLen) {
        if (v.empty()) {
            errs.push_back({cf, "required"});
        } else if (blank(v)) {
            errs.push_back({cf, "must not be blank"});
        } else if (v.size() > maxLen) {
            errs.push_back({cf, "too long"});
        }
    };
    field(CreationField::Name, f.name, Character::MAX_NAME_LEN);
    field(CreationField::Origin, f.origin,
          Character::MAX_ORIGIN_LEN);
    if (f.deck.size() > Character::MAX_DECK) {
        errs.push_back({CreationField::Deck,
                        "deck exceeds the card cap"});
    }
    for (const std::string& card : f.deck) {
        if (card.empty() ||
            card.size() > Character::MAX_CARD_ID_LEN) {
            errs.push_back(
                {CreationField::Deck,
                 "deck carries an empty or oversized card id"});
            break;
        }
    }
    return errs;
}

CreationField FieldForReason(std::string_view reason) {
    // Field names appear quoted in gate reasons ("missing/empty/
    // out-of-range 'name'") and bare in preflight phrases;
    // substring order disambiguates ("deck_seed" hits "seed").
    if (reason.find("name") != std::string_view::npos) {
        return CreationField::Name;
    }
    if (reason.find("origin") != std::string_view::npos) {
        return CreationField::Origin;
    }
    if (reason.find("prior") != std::string_view::npos) {
        return CreationField::Prior;
    }
    if (reason.find("seed") != std::string_view::npos) {
        return CreationField::DeckSeed;
    }
    return CreationField::Deck;
}

std::vector<std::string> RollDeck(
    std::uint64_t seed, const Gameplay::DoctrineLibrary& lib) {
    std::vector<std::string> ids;
    if (lib.Count() == 0) return ids;
    Gameplay::Prng rng(seed);
    // 3..5 distinct cards — SquadSheet's slot bounds.
    std::size_t count =
        Gameplay::SquadSheet::MIN_SLOTS +
        static_cast<std::size_t>(rng.Next() % 3);
    if (count > lib.Count()) count = lib.Count();
    std::set<std::size_t> picked;
    while (picked.size() < count) {
        picked.insert(
            static_cast<std::size_t>(rng.Next() % lib.Count()));
    }
    ids.reserve(count);
    for (std::size_t i : picked) ids.push_back(lib.At(i).id);
    return ids;
}

std::vector<DeckPreviewRow> PreviewDeck(
    const CreationForm& form,
    const Gameplay::DoctrineLibrary& lib) {
    std::vector<DeckPreviewRow> rows;
    auto add = [&lib, &rows](const std::string& id) {
        DeckPreviewRow r;
        r.id = id;
        if (const Gameplay::DoctrineCard* c = lib.Find(id)) {
            r.name = c->name.empty() ? c->id : c->name;
            r.resolved = true;
        } else {
            r.name = id;
        }
        rows.push_back(std::move(r));
    };
    if (!form.deck.empty()) {
        for (const std::string& id : form.deck) add(id);
    } else {
        for (const std::string& id : RollDeck(form.deckSeed, lib)) {
            add(id);
        }
    }
    return rows;
}

} // namespace Potato::Game
