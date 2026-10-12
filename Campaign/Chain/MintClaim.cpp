#include "Campaign/Chain/MintClaim.h"

#include "Campaign/State/CampaignState.h" // MAX_CHAPTERS bound

namespace Potato::Campaign {

using Gameplay::JsonValue;
using Gameplay::Result;

namespace {

int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Required string field: present, non-empty, within bound.
bool ReadField(const JsonValue& doc, std::string_view key,
               std::size_t maxLen, std::string& out) {
    const std::string* s = doc.FindString(key);
    if (!s || s->empty() || s->size() > maxLen) return false;
    out = *s;
    return true;
}

// Achievement keys are machine ids: ASCII lowercase/digits plus
// '_' — byte-exact like ledger fold tags. ':'/'-' are excluded on
// purpose: ':' is an illegal filename character on Windows and '-'
// is the id separator — either would corrupt `<id>.json`.
bool KeyCharsOk(std::string_view key) {
    for (char c : key) {
        const bool ok = (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    return true;
}

} // namespace

const char* ClaimKindName(ClaimKind k) {
    switch (k) {
    case ClaimKind::ChapterSettlement:
        return "chapter_settlement";
    case ClaimKind::Achievement:
        return "achievement";
    case ClaimKind::Settlement:
        return "settlement";
    }
    return "unknown";
}

bool ClaimKindFromName(std::string_view name, ClaimKind& out) {
    if (name == "chapter_settlement") {
        out = ClaimKind::ChapterSettlement;
        return true;
    }
    if (name == "achievement") {
        out = ClaimKind::Achievement;
        return true;
    }
    if (name == "settlement") {
        out = ClaimKind::Settlement;
        return true;
    }
    return false;
}

bool IsResolutionName(std::string_view name) {
    return name == "battle_victory" || name == "governance_victory" ||
           name == "subversion" || name == "defeat";
}

std::string RootHex(std::uint64_t root) {
    std::string s;
    s.reserve(16);
    for (int i = 15; i >= 0; --i) {
        s += "0123456789abcdef"[(root >> (i * 4)) & 0xf];
    }
    return s;
}

bool ParseRootHex(std::string_view s, std::uint64_t& out) {
    if (s.size() != 16) return false;
    std::uint64_t v = 0;
    for (char c : s) {
        const int d = HexVal(c);
        if (d < 0) return false;
        v = (v << 4) | static_cast<std::uint64_t>(d);
    }
    out = v;
    return true;
}

std::string ClaimId(const MintClaim& c) {
    // Both settlement flavors bind the battle record's root —
    // a chapter verdict and an encounter verdict can't share a
    // record, so one id shape serves both.
    if (c.kind == ClaimKind::ChapterSettlement ||
        c.kind == ClaimKind::Settlement) {
        return "settlement-" + RootHex(c.recordRoot);
    }
    std::string id = "achievement-" + c.achievement + "-";
    // Zero-padded 4-digit chapter — ids sort canonically.
    const std::string ch = std::to_string(c.chapter);
    id.append(ch.size() < 4 ? 4 - ch.size() : 0, '0');
    id += ch;
    return id;
}

Result<JsonValue> MintClaim::ToJson() const {
    const std::string id = ClaimId(*this);
    if (id.empty() || id.size() > MAX_ID_LEN) {
        return Gameplay::Fail<JsonValue>("field", "claim id out of range");
    }
    JsonValue::Object ledger;
    ledger["count"] =
        JsonValue::Int(static_cast<std::int64_t>(ledgerCount));
    ledger["tip"] =
        JsonValue::Int(static_cast<std::int64_t>(ledgerTip));

    JsonValue::Object o;
    o["schema"] = JsonValue::String(std::string(SCHEMA));
    o["id"] = JsonValue::String(id);
    o["kind"] = JsonValue::String(ClaimKindName(kind));
    if (kind != ClaimKind::Settlement) {
        // Encounters aren't chapter-keyed — the field is
        // forbidden on "settlement" claims, not merely absent.
        o["chapter"] = JsonValue::Int(chapter);
    }
    if (kind == ClaimKind::ChapterSettlement ||
        kind == ClaimKind::Settlement) {
        o["record_root"] = JsonValue::String(RootHex(recordRoot));
        o["resolution"] = JsonValue::String(resolution);
        if (kind == ClaimKind::Settlement) {
            o["encounter"] = JsonValue::String(encounter);
            o["node"] = JsonValue::String(node);
        }
    } else {
        o["achievement"] = JsonValue::String(achievement);
    }
    o["ledger"] = JsonValue::MakeObject(std::move(ledger));
    return Gameplay::Ok(JsonValue::MakeObject(std::move(o)));
}

Result<MintClaim> MintClaim::FromJson(const JsonValue& doc) {
    if (!doc.IsObject()) {
        return Gameplay::Fail<MintClaim>("field",
                                         "claim is not an object");
    }
    const std::string* schema = doc.FindString("schema");
    if (!schema || *schema != SCHEMA) {
        return Gameplay::Fail<MintClaim>("schema",
                                         "schema mismatch");
    }

    MintClaim c;
    const std::string* kind = doc.FindString("kind");
    if (!kind || !ClaimKindFromName(*kind, c.kind)) {
        return Gameplay::Fail<MintClaim>("kind", "unknown claim kind");
    }
    if (c.kind == ClaimKind::Settlement) {
        if (doc.Has("chapter")) {
            return Gameplay::Fail<MintClaim>(
                "field", "settlement claim carries chapter");
        }
    } else {
        const JsonValue& ch = doc["chapter"];
        if (!ch.IsInt() || ch.AsInt() < 0 ||
            ch.AsInt() > static_cast<std::int64_t>(
                             CampaignState::MAX_CHAPTERS)) {
            return Gameplay::Fail<MintClaim>("field",
                                             "chapter out of range");
        }
        c.chapter = ch.AsInt();
    }

    const JsonValue& ledger = doc["ledger"];
    if (!ledger.IsObject() || !ledger["count"].IsInt() ||
        ledger["count"].AsInt() < 0 || !ledger["tip"].IsInt()) {
        return Gameplay::Fail<MintClaim>("field",
                                         "ledger seal out of range");
    }
    c.ledgerCount = static_cast<std::uint64_t>(
        ledger["count"].AsInt());
    c.ledgerTip = static_cast<std::uint64_t>(ledger["tip"].AsInt());

    if (c.kind == ClaimKind::ChapterSettlement ||
        c.kind == ClaimKind::Settlement) {
        const std::string* root = doc.FindString("record_root");
        if (!root || !ParseRootHex(*root, c.recordRoot)) {
            return Gameplay::Fail<MintClaim>(
                "field", "record_root missing/not 16-hex");
        }
        if (!ReadField(doc, "resolution", MAX_KEY_LEN,
                       c.resolution) ||
            !IsResolutionName(c.resolution)) {
            return Gameplay::Fail<MintClaim>(
                "field", "resolution missing/unknown");
        }
        if (doc.Has("achievement")) {
            return Gameplay::Fail<MintClaim>(
                "field", "settlement claim carries achievement");
        }
        if (c.kind == ClaimKind::Settlement) {
            if (!ReadField(doc, "encounter", MAX_KEY_LEN,
                           c.encounter) ||
                !ReadField(doc, "node", MAX_KEY_LEN, c.node)) {
                return Gameplay::Fail<MintClaim>(
                    "field", "settlement missing encounter/node");
            }
        } else if (doc.Has("encounter") || doc.Has("node")) {
            return Gameplay::Fail<MintClaim>(
                "field", "chapter claim carries encounter fields");
        }
    } else {
        if (!ReadField(doc, "achievement", MAX_KEY_LEN,
                       c.achievement) ||
            !KeyCharsOk(c.achievement)) {
            return Gameplay::Fail<MintClaim>(
                "field", "achievement missing/bad key");
        }
        if (doc.Has("record_root") || doc.Has("resolution") ||
            doc.Has("encounter") || doc.Has("node")) {
            return Gameplay::Fail<MintClaim>(
                "field", "achievement claim carries settlement");
        }
    }

    // The id IS the tamper check: stored must equal recomputed.
    const std::string* id = doc.FindString("id");
    if (!id || id->size() > MAX_ID_LEN || *id != ClaimId(c)) {
        return Gameplay::Fail<MintClaim>(
            "id", "claim id does not match content");
    }
    return Gameplay::Ok(c);
}

} // namespace Potato::Campaign
