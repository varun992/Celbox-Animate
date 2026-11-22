#include "anim/brush.h"

#include <algorithm>
#include <cmath>

namespace anim::brush {

float PressureCurve::apply(float p) const {
    p = std::clamp(p, 0.0f, 1.0f);
    return minOut + (1.0f - minOut) * std::pow(p, gamma);
}

void StrokeBuilder::begin(const InputSample& s) {
    out_.clear();
    window_.clear();
    carry_ = 0;
    anchor_ = {s.pos, curve_.apply(s.pressure)};
    out_.push_back(anchor_);
    window_.push_back(anchor_);
}

void StrokeBuilder::emit(StrokePoint p) {
    // Even spacing along the smoothed path.
    StrokePoint last = out_.back();
    float dist = distance(last.pos, p.pos);
    while (carry_ + dist >= settings_.spacing && dist > 1e-6f) {
        const float t = (settings_.spacing - carry_) / dist;
        StrokePoint q{lerpValue(last.pos, p.pos, t), lerpValue(last.pressure, p.pressure, t)};
        out_.push_back(q);
        last = q;
        dist = distance(last.pos, p.pos);
        carry_ = 0;
    }
    carry_ += dist;
}

void StrokeBuilder::add(const InputSample& s) {
    StrokePoint target{s.pos, curve_.apply(s.pressure)};
    // Pulled string.
    const float d = distance(anchor_.pos, target.pos);
    if (d > settings_.lazyRadius) {
        const float t = (d - settings_.lazyRadius) / d;
        anchor_.pos = lerpValue(anchor_.pos, target.pos, t);
    }
    anchor_.pressure = target.pressure;
    // Moving average.
    StrokePoint smoothed = anchor_;
    if (settings_.smoothingWindow > 0) {
        window_.push_back(anchor_);
        if (int(window_.size()) > settings_.smoothingWindow) window_.erase(window_.begin());
        Vec2 acc{0, 0};
        float pr = 0;
        for (auto& w : window_) { acc = acc + w.pos; pr += w.pressure; }
        smoothed = {acc * (1.0f / window_.size()), pr / window_.size()};
    }
    emit(smoothed);
}

void StrokeBuilder::end() {
    // Catch up to the real pen position so strokes end where the pen lifted.
    if (window_.empty()) return;
    const StrokePoint finalPt = anchor_;
    for (int i = 0; i < settings_.smoothingWindow; ++i) {
        window_.push_back(finalPt);
        window_.erase(window_.begin());
        Vec2 acc{0, 0};
        float pr = 0;
        for (auto& w : window_) { acc = acc + w.pos; pr += w.pressure; }
        emit({acc * (1.0f / window_.size()), pr / window_.size()});
    }
    if (distance(out_.back().pos, finalPt.pos) > 1e-3f) out_.push_back(finalPt);
}

std::vector<StrokePoint> resample(const std::vector<StrokePoint>& pts, int count) {
    std::vector<StrokePoint> out;
    if (pts.empty() || count <= 0) return out;
    if (pts.size() == 1 || count == 1) return std::vector<StrokePoint>(size_t(count), pts.front());
    std::vector<float> cum(pts.size(), 0.0f);
    for (size_t i = 1; i < pts.size(); ++i) cum[i] = cum[i - 1] + distance(pts[i - 1].pos, pts[i].pos);
    const float total = cum.back();
    size_t seg = 1;
    for (int i = 0; i < count; ++i) {
        const float target = total * float(i) / float(count - 1);
        while (seg < pts.size() - 1 && cum[seg] < target) ++seg;
        const float segLen = cum[seg] - cum[seg - 1];
        const float t = segLen > 1e-6f ? (target - cum[seg - 1]) / segLen : 0.0f;
        out.push_back({lerpValue(pts[seg - 1].pos, pts[seg].pos, t), lerpValue(pts[seg - 1].pressure, pts[seg].pressure, t)});
    }
    return out;
}

std::vector<StrokePoint> dabs(const std::vector<StrokePoint>& pts, float diameter, float spacingFraction) {
    std::vector<StrokePoint> out;
    if (pts.empty()) return out;
    const float step = std::max(0.5f, diameter * spacingFraction);
    float total = 0;
    for (size_t i = 1; i < pts.size(); ++i) total += distance(pts[i - 1].pos, pts[i].pos);
    const int n = std::max(1, int(total / step) + 1);
    return resample(pts, n);
}

}  // namespace anim::brush
