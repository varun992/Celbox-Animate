#include "anim/json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace anim::json {
namespace {
const Value kNull;
const std::string kEmptyStr;
const Array kEmptyArr;
const Object kEmptyObj;

void writeString(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                else out += char(c);
        }
    }
    out += '"';
}

void writeValue(std::string& out, const Value& v) {
    if (v.isNull()) { out += "null"; return; }
    if (auto* b = std::get_if<bool>(&v.v)) { out += *b ? "true" : "false"; return; }
    if (auto* d = std::get_if<double>(&v.v)) {
        if (!std::isfinite(*d)) { out += "0"; return; }
        if (*d == std::floor(*d) && std::abs(*d) < 9e15) { out += std::to_string(int64_t(*d)); return; }
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.9g", *d);
        out += buf;
        return;
    }
    if (auto* s = std::get_if<std::string>(&v.v)) { writeString(out, *s); return; }
    if (v.isArray()) {
        out += '[';
        bool first = true;
        for (const auto& e : v.arr()) { if (!first) out += ','; first = false; writeValue(out, e); }
        out += ']';
        return;
    }
    out += '{';
    bool first = true;
    for (const auto& [k, e] : v.obj()) {
        if (!first) out += ',';
        first = false;
        writeString(out, k);
        out += ':';
        writeValue(out, e);
    }
    out += '}';
}

struct Parser {
    std::string_view s;
    size_t p = 0;
    std::string err;

    void ws() { while (p < s.size() && (s[p] == ' ' || s[p] == '\n' || s[p] == '\r' || s[p] == '\t')) ++p; }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at " + std::to_string(p); return false; }

    bool parseString(std::string& out) {
        if (s[p] != '"') return fail("expected string");
        ++p;
        while (p < s.size() && s[p] != '"') {
            char c = s[p++];
            if (c != '\\') { out += c; continue; }
            if (p >= s.size()) return fail("bad escape");
            char e = s[p++];
            switch (e) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (p + 4 > s.size()) return fail("bad unicode escape");
                    unsigned cp = unsigned(std::strtoul(std::string(s.substr(p, 4)).c_str(), nullptr, 16));
                    p += 4;
                    if (cp < 0x80) out += char(cp);
                    else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F)); }
                    else { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
                    break;
                }
                default: out += e;
            }
        }
        if (p >= s.size()) return fail("unterminated string");
        ++p;
        return true;
    }

    bool parseValue(Value& out, int depth) {
        if (depth > 256) return fail("too deep");
        ws();
        if (p >= s.size()) return fail("unexpected end");
        const char c = s[p];
        if (c == '{') {
            ++p;
            Object o;
            ws();
            if (p < s.size() && s[p] == '}') { ++p; out = Value(std::move(o)); return true; }
            while (true) {
                ws();
                std::string key;
                if (p >= s.size() || !parseString(key)) return fail("expected key");
                ws();
                if (p >= s.size() || s[p] != ':') return fail("expected ':'");
                ++p;
                Value v;
                if (!parseValue(v, depth + 1)) return false;
                o.emplace(std::move(key), std::move(v));
                ws();
                if (p < s.size() && s[p] == ',') { ++p; continue; }
                if (p < s.size() && s[p] == '}') { ++p; break; }
                return fail("expected ',' or '}'");
            }
            out = Value(std::move(o));
            return true;
        }
        if (c == '[') {
            ++p;
            Array a;
            ws();
            if (p < s.size() && s[p] == ']') { ++p; out = Value(std::move(a)); return true; }
            while (true) {
                Value v;
                if (!parseValue(v, depth + 1)) return false;
                a.push_back(std::move(v));
                ws();
                if (p < s.size() && s[p] == ',') { ++p; continue; }
                if (p < s.size() && s[p] == ']') { ++p; break; }
                return fail("expected ',' or ']'");
            }
            out = Value(std::move(a));
            return true;
        }
        if (c == '"') { std::string str; if (!parseString(str)) return false; out = Value(std::move(str)); return true; }
        if (s.substr(p, 4) == "true") { p += 4; out = Value(true); return true; }
        if (s.substr(p, 5) == "false") { p += 5; out = Value(false); return true; }
        if (s.substr(p, 4) == "null") { p += 4; out = Value(); return true; }
        // number
        const size_t start = p;
        while (p < s.size() && (std::isdigit((unsigned char)s[p]) || s[p] == '-' || s[p] == '+' || s[p] == '.' || s[p] == 'e' || s[p] == 'E')) ++p;
        if (start == p) return fail("unexpected character");
        out = Value(std::strtod(std::string(s.substr(start, p - start)).c_str(), nullptr));
        return true;
    }
};
}  // namespace

const std::string& Value::str() const { auto* x = std::get_if<std::string>(&v); return x ? *x : kEmptyStr; }
const Array& Value::arr() const { auto* x = std::get_if<std::shared_ptr<Array>>(&v); return x ? **x : kEmptyArr; }
const Object& Value::obj() const { auto* x = std::get_if<std::shared_ptr<Object>>(&v); return x ? **x : kEmptyObj; }
const Value& Value::operator[](const std::string& key) const {
    const auto& o = obj();
    auto it = o.find(key);
    return it == o.end() ? kNull : it->second;
}

std::string write(const Value& v) {
    std::string out;
    writeValue(out, v);
    return out;
}

std::optional<Value> parse(std::string_view text, std::string* error) {
    Parser ps{text};
    Value v;
    if (!ps.parseValue(v, 0)) { if (error) *error = ps.err; return std::nullopt; }
    ps.ws();
    if (ps.p != text.size()) { if (error) *error = "trailing characters"; return std::nullopt; }
    return v;
}

}  // namespace anim::json
