// Drawing aids: grid, rulers (line / ellipse / perspective) and symmetry.
// Tools feed raw pen positions through these before the brush engine.
#pragma once
#include <cmath>
#include <vector>

#include "anim/math.h"

namespace anim::assist {

struct Grid {
    float spacing = 64;
    Vec2 origin{0, 0};
    Vec2 snap(Vec2 p) const {
        return {origin.x + std::round((p.x - origin.x) / spacing) * spacing,
                origin.y + std::round((p.y - origin.y) / spacing) * spacing};
    }
};

struct LineRuler {
    Vec2 a, b;
    Vec2 project(Vec2 p) const {
        const Vec2 d = b - a;
        const float len2 = dot(d, d);
        if (len2 < 1e-9f) return a;
        return a + d * (dot(p - a, d) / len2);
    }
};

// Strokes started at `strokeStart` are constrained to the line through the vanishing point.
struct PerspectiveRuler {
    Vec2 vanishingPoint;
    Vec2 project(Vec2 strokeStart, Vec2 p) const { return LineRuler{vanishingPoint, strokeStart}.project(p); }
};

struct EllipseRuler {
    Vec2 center;
    float rx = 100, ry = 50, rotationDeg = 0;
    Vec2 project(Vec2 p) const {
        const Affine toLocal = (Affine::translate(center) * Affine::rotateDeg(rotationDeg)).inverse();
        const Vec2 l = toLocal.apply(p);
        const float ang = std::atan2(l.y / ry, l.x / rx);
        return (Affine::translate(center) * Affine::rotateDeg(rotationDeg)).apply({rx * std::cos(ang), ry * std::sin(ang)});
    }
};

// Radial symmetry with optional mirroring (kaleidoscope when both).
struct Symmetry {
    Vec2 center;
    int segments = 2;       // 2 + mirror = classic left/right symmetry
    bool mirror = true;
    float axisAngleDeg = 90; // mirror axis (90 = vertical)

    // Transforms to apply to each stroke; the identity is included first.
    std::vector<Affine> transforms() const {
        std::vector<Affine> out;
        const Affine toC = Affine::translate(center), fromC = Affine::translate(center * -1.0f);
        const int n = std::max(1, mirror ? segments / 2 : segments);
        for (int i = 0; i < n; ++i) {
            const Affine rot = toC * Affine::rotateDeg(360.0f * i / n) * fromC;
            out.push_back(rot);
            if (mirror) {
                const Affine reflect = Affine::rotateDeg(axisAngleDeg) * Affine::scale({1, -1}) * Affine::rotateDeg(-axisAngleDeg);
                out.push_back(rot * toC * reflect * fromC);
            }
        }
        return out;
    }
};

}  // namespace anim::assist
