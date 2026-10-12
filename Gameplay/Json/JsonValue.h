#pragma once

#include "Gameplay/Result.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Potato::Gameplay {

// JSON DOM node for all versioned game content (potato.<name>/<ver>).
// Boundary type only — parsing returns Result<JsonValue>, accessors are
// defensive (type mismatch returns fallback, never throws).
class JsonValue {
public:
    enum class Type { Null, Bool, Int, Real, String, Array, Object };

    using Array = std::vector<JsonValue>;
    // std::less<> gives transparent lookup — find(string_view) without
    // allocating a temporary std::string per query.
    using Object = std::map<std::string, JsonValue, std::less<>>;

    JsonValue() = default;
    static JsonValue Bool(bool v);
    static JsonValue Int(std::int64_t v);
    static JsonValue Real(double v);
    static JsonValue String(std::string v);
    static JsonValue MakeArray(Array v);
    static JsonValue MakeObject(Object v);

    // Parse a complete JSON document (RFC 8259). Rejects trailing
    // garbage and nesting deeper than 64. Never throws.
    // Expects BOM-free text — Json::Load strips the BOM for file loads;
    // other callers must strip it themselves.
    static Result<JsonValue> Parse(std::string_view text);

    Type GetType() const { return type_; }
    bool IsNull() const { return type_ == Type::Null; }
    bool IsBool() const { return type_ == Type::Bool; }
    bool IsInt() const { return type_ == Type::Int; }
    bool IsReal() const { return type_ == Type::Real; }
    bool IsString() const { return type_ == Type::String; }
    bool IsArray() const { return type_ == Type::Array; }
    bool IsObject() const { return type_ == Type::Object; }

    // Numeric reads coerce Int <-> Real; other mismatches return fallback.
    bool AsBool(bool fallback = false) const;
    std::int64_t AsInt(std::int64_t fallback = 0) const;
    double AsDouble(double fallback = 0.0) const;
    const std::string& AsString() const;

    const JsonValue& At(std::size_t index) const;           // array; Null if OOB
    const JsonValue& operator[](std::string_view key) const; // object; Null if absent
    bool Has(std::string_view key) const;
    std::size_t Size() const; // array/object member count; 0 otherwise

    const Array& Items() const;
    const Object& Members() const;

    const std::string* FindString(std::string_view key) const;

    // Canonical serializer (Story 1.11): whitespace-free, object keys in
    // std::less<> map order, ints exact, reals via %.17g, strings
    // JSON-escaped. Byte-stable for a given DOM — Emit after Parse is a
    // canonical form, so records hash deterministically.
    std::string Emit() const;
    void EmitTo(std::string& out) const;

private:
    static const JsonValue& NullSingleton();
    static const std::string& EmptyString();
    static const Array& EmptyArray();
    static const Object& EmptyObject();

    Type type_ = Type::Null;
    bool bool_ = false;
    std::int64_t int_ = 0;
    double real_ = 0.0;
    std::string string_;
    Array array_;
    Object object_;
};

} // namespace Potato::Gameplay
