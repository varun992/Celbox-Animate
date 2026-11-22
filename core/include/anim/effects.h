// Animatable effects (blur, glow, ...). The model stores keyframed parameters;
// GPU backends implement the actual filter (Metal compute / D3D12 / Vulkan).
#pragma once
#include <vector>

#include "anim/track.h"

namespace anim {

enum class EffectType { GaussianBlur, Glow, DropShadow, ColorAdjust, MotionBlur };

struct EffectParams {
    EffectType type;
    float radius, intensity;
    Color color;
    Vec2 offset;
};

struct Effect {
    EffectType type = EffectType::GaussianBlur;
    bool enabled = true;
    AnimTrack<float> radius{4.0f};
    AnimTrack<float> intensity{1.0f};
    AnimTrack<Color> color{Color::white()};
    AnimTrack<Vec2> offset{Vec2{0, 0}};

    EffectParams evaluate(double frame) const {
        return {type, radius.valueAt(frame), intensity.valueAt(frame), color.valueAt(frame), offset.valueAt(frame)};
    }
    void shiftKeys(int from, int delta) {
        radius.shiftKeys(from, delta); intensity.shiftKeys(from, delta);
        color.shiftKeys(from, delta); offset.shiftKeys(from, delta);
    }
};

}  // namespace anim
