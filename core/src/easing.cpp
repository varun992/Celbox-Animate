#include "anim/easing.h"

#include <algorithm>
#include <cmath>

namespace anim {
namespace {
float bez(float p1, float p2, float s) {  // 1D cubic with P0=0, P3=1
    const float u = 1 - s;
    return 3 * u * u * s * p1 + 3 * u * s * s * p2 + s * s * s;
}
float bezDeriv(float p1, float p2, float s) {
    const float u = 1 - s;
    return 3 * u * u * p1 + 6 * u * s * (p2 - p1) + 3 * s * s * (1 - p2);
}
}  // namespace

float Easing::apply(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (kind) {
        case EaseKind::Linear: return t;
        case EaseKind::Hold: return t < 1.0f ? 0.0f : 1.0f;
        case EaseKind::CubicBezier: {
            // Solve x(s) = t with Newton, fall back to bisection.
            float s = t;
            for (int i = 0; i < 8; ++i) {
                const float err = bez(x1, x2, s) - t;
                if (std::abs(err) < 1e-6f) return bez(y1, y2, s);
                const float dv = bezDeriv(x1, x2, s);
                if (std::abs(dv) < 1e-6f) break;
                s = std::clamp(s - err / dv, 0.0f, 1.0f);
            }
            float lo = 0, hi = 1;
            s = t;
            for (int i = 0; i < 40; ++i) {
                const float x = bez(x1, x2, s);
                if (std::abs(x - t) < 1e-6f) break;
                (x < t ? lo : hi) = s;
                s = 0.5f * (lo + hi);
            }
            return bez(y1, y2, s);
        }
    }
    return t;
}

}  // namespace anim
