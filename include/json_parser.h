#pragma once
#include <string>
#include <vector>
#include <utility>
#include <memory>

// A small, purpose-built JSON reader. We only need to read tokenizer.json
// (vocab + merge rules), and pulling in a third-party JSON library wasn't
// possible today (the usual one, nlohmann/json, lives on a host our
// network setup couldn't reach) -- so this is a compact recursive-descent
// parser covering the JSON value types we actually need: objects, arrays,
// strings, and numbers.

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::shared_ptr<std::vector<JsonValue>> arrVal;
    std::shared_ptr<std::vector<std::pair<std::string, JsonValue>>> objVal;

    bool isObject() const { return type == JsonType::Object; }
    bool isArray()  const { return type == JsonType::Array; }
    bool isString() const { return type == JsonType::String; }
    bool isNumber() const { return type == JsonType::Number; }

    // Looks up a key inside an Object value. Returns nullptr if this isn't
    // an object or the key doesn't exist. Linear scan -- fine here, since
    // we only ever use this for a handful of structural keys (e.g. "model",
    // "vocab", "merges"), never for looking up individual vocab entries.
    const JsonValue* find(const std::string& key) const {
        if (type != JsonType::Object || !objVal) return nullptr;
        for (const auto& kv : *objVal) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
};

// Parses a full JSON document from text. Throws std::runtime_error on
// malformed input.
JsonValue parseJson(const std::string& text);
