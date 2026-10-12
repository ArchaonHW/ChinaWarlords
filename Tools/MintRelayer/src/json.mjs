// Lossless strict JSON — mirrors Gameplay/Json/JsonValue.cpp.
//
// The game's wire format bit-casts hash-family u64s into int64
// literals; ~half exceed 2^53, so JSON.parse would silently round
// them. This parser yields BigInt for every integer literal in
// int64 range and Number for reals / out-of-range integers —
// exactly the C++ Int-vs-Real split. Semantic parity with
// JsonValue::Parse:
//   - depth cap 64
//   - whitespace is space/tab/CR/LF only
//   - duplicate object keys: last wins
//   - string escapes " \ / b f n r t \uXXXX; lone surrogates
//     rejected; unescaped control chars <0x20 rejected
//   - numbers: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
//     int64-range integers → BigInt; anything else finite → Number
//   - trailing content rejected
//
// Deliberately stricter than C++ in one spot: the input must be
// valid UTF-8 (the caller decodes with fatal:true). The C++ parser
// passes bytes >=0x20 through verbatim, but every game producer
// emits valid UTF-8 — the relayer refuses bytes it cannot
// canonicalize.

export class ParseError extends Error {}

const MAX_DEPTH = 64;
const I64_MIN = -(1n << 63n);
const I64_MAX = (1n << 63n) - 1n;

export function parseJson(text) {
  let pos = 0;
  const fail = (why) => { throw new ParseError(`${why} at offset ${pos}`); };
  const peek = () => (pos < text.length ? text[pos] : "");
  const skipWs = () => {
    while (pos < text.length && " \t\n\r".includes(text[pos])) pos++;
  };
  const literal = (word) => {
    if (text.startsWith(word, pos)) { pos += word.length; return; }
    fail("invalid literal");
  };
  const hex4 = () => {
    if (pos + 4 > text.length) fail("truncated \\u escape");
    let cp = 0;
    for (let i = 0; i < 4; i++) {
      const c = text.charCodeAt(pos++);
      const d = c >= 48 && c <= 57 ? c - 48
              : c >= 97 && c <= 102 ? c - 87
              : c >= 65 && c <= 70 ? c - 55 : -1;
      if (d < 0) fail("bad hex digit in \\u escape");
      cp = (cp << 4) | d;
    }
    return cp;
  };
  const parseString = () => {
    pos++; // '"'
    let out = "";
    for (;;) {
      if (pos >= text.length) fail("unterminated string");
      const c = text[pos++];
      if (c === '"') return out;
      if (c.charCodeAt(0) < 0x20) {
        fail("unescaped control character in string");
      }
      if (c !== "\\") { out += c; continue; }
      if (pos >= text.length) fail("unterminated escape");
      const e = text[pos++];
      switch (e) {
        case '"': out += '"'; break;
        case "\\": out += "\\"; break;
        case "/": out += "/"; break;
        case "b": out += "\b"; break;
        case "f": out += "\f"; break;
        case "n": out += "\n"; break;
        case "r": out += "\r"; break;
        case "t": out += "\t"; break;
        case "u": {
          let cp = hex4();
          if (cp >= 0xd800 && cp <= 0xdbff) {
            if (pos + 2 > text.length || text[pos] !== "\\" ||
                text[pos + 1] !== "u") {
              fail("lone high surrogate in \\u escape");
            }
            pos += 2;
            const lo = hex4();
            if (lo < 0xdc00 || lo > 0xdfff) {
              fail("expected low surrogate in \\u escape");
            }
            cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
          } else if (cp >= 0xdc00 && cp <= 0xdfff) {
            fail("lone low surrogate in \\u escape");
          }
          out += String.fromCodePoint(cp);
          break;
        }
        default: fail("bad escape character");
      }
    }
  };
  const parseNumber = () => {
    const start = pos;
    if (peek() === "-") pos++;
    if (peek() === "0") {
      pos++;
    } else if (peek() >= "1" && peek() <= "9") {
      while (peek() >= "0" && peek() <= "9") pos++;
    } else {
      fail("invalid number");
    }
    let real = false;
    if (peek() === ".") {
      real = true;
      pos++;
      if (!(peek() >= "0" && peek() <= "9")) {
        fail("expected digit after '.'");
      }
      while (peek() >= "0" && peek() <= "9") pos++;
    }
    if (peek() === "e" || peek() === "E") {
      real = true;
      pos++;
      if (peek() === "+" || peek() === "-") pos++;
      if (!(peek() >= "0" && peek() <= "9")) {
        fail("expected digit in exponent");
      }
      while (peek() >= "0" && peek() <= "9") pos++;
    }
    const tok = text.slice(start, pos);
    if (!real) {
      const v = BigInt(tok);
      if (v >= I64_MIN && v <= I64_MAX) return v;
      // Integer literal outside int64 — C++ degrades to Real.
    }
    const v = Number(tok);
    if (!Number.isFinite(v)) fail("real number out of range");
    return v;
  };
  const parseValue = (depth) => {
    if (depth > MAX_DEPTH) fail("nesting depth exceeds 64");
    skipWs();
    const c = peek();
    if (c === "{") {
      pos++;
      const obj = Object.create(null); // no proto-key shenanigans
      skipWs();
      if (peek() === "}") { pos++; return obj; }
      for (;;) {
        skipWs();
        if (peek() !== '"') fail("expected object key");
        const key = parseString();
        skipWs();
        if (peek() !== ":") fail("expected ':'");
        pos++;
        obj[key] = parseValue(depth + 1); // duplicate keys: last wins
        skipWs();
        const d = peek();
        if (d === ",") { pos++; continue; }
        if (d === "}") { pos++; return obj; }
        fail("expected ',' or '}'");
      }
    }
    if (c === "[") {
      pos++;
      const arr = [];
      skipWs();
      if (peek() === "]") { pos++; return arr; }
      for (;;) {
        arr.push(parseValue(depth + 1));
        skipWs();
        const d = peek();
        if (d === ",") { pos++; continue; }
        if (d === "]") { pos++; return arr; }
        fail("expected ',' or ']'");
      }
    }
    if (c === '"') return parseString();
    if (c === "t") { literal("true"); return true; }
    if (c === "f") { literal("false"); return false; }
    if (c === "n") { literal("null"); return null; }
    return parseNumber();
  };

  const out = parseValue(1);
  skipWs();
  if (pos < text.length) fail("trailing characters after document");
  return out;
}

// ---------- canonical emit (byte-for-byte JsonValue::Emit) ----------

const utf8 = new TextEncoder();

// Object key order = unsigned byte order of the UTF-8 key bytes,
// matching C++ std::map<std::string> — not JS's UTF-16 ordering.
function keyBytesCmp(a, b) {
  const ba = utf8.encode(a), bb = utf8.encode(b);
  const n = Math.min(ba.length, bb.length);
  for (let i = 0; i < n; i++) if (ba[i] !== bb[i]) return ba[i] - bb[i];
  return ba.length - bb.length;
}

const HEX = "0123456789abcdef";
function emitString(s, out) {
  out += '"';
  for (const ch of s) { // iterate code points — mirrors byte-passthrough
    const cp = ch.codePointAt(0);
    if (ch === '"') out += '\\"';
    else if (ch === "\\") out += "\\\\";
    else if (ch === "\b") out += "\\b";
    else if (ch === "\f") out += "\\f";
    else if (ch === "\n") out += "\\n";
    else if (ch === "\r") out += "\\r";
    else if (ch === "\t") out += "\\t";
    else if (cp < 0x20) out += "\\u00" + HEX[(cp >> 4) & 0xf] + HEX[cp & 0xf];
    else out += ch; // UTF-8 passthrough on encode
  }
  return out + '"';
}

export function emitCanon(v) {
  if (v === null) return "null";
  if (typeof v === "boolean") return v ? "true" : "false";
  if (typeof v === "bigint") return v.toString(10);
  if (typeof v === "number") {
    if (!Number.isFinite(v)) return "null";
    // Reals never occur in potato.* schemas; this branch exists for
    // completeness. JS shortest-print ≈ std::to_chars shortest.
    let s = String(v);
    if (!/[.eE]/.test(s)) s += ".0"; // keep type stable on re-parse
    return s;
  }
  if (typeof v === "string") return emitString(v, "");
  if (Array.isArray(v)) {
    return "[" + v.map(emitCanon).join(",") + "]";
  }
  const keys = Object.keys(v).sort(keyBytesCmp);
  return "{" + keys
    .map((k) => emitString(k, "") + ":" + emitCanon(v[k]))
    .join(",") + "}";
}

export function emitBytes(v) {
  return utf8.encode(emitCanon(v));
}

// Read a wire file: UTF-8 (fatal) + BOM strip, mirroring Json::Load.
export function decodeWire(bytes) {
  const text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
  return text.startsWith("\uFEFF") ? text.slice(1) : text;
}
