// FNV-1a 64 — same constants as the C++ chain (Ledger.cpp,
// BattleRecorder): offset basis 14695981039346656037, prime
// 1099511628211. All arithmetic in BigInt; every multiply is
// clamped to u64 so JS precision can't drift from C++.
export const FNV_OFFSET = 14695981039346656037n;
export const FNV_PRIME = 1099511628211n;

const M64 = (v) => BigInt.asUintN(64, v);

export function fnv1aBytes(bytes, h = FNV_OFFSET) {
  for (const b of bytes) {
    h ^= BigInt(b);
    h = M64(h * FNV_PRIME);
  }
  return h;
}

export function fnv1aString(text, h = FNV_OFFSET) {
  return fnv1aBytes(new TextEncoder().encode(text), h);
}

// Folds a u64 as 8 little-endian bytes — the C++ pattern
// `h ^= uint8(v >> (i*8)); h *= prime;`.
export function fnv1aU64(v, h = FNV_OFFSET) {
  const u = M64(v);
  for (let i = 0n; i < 8n; i++) {
    h ^= (u >> (i * 8n)) & 0xffn;
    h = M64(h * FNV_PRIME);
  }
  return h;
}
