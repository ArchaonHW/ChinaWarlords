#pragma once

#include "Campaign/Rivals/RivalDeck.h" // RivalPrior vocabulary
#include "Gameplay/Json/JsonValue.h"
#include "Gameplay/Result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Campaign {

// Player commander identity (Story 12.4, FR24): potato.character/1.
// Predefined generals and player-created commanders share the wire
// doc — provenance is the `created` flag, not a type split. The
// character is a persisted identity, never a sim entity: priors bind
// into FogConfig/IntelLedger surfaces (CommanderBind) at campaign
// start; nothing in Gameplay/ reads it.
//
// Wire shape:
//   {"schema":"potato.character/1","id":"lv_bu","name":"呂布",
//    "origin":"九原","prior":"aggressive","deck_seed":42,
//    "deck":["press_the_advance"],"created":false}
struct Character {
    std::string id;      // hearsay subject key (IntelLedger subject)
    std::string name;    // display name, may carry UTF-8 CJK
    std::string origin;  // 出身 — bounded free string for now
    // Personality prior — same vocabulary rivals carry
    // (RivalDeck.h: "biases doctrine authorship and fog shape").
    RivalPrior prior = RivalPrior::Aggressive;
    std::uint64_t deckSeed = 0;       // starting deck roll seed
    std::vector<std::string> deck;    // optional explicit card ids
    bool created = false;             // player-authored vs roster

    static constexpr std::string_view SCHEMA = "potato.character/1";
    static constexpr std::size_t MAX_ID_LEN = 64;
    static constexpr std::size_t MAX_NAME_LEN = 64;
    static constexpr std::size_t MAX_ORIGIN_LEN = 64;
    static constexpr std::size_t MAX_DECK = 32;
    static constexpr std::size_t MAX_CARD_ID_LEN = 64;

    // potato.character/1 file → gated load → validation.
    static Gameplay::Result<Character> Load(std::string_view path);
    // Validate a DOM root (schema must already be gated for file
    // loads). Transactional: failure leaves no partial character.
    static Gameplay::Result<Character> FromJson(
        const Gameplay::JsonValue& root);
    // Canonical form: `deck` emits only when non-empty, `created`
    // only when true — absent == default, so round-trips are
    // byte-identical.
    Gameplay::JsonValue ToJson() const;
};

struct RejectedCharacter {
    std::filesystem::path path;
    std::string error;
    std::string reason;
};

struct CharacterLoadResult {
    bool ok = false;
    std::string error;
    std::string reason;
    std::vector<RejectedCharacter> rejected; // per-file failures
};

// Boot-time registry (WorldLibrary precedent): the predefined
// general roster plus any player-authored docs live in one content
// dir; built once by Load(), immutable thereafter (NFR9). Per-file
// isolation: a bad character file is rejected and logged, never
// fails the library; Load fails wholesale only if the directory
// itself is unreadable or the file count overflows.
class CharacterLibrary {
public:
    static constexpr std::string_view SCHEMA = "potato.character/1";
    static constexpr std::size_t MAX_CHARACTERS = 256;

    // Sorted-filename iteration, per-file rejection into
    // result.rejected. Directory unreadable -> !ok.
    static CharacterLoadResult Load(const std::filesystem::path& dir,
                                    CharacterLibrary& out);

    const Character* Find(std::string_view id) const;
    std::size_t Size() const { return chars_.size(); }
    // Canonical order: character id.
    const std::vector<Character>& Characters() const {
        return chars_;
    }

private:
    std::vector<Character> chars_;
};

} // namespace Potato::Campaign
