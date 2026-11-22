// Layer tree and per-layer exposure (the horizontal timeline row).
//
// Exposure model (Animate-style): a row of frames where each keyframe starts a
// span that lasts until the next keyframe. A keyframe points at a Drawing
// (vector/raster layers), a SymbolInstance (symbol layers) or a child cel
// (Clip Studio-style animation folders). Blank keyframe = key with no content.
#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "anim/audio.h"
#include "anim/effects.h"
#include "anim/ids.h"
#include "anim/track.h"

namespace anim {

enum class LayerKind {
    Folder,   // groups children; animatable transform + effects (CSP-style folders)
    Vector,
    Raster,
    Symbol,   // instances of a symbol (graphic / movie clip) with its own timeline
    Audio,
    Model3D,  // future: glTF/USD object for Blender round-trip (not implemented yet)
};

enum class BlendMode { Normal, Multiply, Screen, Add, Overlay };

enum class LoopMode { Loop, PlayOnce, SingleFrame, PlayOnceReverse };

struct SymbolInstance {
    SymbolId symbol;
    LoopMode loop = LoopMode::Loop;
    int firstFrame = 0;
    bool operator==(const SymbolInstance&) const = default;
};

struct Keyframe {
    DrawingId drawing;                      // Vector / Raster layers
    std::optional<SymbolInstance> instance; // Symbol layers
    LayerId cel;                            // animation folders: which child cel shows
    std::string label;                      // frame label / comment
    bool blank() const { return !drawing && !instance && !cel; }
};

class Exposure {
public:
    struct Span { int start; int end; const Keyframe* key; };  // [start, end)

    // Key active at `frame`, or nullptr past the end / before the first key.
    std::optional<Span> spanAt(int frame) const {
        if (frame < 0 || frame >= end_ || keys_.empty()) return std::nullopt;
        auto it = keys_.upper_bound(frame);
        if (it == keys_.begin()) return std::nullopt;
        auto next = it;
        --it;
        return Span{it->first, next == keys_.end() ? end_ : next->first, &it->second};
    }
    const Keyframe* keyAt(int frame) const { auto s = spanAt(frame); return s ? s->key : nullptr; }
    // Last key at or before `frame`, even past the end (used to extend a row).
    const Keyframe* keyAtOrBefore(int frame) const {
        auto it = keys_.upper_bound(frame);
        return it == keys_.begin() ? nullptr : &std::prev(it)->second;
    }
    Keyframe* mutableKey(int frame) { auto it = keys_.find(frame); return it == keys_.end() ? nullptr : &it->second; }
    bool isKey(int frame) const { return keys_.count(frame) != 0; }

    void setKey(int frame, Keyframe k) {
        keys_[frame] = std::move(k);
        if (frame + 1 > end_) end_ = frame + 1;
    }
    // "Clear keyframe": the previous span absorbs this one.
    bool removeKey(int frame) { return keys_.erase(frame) != 0; }

    // F5-style: insert `count` held frames at `frame`, pushing later keys right.
    void insertFrames(int frame, int count) {
        std::map<int, Keyframe> shifted;
        for (auto& [f, k] : keys_) shifted.emplace(f > frame ? f + count : f, std::move(k));
        keys_ = std::move(shifted);
        end_ = std::max(end_, frame) + count;
    }
    void removeFrames(int frame, int count) {
        std::map<int, Keyframe> shifted;
        for (auto& [f, k] : keys_) {
            if (f >= frame && f < frame + count) continue;  // keys inside the removed range
            shifted.emplace(f >= frame + count ? f - count : f, std::move(k));
        }
        keys_ = std::move(shifted);
        end_ = std::max(frame, end_ - count);
        // Drop keys that now start at or past the end.
        while (!keys_.empty() && keys_.rbegin()->first >= end_) keys_.erase(std::prev(keys_.end()));
    }
    void setLength(int len) { end_ = len; }
    int length() const { return end_; }
    const std::map<int, Keyframe>& keys() const { return keys_; }

private:
    std::map<int, Keyframe> keys_;
    int end_ = 0;
};

struct Model3DRef {  // placeholder for the Blender/glTF/USD milestone
    std::string assetUri;
    enum class Format { GLTF, USD } format = Format::GLTF;
    std::string animationClip;  // baked clip name inside the asset
    bool followSceneCamera = true;
};

struct Layer {
    LayerId id;
    std::string name;
    LayerKind kind = LayerKind::Vector;
    bool visible = true;
    bool locked = false;
    bool clipToBelow = false;      // clipping mask
    bool animationFolder = false;  // Folder whose keyframes choose a child cel (CSP)
    BlendMode blend = BlendMode::Normal;
    Color timelineTint{0.4f, 0.6f, 1.0f, 1.0f};

    TransformTracks transform;
    std::vector<Effect> effects;
    Exposure exposure;
    std::vector<std::unique_ptr<Layer>> children;  // Folder only; index 0 = bottom
    std::vector<AudioClip> audioClips;             // Audio only
    std::optional<Model3DRef> model;               // Model3D only

    Layer() = default;
    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;
    Layer(Layer&&) = default;
    Layer& operator=(Layer&&) = default;

    std::unique_ptr<Layer> clone() const;  // deep copy (same ids)
    bool isFolder() const { return kind == LayerKind::Folder; }
    int contentLength() const;             // max exposure end in this subtree
};

// A timeline is a root folder. The document and every symbol own one.
class Timeline {
public:
    Timeline();
    Layer& root() { return *root_; }
    const Layer& root() const { return *root_; }

    Layer* find(LayerId id);
    const Layer* find(LayerId id) const { return const_cast<Timeline*>(this)->find(id); }
    Layer* parentOf(LayerId id);

    // Adds at the top of `parent` (or root) and returns the new layer.
    Layer* addLayer(IdAllocator& ids, LayerKind kind, std::string name, LayerId parent = {}, int index = -1);
    std::unique_ptr<Layer> detach(LayerId id);
    bool attach(std::unique_ptr<Layer> layer, LayerId parent = {}, int index = -1);
    bool move(LayerId id, LayerId newParent, int index = -1);  // reparent (drag into folder)

    int length() const { return root_->contentLength(); }

private:
    std::unique_ptr<Layer> root_;
};

}  // namespace anim
