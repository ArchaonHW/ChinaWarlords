#include "Gameplay/Json/JsonValue.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <system_error>

namespace Potato::Gameplay {

namespace {

constexpr int MAX_DEPTH = 64;

// RFC 8259 recursive-descent parser. All failures return a reason
// string; callers wrap it into Result<JsonValue>.
struct Parser {
    std::string_view text;
    std::size_t pos = 0;
    std::string err;

    bool Fail(std::string reason) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%s at offset %zu", reason.c_str(), pos);
        err = buf;
        return false;
    }

    char Peek() const { return pos < text.size() ? text[pos] : '\0'; }
    bool Eof() const { return pos >= text.size(); }

    void SkipWs() {
        while (pos < text.size()) {
            char c = text[pos];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            ++pos;
        }
    }

    bool Literal(std::string_view word) {
        if (text.substr(pos, word.size()) == word) { pos += word.size(); return true; }
        return Fail("invalid literal");
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (depth > MAX_DEPTH) return Fail("nesting depth exceeds 64");
        SkipWs();
        switch (Peek()) {
            case '{': return ParseObject(out, depth);
            case '[': return ParseArray(out, depth);
            case '"': {
                std::string s;
                if (!ParseString(s)) return false;
                out = JsonValue::String(std::move(s));
                return true;
            }
            case 't': { if (!Literal("true")) return false; out = JsonValue::Bool(true); return true; }
            case 'f': { if (!Literal("false")) return false; out = JsonValue::Bool(false); return true; }
            case 'n': { if (!Literal("null")) return false; out = JsonValue(); return true; }
            default: return ParseNumber(out);
        }
    }

    bool ParseObject(JsonValue& out, int depth) {
        ++pos; // '{'
        JsonValue::Object members;
        SkipWs();
        if (Peek() == '}') { ++pos; out = JsonValue::MakeObject(std::move(members)); return true; }
        while (true) {
            SkipWs();
            if (Peek() != '"') return Fail("expected object key");
            std::string key;
            if (!ParseString(key)) return false;
            SkipWs();
            if (Peek() != ':') return Fail("expected ':'");
            ++pos;
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            members[std::move(key)] = std::move(value); // duplicate keys: last wins
            SkipWs();
            char c = Peek();
            if (c == ',') { ++pos; continue; }
            if (c == '}') { ++pos; out = JsonValue::MakeObject(std::move(members)); return true; }
            return Fail("expected ',' or '}'");
        }
    }

    bool ParseArray(JsonValue& out, int depth) {
        ++pos; // '['
        JsonValue::Array items;
        SkipWs();
        if (Peek() == ']') { ++pos; out = JsonValue::MakeArray(std::move(items)); return true; }
        while (true) {
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            items.push_back(std::move(value));
            SkipWs();
            char c = Peek();
            if (c == ',') { ++pos; continue; }
            if (c == ']') { ++pos; out = JsonValue::MakeArray(std::move(items)); return true; }
            return Fail("expected ',' or ']'");
        }
    }

    static void AppendUtf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool Hex4(std::uint32_t& cp) {
        if (pos + 4 > text.size()) return Fail("truncated \\u escape");
        cp = 0;
        for (int i = 0; i < 4; ++i) {
            char c = text[pos++];
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') cp |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') cp |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return Fail("bad hex digit in \\u escape");
        }
        return true;
    }

    bool ParseString(std::string& out) {
        ++pos; // '"'
        out.clear();
        while (true) {
            if (Eof()) return Fail("unterminated string");
            char c = text[pos++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20)
                return Fail("unescaped control character in string");
            if (c != '\\') { out += c; continue; }
            if (Eof()) return Fail("unterminated escape");
            char e = text[pos++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp;
                    if (!Hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        // high surrogate — require a low surrogate to follow
                        if (pos + 2 > text.size() || text[pos] != '\\' || text[pos + 1] != 'u')
                            return Fail("lone high surrogate in \\u escape");
                        pos += 2;
                        std::uint32_t lo;
                        if (!Hex4(lo)) return false;
                        if (lo < 0xDC00 || lo > 0xDFFF)
                            return Fail("expected low surrogate in \\u escape");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return Fail("lone low surrogate in \\u escape");
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: return Fail("bad escape character");
            }
        }
    }

    bool ParseNumber(JsonValue& out) {
        const std::size_t start = pos;
        if (Peek() == '-') ++pos;

        // integer part: 0 | [1-9][0-9]*  (leading zeros invalid)
        if (Peek() == '0') {
            ++pos;
        } else if (Peek() >= '1' && Peek() <= '9') {
            while (Peek() >= '0' && Peek() <= '9') ++pos;
        } else {
            return Fail("invalid number");
        }

        bool real = false;
        if (Peek() == '.') {
            real = true;
            ++pos;
            if (!(Peek() >= '0' && Peek() <= '9')) return Fail("expected digit after '.'");
            while (Peek() >= '0' && Peek() <= '9') ++pos;
        }
        if (Peek() == 'e' || Peek() == 'E') {
            real = true;
            ++pos;
            if (Peek() == '+' || Peek() == '-') ++pos;
            if (!(Peek() >= '0' && Peek() <= '9')) return Fail("expected digit in exponent");
            while (Peek() >= '0' && Peek() <= '9') ++pos;
        }

        const std::string_view tok = text.substr(start, pos - start);
        if (!real) {
            // from_chars: exact int64 range incl. INT64_MIN; huge integer
            // literals degrade to Real rather than hard-rejecting (RFC 8259
            // leaves range implementation-defined).
            std::int64_t v = 0;
            const auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), v);
            if (ec == std::errc() && ptr == tok.data() + tok.size()) {
                out = JsonValue::Int(v);
                return true;
            }
            if (ec != std::errc::result_out_of_range) return Fail("invalid number");
        }
        {
            double v = 0.0;
            const auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), v);
            if (ec == std::errc::result_out_of_range || !std::isfinite(v))
                return Fail("real number out of range");
            if (ec != std::errc() || ptr != tok.data() + tok.size())
                return Fail("invalid real number");
            out = JsonValue::Real(v);
        }
        return true;
    }
};

} // namespace

JsonValue JsonValue::Bool(bool v) { JsonValue j; j.type_ = Type::Bool; j.bool_ = v; return j; }
JsonValue JsonValue::Int(std::int64_t v) { JsonValue j; j.type_ = Type::Int; j.int_ = v; return j; }
JsonValue JsonValue::Real(double v) { JsonValue j; j.type_ = Type::Real; j.real_ = v; return j; }
JsonValue JsonValue::String(std::string v) { JsonValue j; j.type_ = Type::String; j.string_ = std::move(v); return j; }
JsonValue JsonValue::MakeArray(Array v) { JsonValue j; j.type_ = Type::Array; j.array_ = std::move(v); return j; }
JsonValue JsonValue::MakeObject(Object v) { JsonValue j; j.type_ = Type::Object; j.object_ = std::move(v); return j; }

Result<JsonValue> JsonValue::Parse(std::string_view text) {
    Parser p{ text, 0, {} };
    JsonValue out;
    if (!p.ParseValue(out, 1)) return Fail<JsonValue>("parse", p.err);
    p.SkipWs();
    if (!p.Eof()) {
        p.Fail("trailing characters after document");
        return Fail<JsonValue>("parse", p.err);
    }
    return Ok(std::move(out));
}

bool JsonValue::AsBool(bool fallback) const {
    return type_ == Type::Bool ? bool_ : fallback;
}
std::int64_t JsonValue::AsInt(std::int64_t fallback) const {
    if (type_ == Type::Int) return int_;
    if (type_ == Type::Real) {
        // Casting non-finite or out-of-range doubles to int64 is UB — guard.
        if (std::isfinite(real_) &&
            real_ >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
            real_ < 9223372036854775808.0) {
            return static_cast<std::int64_t>(real_);
        }
    }
    return fallback;
}
double JsonValue::AsDouble(double fallback) const {
    if (type_ == Type::Real) return real_;
    if (type_ == Type::Int) return static_cast<double>(int_);
    return fallback;
}
const std::string& JsonValue::AsString() const {
    return type_ == Type::String ? string_ : EmptyString();
}
const JsonValue& JsonValue::At(std::size_t index) const {
    return (type_ == Type::Array && index < array_.size()) ? array_[index] : NullSingleton();
}
const JsonValue& JsonValue::operator[](std::string_view key) const {
    if (type_ != Type::Object) return NullSingleton();
    auto it = object_.find(key);
    return it != object_.end() ? it->second : NullSingleton();
}
bool JsonValue::Has(std::string_view key) const {
    return type_ == Type::Object && object_.find(key) != object_.end();
}
std::size_t JsonValue::Size() const {
    if (type_ == Type::Array) return array_.size();
    if (type_ == Type::Object) return object_.size();
    return 0;
}
const JsonValue::Array& JsonValue::Items() const {
    return type_ == Type::Array ? array_ : EmptyArray();
}
const JsonValue::Object& JsonValue::Members() const {
    return type_ == Type::Object ? object_ : EmptyObject();
}
const std::string* JsonValue::FindString(std::string_view key) const {
    if (type_ != Type::Object) return nullptr;
    auto it = object_.find(key);
    return (it != object_.end() && it->second.IsString()) ? &it->second.string_ : nullptr;
}

namespace {

void EmitStringTo(const std::string& s, std::string& out) {
    static const char hex[] = "0123456789abcdef";
    out += '"';
    for (const unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += hex[(c >> 4) & 0xF];
                    out += hex[c & 0xF];
                } else {
                    out += static_cast<char>(c); // UTF-8 passthrough
                }
        }
    }
    out += '"';
}

} // namespace

std::string JsonValue::Emit() const {
    std::string out;
    EmitTo(out);
    return out;
}

void JsonValue::EmitTo(std::string& out) const {
    switch (type_) {
        case Type::Null:   out += "null";                    break;
        case Type::Bool:   out += bool_ ? "true" : "false";  break;
        case Type::Int: {
            // int64 min is the only value whose negation overflows —
            // render digit-wise to stay portable and UB-free.
            if (int_ == std::numeric_limits<std::int64_t>::min()) {
                out += "-9223372036854775808";
                break;
            }
            std::uint64_t mag = int_ < 0
                ? static_cast<std::uint64_t>(-int_)
                : static_cast<std::uint64_t>(int_);
            char buf[20];
            int n = 0;
            do { buf[n++] = static_cast<char>('0' + mag % 10); mag /= 10; }
            while (mag != 0);
            if (int_ < 0) out += '-';
            while (n > 0) out += buf[--n];
            break;
        }
        case Type::Real: {
            // std::to_chars: shortest round-trip, locale-independent.
            // JSON cannot represent non-finite values — emit `null`
            // (documented lossy; the parser would reject nan/inf anyway).
            if (!std::isfinite(real_)) {
                out += "null";
                break;
            }
            char buf[32];
            const auto res = std::to_chars(buf, buf + sizeof(buf), real_);
            if (res.ec != std::errc()) {
                out += "0.0"; // unreachable in practice
                break;
            }
            out.append(buf, static_cast<std::size_t>(res.ptr - buf));
            // Keep the type stable through parse∘emit: an integral-looking
            // emission ("2", "1e3") would re-parse as Type::Int.
            bool typeMarker = false;
            for (const char* p = buf; p != res.ptr; ++p) {
                if (*p == '.' || *p == 'e' || *p == 'E') { typeMarker = true; break; }
            }
            if (!typeMarker) out += ".0";
            break;
        }
        case Type::String: EmitStringTo(string_, out); break;
        case Type::Array:
            out += '[';
            for (std::size_t i = 0; i < array_.size(); ++i) {
                if (i) out += ',';
                array_[i].EmitTo(out);
            }
            out += ']';
            break;
        case Type::Object:
            out += '{';
            for (auto it = object_.begin(); it != object_.end(); ++it) {
                if (it != object_.begin()) out += ',';
                EmitStringTo(it->first, out);
                out += ':';
                it->second.EmitTo(out);
            }
            out += '}';
            break;
    }
}

const JsonValue& JsonValue::NullSingleton() {
    static const JsonValue v;
    return v;
}
const std::string& JsonValue::EmptyString() {
    static const std::string s;
    return s;
}
const JsonValue::Array& JsonValue::EmptyArray() {
    static const Array v;
    return v;
}
const JsonValue::Object& JsonValue::EmptyObject() {
    static const Object v;
    return v;
}

} // namespace Potato::Gameplay
