// Minimal 2D math used by the model. Rendering backends convert Affine to their
// own matrix types (simd_float3x3 on Metal, D2D1_MATRIX_3X2_F on Windows).
#pragma once
#include <algorithm>
#include <cmath>

namespace anim {

constexpr double kPi = 3.14159265358979323846;

struct Vec2 {
    float x = 0, y = 0;
    constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr bool operator==(const Vec2&) const = default;
    float length() const { return std::sqrt(x * x + y * y); }
};
inline float distance(Vec2 a, Vec2 b) { return (a - b).length(); }
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

// Linear-light premultiplication is a renderer concern; the model stores
// straight (non-premultiplied) sRGB floats.
struct Color {
    float r = 0, g = 0, b = 0, a = 1;
    constexpr bool operator==(const Color&) const = default;
    static constexpr Color white() { return {1, 1, 1, 1}; }
    static constexpr Color black() { return {0, 0, 0, 1}; }
};

inline float lerpValue(float a, float b, float t) { return a + (b - a) * t; }
inline Vec2 lerpValue(Vec2 a, Vec2 b, float t) { return {lerpValue(a.x, b.x, t), lerpValue(a.y, b.y, t)}; }
inline Color lerpValue(Color a, Color b, float t) {
    return {lerpValue(a.r, b.r, t), lerpValue(a.g, b.g, t), lerpValue(a.b, b.b, t), lerpValue(a.a, b.a, t)};
}

// x' = a*x + c*y + tx ; y' = b*x + d*y + ty
struct Affine {
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;

    static Affine translate(Vec2 t) { return {1, 0, 0, 1, t.x, t.y}; }
    static Affine scale(Vec2 s) { return {s.x, 0, 0, s.y, 0, 0}; }
    static Affine rotateDeg(float deg) {
        const float r = float(deg * kPi / 180.0), cs = std::cos(r), sn = std::sin(r);
        return {cs, sn, -sn, cs, 0, 0};
    }
    static Affine skewXDeg(float deg) { return {1, 0, std::tan(float(deg * kPi / 180.0)), 1, 0, 0}; }

    // (A * B)(p) == A(B(p))
    Affine operator*(const Affine& o) const {
        return {a * o.a + c * o.b, b * o.a + d * o.b, a * o.c + c * o.d,
                b * o.c + d * o.d, a * o.tx + c * o.ty + tx, b * o.tx + d * o.ty + ty};
    }
    Vec2 apply(Vec2 p) const { return {a * p.x + c * p.y + tx, b * p.x + d * p.y + ty}; }
    float determinant() const { return a * d - b * c; }
    Affine inverse() const {
        const float det = determinant();
        if (std::abs(det) < 1e-12f) return {};
        const float id = 1.0f / det;
        return {d * id, -b * id, -c * id, a * id, (c * ty - d * tx) * id, (b * tx - a * ty) * id};
    }
    bool approxEqual(const Affine& o, float eps = 1e-4f) const {
        return std::abs(a - o.a) < eps && std::abs(b - o.b) < eps && std::abs(c - o.c) < eps &&
               std::abs(d - o.d) < eps && std::abs(tx - o.tx) < eps && std::abs(ty - o.ty) < eps;
    }
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
};

}  // namespace anim
