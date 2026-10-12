// potato.ledger/3 verification — a port of Ledger::FromJson +
// Verify(). The ledger file is untrusted: load is rejected unless
// the schema holds, every entry invariant holds, the hash chain
// replays, and the stored seal matches a recompute.
//
// Wire shape (entry): {seq, credit:{account,amount}, debit:{...},
// memo, tags:[], provenance:"honest"|"forged", suspect:bool,
// prevHash:int64, hash:int64}. Hash-family values are u64 bit-cast
// into int64 — parseJson yields BigInt for them; compare via
// BigInt.asUintN(64).
import { emitBytes, parseJson, decodeWire } from "./json.mjs";
import { fnv1aBytes, fnv1aU64, FNV_OFFSET } from "./fnv.mjs";

export const LEDGER_SCHEMA = "potato.ledger/3";
export const GENESIS_HASH = FNV_OFFSET; // 14695981039346656037
export const MAX_ENTRIES = 1000000;
export const MAX_TAGS = 16;
export const MAX_TAG_LEN = 64;
export const MAX_MEMO_LEN = 256;
export const MAX_AMOUNT = 1000000000000n;

const ACCOUNTS = new Set([
  "martial_merit", "popular_support", "mandate",
  "army_prestige", "materiel",
]);

const M64 = (v) => BigInt.asUintN(64, v);
const isInt = (v) => typeof v === "bigint"; // reals degrade → Number
const isStr = (v) => typeof v === "string";
const isObj = (v) =>
  v !== null && typeof v === "object" && !Array.isArray(v);

function legFromJson(j) {
  if (!isObj(j) || !isInt(j.amount) || !isStr(j.account) ||
      !ACCOUNTS.has(j.account)) {
    return null;
  }
  return { account: j.account, amount: j.amount };
}

// Shared invariants — same rules the C++ write path enforces.
function validateLegs(credit, debit) {
  if (credit.amount <= 0n || debit.amount <= 0n) {
    return "credit/debit amount must be > 0";
  }
  if (credit.amount > MAX_AMOUNT || debit.amount > MAX_AMOUNT) {
    return "amount exceeds MAX_AMOUNT";
  }
  if (credit.account === debit.account) {
    return "credit and debit must be different accounts";
  }
  return null;
}

function validateMeta(memo, tags) {
  if (new TextEncoder().encode(memo).length > MAX_MEMO_LEN) {
    return "memo too long";
  }
  if (tags.length > MAX_TAGS) return "too many tags";
  for (const t of tags) {
    const len = new TextEncoder().encode(t).length;
    if (len === 0 || len > MAX_TAG_LEN) return "bad tag length";
  }
  const sorted = [...tags].sort();
  for (let i = 1; i < sorted.length; i++) {
    if (sorted[i] === sorted[i - 1]) return "duplicate tag";
  }
  return null;
}

// The content object the entry hash commits to — identical fields
// to the wire entry sans suspect/prevHash/hash. Keys re-sorted by
// emitBytes anyway (canonical emit = std::map byte order).
function entryContentEmit(e) {
  return emitBytes({
    seq: e.seq,
    credit: { account: e.credit.account, amount: e.credit.amount },
    debit: { account: e.debit.account, amount: e.debit.amount },
    memo: e.memo,
    provenance: e.provenance,
    tags: e.tags,
  });
}

// EntryHash: fnv1a over prevHash (LE bytes) then the canonical
// content emit — same convention as the battle record root.
export function entryHash(e) {
  const h = fnv1aU64(M64(e.prevHash));
  return fnv1aBytes(entryContentEmit(e), h);
}

export function sealHash(count, tip) {
  let h = fnv1aU64(M64(count));
  h = fnv1aU64(M64(tip), h);
  return h;
}

export function ledgerTip(entries) {
  return entries.length === 0
    ? GENESIS_HASH
    : M64(entries[entries.length - 1].hash);
}

// Throws Error — the ledger is the batch's root of trust; a bad
// ledger means NO claim can be verified, so this fails wholesale
// rather than per-file.
export function verifyLedger(doc) {
  if (!isObj(doc)) throw new Error("schema: ledger is not an object");
  if (doc.schema !== LEDGER_SCHEMA) {
    throw new Error("schema: expected potato.ledger/3");
  }
  if (!isInt(doc.seal)) {
    throw new Error("schema: missing chain seal");
  }
  if (!Array.isArray(doc.entries) ||
      doc.entries.length > MAX_ENTRIES) {
    throw new Error("schema: entries missing or oversized");
  }
  const entries = [];
  let expectSeq = 0n;
  for (const j of doc.entries) {
    if (!isObj(j) || !isInt(j.seq) || !isStr(j.memo) ||
        !Array.isArray(j.tags) || !isStr(j.provenance) ||
        typeof j.suspect !== "boolean" ||
        !isInt(j.prevHash) || !isInt(j.hash)) {
      throw new Error("schema: mistyped entry field");
    }
    if (j.seq !== expectSeq) {
      throw new Error("schema: entry seq not contiguous from 0");
    }
    const e = {
      seq: expectSeq++,
      credit: legFromJson(j.credit),
      debit: legFromJson(j.debit),
      memo: j.memo,
      tags: [],
      provenance: j.provenance,
      suspect: j.suspect,
      prevHash: j.prevHash,
      hash: j.hash,
    };
    if (e.credit === null || e.debit === null) {
      throw new Error("schema: bad entry leg");
    }
    const legWhy = validateLegs(e.credit, e.debit);
    if (legWhy) throw new Error(`unbalanced: ${legWhy}`);
    for (const t of j.tags) {
      if (!isStr(t)) throw new Error("schema: non-string tag");
      e.tags.push(t);
    }
    const metaWhy = validateMeta(e.memo, e.tags);
    if (metaWhy) throw new Error(`schema: ${metaWhy}`);
    if (e.provenance !== "honest" && e.provenance !== "forged") {
      throw new Error("schema: unknown provenance");
    }
    entries.push(e);
  }
  // Chain replay — Verify(): link, stored-hash recompute.
  for (let i = 0; i < entries.length; i++) {
    const e = entries[i];
    const expect = i === 0 ? GENESIS_HASH : M64(entries[i - 1].hash);
    if (M64(e.prevHash) !== expect) {
      throw new Error("chain: chain link broken");
    }
    if (M64(e.hash) !== entryHash(e)) {
      throw new Error("chain: entry hash mismatch");
    }
  }
  const tip = ledgerTip(entries);
  if (M64(doc.seal) !== sealHash(BigInt(entries.length), tip)) {
    throw new Error("chain: seal mismatch");
  }
  return { entries, tip, seal: M64(doc.seal) };
}

export function loadLedger(bytes) {
  let doc;
  try {
    doc = parseJson(decodeWire(bytes));
  } catch (err) {
    throw new Error(`parse: ${err.message}`);
  }
  return verifyLedger(doc);
}
