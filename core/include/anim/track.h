// AnimTrack<T>: a keyframed, interpolated property. Every animatable value in
// the app (transform, opacity, effect params, camera, audio gain) is one of these,
// so motion tweens, the graph editor and AI tooling all share one code path.
#pragma once
#include <algorithm>
#include <vector>

#include "anim/easing.h"
#include "anim/math.h"

namespace anim {

template <class T>
struct Key {
    int frame = 0;
    T value{};
    Easing ease{};  // easing of the segment that *starts* at this key
};

template <class T>
class AnimTrack {
public:
    AnimTrack() = default;
    explicit AnimTrack(T defaultValue) : default_(defaultValue) {}

    void setKey(int frame, T value, Easing ease = Easing::linear()) {
        auto it = lowerBound(frame);
        if (it != keys_.end() && it->frame == frame) { it->value = value; it->ease = ease; return; }
        keys_.insert(it, Key<T>{frame, value, ease});
    }
    bool removeKey(int frame) {
        auto it = lowerBound(frame);
        if (it == keys_.end() || it->frame != frame) return false;
        keys_.erase(it);
        return true;
    }
    bool hasKey(int frame) const {
        auto it = std::lower_bound(keys_.begin(), keys_.end(), frame,
                                   [](const Key<T>& k, int f) { return k.frame < f; });
        return it != keys_.end() && it->frame == frame;
    }
    // Shift every key at or after `fromFrame` (used by insert/remove frames).
    void shiftKeys(int fromFrame, int delta) {
        for (auto& k : keys_) if (k.frame >= fromFrame) k.frame += delta;
    }
    void setDefault(T v) { default_ = v; }

    // Fractional frames are allowed (sub-frame motion blur, audio scrubbing).
    T valueAt(double frame) const {
        if (keys_.empty()) return default_;
        if (frame <= keys_.front().frame) return keys_.front().value;
        if (frame >= keys_.back().frame) return keys_.back().value;
        auto hi = std::upper_bound(keys_.begin(), keys_.end(), frame,
                                   [](double f, const Key<T>& k) { return f < k.frame; });
        auto lo = hi - 1;
        const float t = float((frame - lo->frame) / double(hi->frame - lo->frame));
        return lerpValue(lo->value, hi->value, lo->ease.apply(t));
    }

    const std::vector<Key<T>>& keys() const { return keys_; }
    bool animated() const { return keys_.size() > 1; }
    bool empty() const { return keys_.empty(); }
    T defaultValue() const { return default_; }

private:
    typename std::vector<Key<T>>::iterator lowerBound(int frame) {
        return std::lower_bound(keys_.begin(), keys_.end(), frame,
                                [](const Key<T>& k, int f) { return k.frame < f; });
    }
    std::vector<Key<T>> keys_;
    T default_{};
};

// The transform every layer / folder / symbol instance carries.
struct TransformTracks {
    AnimTrack<Vec2> position{Vec2{0, 0}};
    AnimTrack<float> rotationDeg{0.0f};
    AnimTrack<Vec2> scale{Vec2{1, 1}};
    AnimTrack<float> skewDeg{0.0f};
    AnimTrack<Vec2> anchor{Vec2{0, 0}};   // pivot, in the layer's local space
    AnimTrack<float> opacity{1.0f};

    Affine matrixAt(double frame) const {
        const Vec2 an = anchor.valueAt(frame);
        return Affine::translate(position.valueAt(frame)) * Affine::rotateDeg(rotationDeg.valueAt(frame)) *
               Affine::skewXDeg(skewDeg.valueAt(frame)) * Affine::scale(scale.valueAt(frame)) *
               Affine::translate(an * -1.0f);
    }
    void shiftKeys(int fromFrame, int delta) {
        position.shiftKeys(fromFrame, delta); rotationDeg.shiftKeys(fromFrame, delta);
        scale.shiftKeys(fromFrame, delta); skewDeg.shiftKeys(fromFrame, delta);
        anchor.shiftKeys(fromFrame, delta); opacity.shiftKeys(fromFrame, delta);
    }
};

}  // namespace anim
