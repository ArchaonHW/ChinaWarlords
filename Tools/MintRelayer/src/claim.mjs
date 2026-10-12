// potato.mintclaim/1 validation — a port of MintClaim::FromJson +
// ClaimId, plus the relayer-side checks that bind the claim to the
// ledger: seal consistency and (for settlements) the record_root:
// anchor. The claim file is untrusted; every field is re-derived.
import { emitBytes, emitCanon, parseJson } from "./json.mjs";

export const CLAIM_SCHEMA = "potato.mintclaim/1";
export const MAX_CHAPTERS = 64;
export const MAX_KEY_LEN = 64;
export const MAX_ID_LEN = 128;

const RESOLUTIONS = new Set([
  "battle_victory", "governance_victory", "subversion", "defeat",
]);

const isInt = (v) => typeof v === "bigint";
const isStr = (v) => typeof v === "string";
const isObj = (v) =>
  v !== null && typeof v === "object" && !Array.isArray(v);
const M64 = (v) => BigInt.asUintN(64, v);

export function rootHex(root) {
  return M64(root).toString(16).padStart(16, "0");
}

function parseRootHex(s) {
  if (!isStr(s) || s.length !== 16 || !/^[0-9a-f]{16}$/.test(s)) {
    return null;
  }
  return BigInt("0x" + s);
}

// Achievement key alphabet: [a-z0-9_] — ':' is an illegal filename
// char on Windows and '-' is the id separator.
const keyCharsOk = (k) => /^[a-z0-9_]+$/.test(k);

export function claimId(c) {
  // Both settlement flavors bind the battle record's root — a
  // chapter verdict and an encounter verdict can't share a record.
  if (c.kind === "chapter_settlement" || c.kind === "settlement") {
    return "settlement-" + rootHex(c.recordRoot);
  }
  return "achievement-" + c.achievement + "-" +
    String(c.chapter).padStart(4, "0");
}

// Returns {ok:true, claim:{kind,chapter,ledgerCount,ledgerTip,
// recordRoot|achievement, id}} or {ok:false, error, reason} —
// mirrors MintClaim::FromJson's fail classes.
export function validateClaim(doc) {
  const fail = (error, reason) => ({ ok: false, error, reason });
  if (!isObj(doc)) return fail("field", "claim is not an object");
  if (doc.schema !== CLAIM_SCHEMA) {
    return fail("schema", "schema mismatch");
  }
  const kind = doc.kind;
  if (kind !== "chapter_settlement" && kind !== "achievement" &&
      kind !== "settlement") {
    return fail("kind", "unknown claim kind");
  }
  const c = {
    kind,
    ledgerCount: undefined,
    ledgerTip: undefined,
  };
  if (kind === "settlement") {
    // Encounters aren't chapter-keyed — the field is forbidden on
    // "settlement" claims, not merely absent.
    if ("chapter" in doc) {
      return fail("field", "settlement claim carries chapter");
    }
  } else {
    const ch = doc.chapter;
    if (!isInt(ch) || ch < 0n || ch > BigInt(MAX_CHAPTERS)) {
      return fail("field", "chapter out of range");
    }
    c.chapter = ch;
  }
  const ledger = doc.ledger;
  if (!isObj(ledger) || !isInt(ledger.count) || ledger.count < 0n ||
      !isInt(ledger.tip)) {
    return fail("field", "ledger seal out of range");
  }
  c.ledgerCount = ledger.count;
  c.ledgerTip = ledger.tip;
  if (kind === "chapter_settlement" || kind === "settlement") {
    const root = parseRootHex(doc.record_root);
    if (root === null) {
      return fail("field", "record_root missing/not 16-hex");
    }
    c.recordRoot = root;
    if (!isStr(doc.resolution) || doc.resolution.length === 0 ||
        new TextEncoder().encode(doc.resolution).length > MAX_KEY_LEN ||
        !RESOLUTIONS.has(doc.resolution)) {
      return fail("field", "resolution missing/unknown");
    }
    c.resolution = doc.resolution;
    if ("achievement" in doc) {
      return fail("field", "settlement claim carries achievement");
    }
    if (kind === "settlement") {
      for (const f of ["encounter", "node"]) {
        const v = doc[f];
        if (!isStr(v) || v.length === 0 ||
            new TextEncoder().encode(v).length > MAX_KEY_LEN) {
          return fail("field", "settlement missing encounter/node");
        }
        c[f] = v;
      }
    } else if ("encounter" in doc || "node" in doc) {
      return fail("field", "chapter claim carries encounter fields");
    }
  } else {
    const a = doc.achievement;
    if (!isStr(a) || a.length === 0 ||
        new TextEncoder().encode(a).length > MAX_KEY_LEN ||
        !keyCharsOk(a)) {
      return fail("field", "achievement missing/bad key");
    }
    c.achievement = a;
    if ("record_root" in doc || "resolution" in doc ||
        "encounter" in doc || "node" in doc) {
      return fail("field", "achievement claim carries settlement");
    }
  }
  // The id IS the tamper check: stored must equal recomputed.
  const id = doc.id;
  if (!isStr(id) || id.length > MAX_ID_LEN || id !== claimId(c)) {
    return fail("id", "claim id does not match content");
  }
  c.id = id;
  return { ok: true, claim: c };
}

// Relayer-side binding — ran after validateClaim passes:
//  - seal consistency: the claim's ledger block must equal the
//    file's head (count + tip the game stamped at emission).
//  - settlement anchor: some ledger entry must carry
//    `record_root:<16hex>` — byte-exact fold-tag discipline.
export function checkSeal(claim, ledgerState) {
  if (claim.ledgerCount !== BigInt(ledgerState.entries.length)) {
    return "seal: ledger.count != entries length";
  }
  if (M64(claim.ledgerTip) !== ledgerState.tip) {
    return "seal: ledger.tip != chain tip";
  }
  return null;
}

export function checkAnchor(claim, ledgerState) {
  if (claim.kind !== "chapter_settlement" &&
      claim.kind !== "settlement") return null;
  const want = "record_root:" + rootHex(claim.recordRoot);
  for (const e of ledgerState.entries) {
    if (e.tags.includes(want)) return null;
  }
  return "anchor: no record_root:" + rootHex(claim.recordRoot) +
    " tag in ledger chain";
}

// Full per-file pipeline: parse → validate → canonical round-trip
// (emit→parse→validate→emit must be byte-stable) → seal → anchor.
// Returns {ok, claim?, doc?, error?, reason?} — never throws on
// content. `doc` is the parsed claim for the merkle fold's
// canonical emit.
export function processClaimBytes(bytes, ledgerState) {
  let doc;
  try {
    const text = new TextDecoder("utf-8", { fatal: true })
      .decode(bytes);
    doc = parseJson(text.startsWith("﻿") ? text.slice(1) : text);
  } catch (err) {
    return { ok: false, error: "parse", reason: err.message };
  }
  const v = validateClaim(doc);
  if (!v.ok) return v;
  // Round-trip: the accepted claim must re-emit to the same
  // canonical bytes a second parse would — guards against fields
  // our validator normalizes but the file expressed differently.
  const canon = emitBytes(doc);
  const reparsed = parseJson(new TextDecoder().decode(canon));
  const v2 = validateClaim(reparsed);
  if (!v2.ok || emitBytes(reparsed).length !== canon.length ||
      emitCanon(reparsed) !== emitCanon(doc)) {
    return { ok: false, error: "roundtrip",
             reason: "canonical emit not stable" };
  }
  const sealWhy = checkSeal(v.claim, ledgerState);
  if (sealWhy) return { ok: false, error: "seal", reason: sealWhy };
  const anchorWhy = checkAnchor(v.claim, ledgerState);
  if (anchorWhy) {
    return { ok: false, error: "anchor", reason: anchorWhy };
  }
  return { ok: true, claim: v.claim, doc };
}
