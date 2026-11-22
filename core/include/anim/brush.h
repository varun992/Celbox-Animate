// Brush engine input pipeline: Apple Pencil / pen samples -> stabilized,
// evenly spaced stroke points. Vector brushes store the result; raster brushes
// stamp dabs along it on the GPU. Shared by every platform so strokes feel the
// same everywhere.
#pragma once
#include <vector>

#include "anim/drawing.h"

namespace anim::brush {

struct InputSample {
    Vec2 pos;
    float pressure = 1;   // 0..1 (Pencil force / max force)
    float tiltRad = 0;    // altitude, for shading brushes
    double timeSec = 0;
};

struct PressureCurve {
    float gamma = 1.0f;   // <1 softer, >1 firmer
    float minOut = 0.05f;
    float apply(float p) const;
};

struct StabilizerSettings {
    float lazyRadius = 0;   // "pulled string": cursor must move this far before ink follows
    int smoothingWindow = 4;  // moving average of the string tip (0 = off)
    float spacing = 2.0f;   // output point spacing in canvas px
};

class StrokeBuilder {
public:
    StrokeBuilder(StabilizerSettings s = {}, PressureCurve c = {}) : settings_(s), curve_(c) {}
    void begin(const InputSample& s);
    void add(const InputSample& s);  // call for each coalesced/predicted touch
    void end();                       // drains the stabilizer to the final pen point
    const std::vector<StrokePoint>& points() const { return out_; }

private:
    void emit(StrokePoint p);
    StabilizerSettings settings_;
    PressureCurve curve_;
    StrokePoint anchor_{};
    std::vector<StrokePoint> window_;
    std::vector<StrokePoint> out_;
    float carry_ = 0;  // distance travelled since the last emitted point
};

// Arc-length resample to exactly `count` points (used by tweening and AI prep).
std::vector<StrokePoint> resample(const std::vector<StrokePoint>& pts, int count);

// Dab centers for raster stamping, spaced as a fraction of brush diameter.
std::vector<StrokePoint> dabs(const std::vector<StrokePoint>& pts, float diameter, float spacingFraction = 0.15f);

}  // namespace anim::brush
