// Easing curves for tweens. A cubic-bezier easing uses the same convention as
// CSS / most animation tools: control points (x1,y1) and (x2,y2) on a unit curve.
#pragma once

namespace anim {

enum class EaseKind { Linear, Hold, CubicBezier };

struct Easing {
    EaseKind kind = EaseKind::Linear;
    float x1 = 0, y1 = 0, x2 = 1, y2 = 1;

    static Easing linear() { return {}; }
    static Easing hold() { return {EaseKind::Hold}; }
    static Easing bezier(float x1, float y1, float x2, float y2) { return {EaseKind::CubicBezier, x1, y1, x2, y2}; }
    static Easing easeIn() { return bezier(0.42f, 0, 1, 1); }
    static Easing easeOut() { return bezier(0, 0, 0.58f, 1); }
    static Easing easeInOut() { return bezier(0.42f, 0, 0.58f, 1); }

    // t in [0,1] -> eased progress (may overshoot for bezier y outside [0,1]).
    float apply(float t) const;
    bool operator==(const Easing&) const = default;
};

}  // namespace anim
