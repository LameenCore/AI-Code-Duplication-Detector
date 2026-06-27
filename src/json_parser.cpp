#include "json_parser.h"
#include <stdexcept>
#include <cctype>
#include <cstdlib>

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : s(text), n(text.size()), pos(0) {}

    JsonValue parse() {
        skipWhitespace();
        return parseValue();
    }

private:
    const std::string& s;
    size_t n;
    size_t pos;

    void skipWhitespace() {
        while (pos < n && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) pos++;
    }

    char peek() {
        if (pos >= n) throw std::runtime_error("JSON: unexpected end of input");
        return s[pos];
    }

    JsonValue parseValue() {
        skipWhitespace();
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        return parseNumber();
    }

    JsonValue parseObject() {
        JsonValue v;
        v.type = JsonType::Object;
        v.objVal = std::make_shared<std::vector<std::pair<std::string, JsonValue>>>();
        pos++; // {
        skipWhitespace();
        if (pos < n && s[pos] == '}') { pos++; return v; }
        while (true) {
            skipWhitespace();
            JsonValue keyVal = parseString();
            skipWhitespace();
            if (peek() != ':') throw std::runtime_error("JSON: expected ':'");
            pos++; // :
            JsonValue val = parseValue();
            v.objVal->emplace_back(std::move(keyVal.strVal), std::move(val));
            skipWhitespace();
            char c = peek();
            if (c == ',') { pos++; continue; }
            if (c == '}') { pos++; break; }
            throw std::runtime_error("JSON: expected ',' or '}'");
        }
        return v;
    }

    JsonValue parseArray() {
        JsonValue v;
        v.type = JsonType::Array;
        v.arrVal = std::make_shared<std::vector<JsonValue>>();
        pos++; // [
        skipWhitespace();
        if (pos < n && s[pos] == ']') { pos++; return v; }
        while (true) {
            JsonValue val = parseValue();
            v.arrVal->push_back(std::move(val));
            skipWhitespace();
            char c = peek();
            if (c == ',') { pos++; continue; }
            if (c == ']') { pos++; break; }
            throw std::runtime_error("JSON: expected ',' or ']'");
        }
        return v;
    }

    static void appendUtf8(std::string& out, unsigned int codepoint) {
        if (codepoint <= 0x7F) {
            out += (char)codepoint;
        } else if (codepoint <= 0x7FF) {
            out += (char)(0xC0 | (codepoint >> 6));
            out += (char)(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0xFFFF) {
            out += (char)(0xE0 | (codepoint >> 12));
            out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
            out += (char)(0x80 | (codepoint & 0x3F));
        } else {
            out += (char)(0xF0 | (codepoint >> 18));
            out += (char)(0x80 | ((codepoint >> 12) & 0x3F));
            out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
            out += (char)(0x80 | (codepoint & 0x3F));
        }
    }

    JsonValue parseString() {
        if (peek() != '"') throw std::runtime_error("JSON: expected '\"'");
        pos++; // opening quote
        std::string out;
        while (true) {
            if (pos >= n) throw std::runtime_error("JSON: unterminated string");
            char c = s[pos];
            if (c == '"') { pos++; break; }
            if (c == '\\') {
                pos++;
                if (pos >= n) throw std::runtime_error("JSON: unterminated escape");
                char e = s[pos];
                switch (e) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        if (pos + 4 >= n) throw std::runtime_error("JSON: bad \\u escape");
                        unsigned int cp = (unsigned int)std::strtoul(s.substr(pos + 1, 4).c_str(), nullptr, 16);
                        appendUtf8(out, cp);
                        pos += 4;
                        break;
                    }
                    default:
                        out += e;
                }
                pos++;
            } else {
                out += c;
                pos++;
            }
        }
        JsonValue v;
        v.type = JsonType::String;
        v.strVal = std::move(out);
        return v;
    }

    JsonValue parseNumber() {
        size_t start = pos;
        if (pos < n && (s[pos] == '-' || s[pos] == '+')) pos++;
        while (pos < n && (std::isdigit((unsigned char)s[pos]) || s[pos] == '.' ||
                            s[pos] == 'e' || s[pos] == 'E' || s[pos] == '+' || s[pos] == '-')) pos++;
        std::string numStr = s.substr(start, pos - start);
        JsonValue v;
        v.type = JsonType::Number;
        v.numVal = std::strtod(numStr.c_str(), nullptr);
        return v;
    }

    JsonValue parseBool() {
        if (s.compare(pos, 4, "true") == 0) {
            pos += 4;
            JsonValue v; v.type = JsonType::Bool; v.boolVal = true; return v;
        }
        if (s.compare(pos, 5, "false") == 0) {
            pos += 5;
            JsonValue v; v.type = JsonType::Bool; v.boolVal = false; return v;
        }
        throw std::runtime_error("JSON: invalid literal");
    }

    JsonValue parseNull() {
        if (s.compare(pos, 4, "null") == 0) {
            pos += 4;
            JsonValue v; v.type = JsonType::Null; return v;
        }
        throw std::runtime_error("JSON: invalid literal");
    }
};

} // namespace

JsonValue parseJson(const std::string& text) {
    Parser p(text);
    return p.parse();
}
