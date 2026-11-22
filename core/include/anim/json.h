// Tiny dependency-free JSON value, writer and parser used by the document
// format. Numbers are doubles; that is enough for frames, ids < 2^53 and floats.
#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace anim::json {

struct Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

struct Value {
    std::variant<std::nullptr_t, bool, double, std::string, std::shared_ptr<Array>, std::shared_ptr<Object>> v = nullptr;

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : v(b) {}
    Value(double d) : v(d) {}
    Value(float d) : v(double(d)) {}
    Value(int i) : v(double(i)) {}
    Value(uint64_t i) : v(double(i)) {}
    Value(const char* s) : v(std::string(s)) {}
    Value(std::string s) : v(std::move(s)) {}
    Value(Array a) : v(std::make_shared<Array>(std::move(a))) {}
    Value(Object o) : v(std::make_shared<Object>(std::move(o))) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(v); }
    bool isObject() const { return std::holds_alternative<std::shared_ptr<Object>>(v); }
    bool isArray() const { return std::holds_alternative<std::shared_ptr<Array>>(v); }

    double num(double def = 0) const { auto* d = std::get_if<double>(&v); return d ? *d : def; }
    int i(int def = 0) const { return int(num(def)); }
    uint64_t u64() const { return uint64_t(num(0)); }
    float f(float def = 0) const { return float(num(def)); }
    bool b(bool def = false) const { auto* x = std::get_if<bool>(&v); return x ? *x : def; }
    const std::string& str() const;
    const Array& arr() const;
    const Object& obj() const;

    // Object lookup; returns a null value when missing (safe chaining).
    const Value& operator[](const std::string& key) const;
};

std::string write(const Value& v);
std::optional<Value> parse(std::string_view text, std::string* error = nullptr);

}  // namespace anim::json
