#include "Campaign/Narrative/BencaoCodex.h"

#include "Campaign/Governance/Accumulators.h"
#include "Campaign/Ledger/Ledger.h"
#include "Campaign/Myth/MythLog.h"
#include "Campaign/Myth/MythState.h"
#include "Campaign/Roster/Roster.h"

#include <algorithm>
#include <set>
#include <utility>

namespace Potato::Campaign {
namespace {

using Gameplay::JsonValue;
using Gameplay::Result;

// Precomputed signal views — one ledger scan + one governance fold
// per ResolveBencaoUnlocks call, not per entry. At ledger scale
// (MAX_ENTRIES entries × library size) the naive shape is needlessly
// quadratic.
struct EvalCtx {
    explicit EvalCtx(const CodexSignals& s) : sig(s) {}
    const CodexSignals& sig;
    std::set<std::string> ledgerTags;   // every tag ever posted
    std::set<std::string> mythActions;  // every action ever logged
    std::int64_t corruption = 0;
    bool infiltrated = false;
};

EvalCtx Prepare(const CodexSignals& sig) {
    EvalCtx ctx(sig);
    if (sig.ledger) {
        for (const LedgerEntry& e : sig.ledger->Entries()) {
            for (const std::string& t : e.tags) {
                ctx.ledgerTags.insert(t);
            }
        }
        ctx.corruption = FoldGovernance(*sig.ledger).corruption;
    }
    if (sig.mythLog) {
        for (const MythLogEntry& e : sig.mythLog->Entries()) {
            ctx.mythActions.insert(e.action);
        }
    }
    ctx.infiltrated = sig.myth && !sig.chapterId.empty() &&
                      sig.myth->HasChapter(sig.chapterId);
    return ctx;
}

// Trigger test + provenance: when the trigger fires, `detail`
// carries WHAT matched (the tag posted, the action logged, the
// terrain flag, the corruption level, the closing chapter) — the
// queue keeps the moment, not just the verdict.
bool Triggered(const BencaoEntry& e, const EvalCtx& ctx,
               std::string& detail) {
    switch (e.unlockKind) {
    case UnlockKind::Terrain:
        if (std::find(ctx.sig.terrains.begin(), ctx.sig.terrains.end(),
                      e.unlockParam) != ctx.sig.terrains.end()) {
            detail = e.unlockParam;
            return true;
        }
        return false;
    case UnlockKind::LedgerTag:
        if (ctx.ledgerTags.count(e.unlockParam)) {
            detail = e.unlockParam;
            return true;
        }
        return false;
    case UnlockKind::Governance: {
        // Resolution seals book `resolution:<kind>`; governance-
        // flavored deed tags may also match the event id bare.
        const std::string seal =
            std::string(Ledger::TAG_RESOLUTION) + e.unlockParam;
        if (ctx.ledgerTags.count(seal)) {
            detail = seal;
            return true;
        }
        if (ctx.ledgerTags.count(e.unlockParam)) {
            detail = e.unlockParam;
            return true;
        }
        return false;
    }
    case UnlockKind::MythState:
        if (e.unlockParam == "infiltrated") {
            if (ctx.infiltrated) {
                detail = std::string(ctx.sig.chapterId);
                return true;
            }
            return false;
        }
        if (ctx.mythActions.count(e.unlockParam)) {
            detail = e.unlockParam;
            return true;
        }
        return false;
    case UnlockKind::Corruption:
        if (ctx.corruption >= e.unlockInt) {
            detail = std::to_string(ctx.corruption);
            return true;
        }
        return false;
    case UnlockKind::ChapterClose:
        if (e.unlockInt < 0 || e.unlockInt == ctx.sig.chapterIndex) {
            detail = std::to_string(ctx.sig.chapterIndex);
            return true;
        }
        return false;
    }
    return false;
}

// Id bound: non-empty, within the library's own wire bound — a
// persisted id that can't name an entry is junk occupying capacity.
const char* CheckId(const std::string& id) {
    if (id.empty() || id.size() > BencaoLibrary::MAX_ID_LEN) {
        return "bad id";
    }
    return nullptr;
}

} // namespace

bool BencaoCodex::IsUnlocked(std::string_view id) const {
    return std::find(unlocked_.begin(), unlocked_.end(), id) !=
           unlocked_.end();
}

std::vector<PendingPage> BencaoCodex::TakePending(std::size_t max) {
    const std::size_t n = std::min(max, pending_.size());
    std::vector<PendingPage> out(pending_.begin(), pending_.begin() + n);
    pending_.erase(pending_.begin(), pending_.begin() + n);
    // Delivered = transcribed into the book: the drained pages move
    // from the queue to the unlocked set.
    for (const PendingPage& p : out) {
        unlocked_.push_back(p.id);
    }
    return out;
}

Result<JsonValue> BencaoCodex::ToJson() const {
    JsonValue::Array unlocked;
    unlocked.reserve(unlocked_.size());
    for (const std::string& id : unlocked_) {
        unlocked.push_back(JsonValue::String(id));
    }
    JsonValue::Array pending;
    pending.reserve(pending_.size());
    for (const PendingPage& p : pending_) {
        JsonValue::Object o;
        o["id"] = JsonValue::String(p.id);
        o["kind"] = JsonValue::String(p.kind);
        o["detail"] = JsonValue::String(p.detail);
        pending.push_back(JsonValue::MakeObject(std::move(o)));
    }
    JsonValue::Object root;
    root["schema"] = JsonValue::String(std::string(SCHEMA));
    root["unlocked"] = JsonValue::MakeArray(std::move(unlocked));
    root["pending"] = JsonValue::MakeArray(std::move(pending));
    return Gameplay::Ok(JsonValue::MakeObject(std::move(root)));
}

Result<BencaoCodex> BencaoCodex::FromJson(const JsonValue& doc) {
    if (!doc.IsObject()) {
        return Gameplay::Fail<BencaoCodex>("schema",
                                           "root is not an object");
    }
    const std::string* schema = doc.FindString("schema");
    if (!schema || *schema != SCHEMA) {
        return Gameplay::Fail<BencaoCodex>(
            "schema", "expected potato.bencao_state/1");
    }
    if (!doc.Has("unlocked") || !doc["unlocked"].IsArray() ||
        !doc.Has("pending") || !doc["pending"].IsArray()) {
        return Gameplay::Fail<BencaoCodex>("schema",
                                           "unlocked/pending: not arrays");
    }
    BencaoCodex codex;
    // Union bound: a codex can never hold more pages than a library
    // can host — the two lists share one budget.
    const std::size_t total =
        doc["unlocked"].Size() + doc["pending"].Size();
    if (total > MAX_ENTRIES) {
        return Gameplay::Fail<BencaoCodex>("overflow",
                                           "codex exceeds MAX_ENTRIES");
    }
    for (const JsonValue& v : doc["unlocked"].Items()) {
        if (!v.IsString()) {
            return Gameplay::Fail<BencaoCodex>("bencao_state",
                                               "non-string id");
        }
        if (const char* err = CheckId(v.AsString())) {
            return Gameplay::Fail<BencaoCodex>("bencao_state", err);
        }
        const std::string& id = v.AsString();
        if (std::find(codex.unlocked_.begin(), codex.unlocked_.end(),
                      id) == codex.unlocked_.end()) {
            codex.unlocked_.push_back(id);
        }
    }
    for (const JsonValue& v : doc["pending"].Items()) {
        if (!v.IsObject()) {
            return Gameplay::Fail<BencaoCodex>("bencao_state",
                                               "pending not an object");
        }
        PendingPage p;
        const std::string* id = v.FindString("id");
        const std::string* kind = v.FindString("kind");
        const std::string* detail = v.FindString("detail");
        UnlockKind parsedKind;
        if (!id || !kind || !detail ||
            detail->size() > MAX_DETAIL_LEN ||
            !UnlockKindFromName(*kind, parsedKind)) {
            return Gameplay::Fail<BencaoCodex>(
                "bencao_state", "pending id/kind/detail bad");
        }
        if (const char* err = CheckId(*id)) {
            return Gameplay::Fail<BencaoCodex>("bencao_state", err);
        }
        p.id = *id;
        p.kind = *kind;
        p.detail = *detail;
        // Pending ids already written into the book are stale —
        // drop them; the canonical form keeps the lists disjoint.
        if (codex.IsUnlocked(p.id)) {
            continue;
        }
        const auto dup = std::find_if(
            codex.pending_.begin(), codex.pending_.end(),
            [&](const PendingPage& q) { return q.id == p.id; });
        if (dup == codex.pending_.end()) {
            codex.pending_.push_back(std::move(p));
        }
    }
    return Gameplay::Ok(std::move(codex));
}

Result<std::vector<std::string>>
ResolveBencaoUnlocks(const BencaoLibrary& lib, const CodexSignals& signals,
                     BencaoCodex& codex) {
    const EvalCtx ctx = Prepare(signals);
    std::vector<std::string> fresh;
    for (const BencaoEntry& e : lib.Entries()) {
        if (codex.IsUnlocked(e.id)) {
            continue;
        }
        // A pending page is already claimed by the queue — don't
        // double-enqueue it.
        const auto queued = std::find_if(
            codex.pending_.begin(), codex.pending_.end(),
            [&](const PendingPage& p) { return p.id == e.id; });
        if (queued != codex.pending_.end()) {
            continue;
        }
        std::string detail;
        if (!Triggered(e, ctx, detail)) {
            continue;
        }
        codex.pending_.push_back(
            {e.id, UnlockKindName(e.unlockKind), std::move(detail)});
        fresh.push_back(e.id);
    }
    return Gameplay::Ok(std::move(fresh));
}

std::vector<std::string>
TerrainFlagsOf(const Gameplay::BattleMap& map) {
    std::uint32_t seen = 0;
    for (std::size_t i = 0; i < map.RegionCount(); ++i) {
        seen |= map.RegionAt(i).terrain;
    }
    std::vector<std::string> out;
    // Fixed flag order — output is deterministic no matter which
    // regions carried which flags.
    static const std::pair<Gameplay::Terrain, std::string_view>
        kNames[] = {
            {Gameplay::TERRAIN_WATER, "water"},
            {Gameplay::TERRAIN_RIVER, "river"},
            {Gameplay::TERRAIN_ROAD, "road"},
            {Gameplay::TERRAIN_FOREST, "forest"},
            {Gameplay::TERRAIN_HIGHLAND, "highland"},
            {Gameplay::TERRAIN_CHOKEPOINT, "chokepoint"},
            {Gameplay::TERRAIN_OPEN, "open"},
        };
    for (const auto& [flag, name] : kNames) {
        if (seen & flag) {
            out.emplace_back(name);
        }
    }
    return out;
}

} // namespace Potato::Campaign
