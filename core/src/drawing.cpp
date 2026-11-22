#include "anim/drawing.h"

#include <algorithm>
#include <limits>

namespace anim {

Rect VectorDrawing::bounds() const {
    float x0 = std::numeric_limits<float>::max(), y0 = x0, x1 = -x0, y1 = -x0;
    auto add = [&](Vec2 p, float pad) {
        x0 = std::min(x0, p.x - pad); y0 = std::min(y0, p.y - pad);
        x1 = std::max(x1, p.x + pad); y1 = std::max(y1, p.y + pad);
    };
    for (const auto& s : strokes) for (const auto& p : s.points) add(p.pos, s.width * 0.5f);
    for (const auto& f : fills) for (const auto& p : f.contour) add(p, 0);
    if (x1 < x0) return {};
    return {x0, y0, x1 - x0, y1 - y0};
}

}  // namespace anim
