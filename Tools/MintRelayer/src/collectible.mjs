// potato.collectible/1 — the keepsake descriptor + the gate that
// binds it: a strike may only seal an artifact the player's own
// campaign record actually produced.
//
// Gate truth is relayer-side (a contract can't read a campaign
// save): the player presents evidence docs, we re-validate the
// ledger (full replay — the seal stamped into the descriptor is
// the one the chain really carries) and check artifact presence.
import { emitCanon } from "./json.mjs";

export const COLLECTIBLE_SCHEMA = "potato.collectible/1";
export const COLLECTIBLE_KINDS = new Set(
  ["bencao", "frontispiece", "dossier"]);
export const DEFAULT_PRICE = 50n;

const isObj = (v) =>
  v !== null && typeof v === "object" && !Array.isArray(v);
const isStr = (v) => typeof v === "string";

const M64 = (v) => BigInt.asUintN(64, v);
const i64 = (u) => BigInt.asIntN(64, M64(u));

// Evidence surface per kind — the artifact must be present in the
// player's own record.
//   bencao      → id in a presented potato.bencao_state/1
//                 `unlocked` list (real check — Epic 10 file)
//   frontispiece/dossier → placeholder gate while Epics 8/6 are
//                 unlanded: the source id appears verbatim in some
//                 presented evidence doc's canonical emit.
function gateOk(kind, sourceId, evidenceDocs) {
  if (kind === "bencao") {
    for (const doc of evidenceDocs) {
      if (isObj(doc) && doc.schema === "potato.bencao_state/1" &&
          Array.isArray(doc.unlocked) &&
          doc.unlocked.includes(sourceId)) {
        return true;
      }
    }
    return false;
  }
  for (const doc of evidenceDocs) {
    // Verbatim substring inside the canonical emit — a placeholder,
    // not a claim of field-level presence (8/6 pending).
    if (emitCanon(doc).includes('"' + sourceId + '"')) {
      return true;
    }
  }
  return false;
}

// kind/sourceId shape — the id alphabet is the claims' machine-id
// set; kinds reject anything outside the closed set.
export function buildDescriptor(kind, sourceId, ledgerState,
                                evidenceDocs) {
  if (!COLLECTIBLE_KINDS.has(kind)) {
    return { ok: false, reason: `unknown kind '${kind}'` };
  }
  if (!isStr(sourceId) || sourceId.length === 0 ||
      new TextEncoder().encode(sourceId).length > 128) {
    return { ok: false, reason: "source_id out of range" };
  }
  if (!gateOk(kind, sourceId, evidenceDocs)) {
    return { ok: false,
             reason: `gate: '${sourceId}' not in presented record` };
  }
  const descriptor = {
    schema: COLLECTIBLE_SCHEMA,
    kind,
    source_id: sourceId,
    campaign: {
      count: i64(BigInt(ledgerState.entries.length)),
      tip: i64(ledgerState.tip),
      seal: i64(ledgerState.seal),
    },
  };
  return { ok: true, descriptor };
}
