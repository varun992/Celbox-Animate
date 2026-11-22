// Document = canvas + movie settings + palette + drawings + symbols + root
// timeline + cameras + audio. This is the single source of truth that every
// platform shell (iPad, Mac, Windows) renders and edits.
#pragma once
#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "anim/audio.h"
#include "anim/drawing.h"
#include "anim/layer.h"
#include "anim/palette.h"

namespace anim {

// The drawing surface. It can be larger than the movie frame so the camera
// can pan across a wide background (requirement #6).
struct CanvasSettings {
    int width = 3840, height = 2160;
    Color background = Color::white();
    float dpi = 144;
};

// What gets exported: resolution, frame rate, duration.
struct MovieSettings {
    int width = 1920, height = 1080;
    double fps = 24.0;
    int frameCount = 0;  // 0 = follow timeline length
    float pixelAspect = 1.0f;
};

enum class SymbolKind {
    Graphic,    // frames follow the parent timeline (scrubbable, deterministic)
    MovieClip,  // plays on its own clock from when it appears
};

struct SymbolDef {
    SymbolId id;
    std::string name;
    SymbolKind kind = SymbolKind::Graphic;
    Timeline timeline;
    Vec2 registration{0, 0};
    int length() const { return std::max(1, timeline.length()); }
};

struct Camera {
    CameraId id;
    std::string name = "Camera";
    AnimTrack<Vec2> center;          // in canvas pixels
    AnimTrack<float> zoom{1.0f};     // output px per canvas px
    AnimTrack<float> rotationDeg{0.0f};

    // Canvas space -> output (movie) pixel space at `frame`.
    Affine viewMatrix(double frame, const MovieSettings& m) const {
        return Affine::translate({m.width * 0.5f, m.height * 0.5f}) * Affine::scale({zoom.valueAt(frame), zoom.valueAt(frame)}) *
               Affine::rotateDeg(-rotationDeg.valueAt(frame)) * Affine::translate(center.valueAt(frame) * -1.0f);
    }
    // The camera frame drawn on the canvas (for the camera tool overlay).
    std::array<Vec2, 4> frameCorners(double frame, const MovieSettings& m) const {
        const Affine inv = viewMatrix(frame, m).inverse();
        const float w = float(m.width), h = float(m.height);
        return {inv.apply({0, 0}), inv.apply({w, 0}), inv.apply({w, h}), inv.apply({0, h})};
    }
};

class Document {
public:
    Document();

    IdAllocator ids;
    CanvasSettings canvas;
    MovieSettings movie;
    Palette palette;
    DrawingStore drawings;
    std::unordered_map<SymbolId, SymbolDef> symbols;
    Timeline timeline;
    std::deque<Camera> cameras;  // deque: references stay valid when adding cameras
    std::map<int, CameraId> cameraCuts;  // frame -> active camera from that frame on
    std::vector<AudioAsset> audioAssets;
    std::vector<LipSyncTrack> lipSync;

    Layer* addLayer(LayerKind kind, std::string name, LayerId parent = {}) {
        return timeline.addLayer(ids, kind, std::move(name), parent);
    }
    SymbolDef& createSymbol(std::string name, SymbolKind kind);
    SymbolDef* symbol(SymbolId id) { auto it = symbols.find(id); return it == symbols.end() ? nullptr : &it->second; }
    const SymbolDef* symbol(SymbolId id) const { return const_cast<Document*>(this)->symbol(id); }

    // Finds a layer in the root timeline or in any symbol timeline.
    Layer* findLayer(LayerId id);
    Timeline* timelineOwning(LayerId id);

    // Animate's "Convert to Symbol": moves the layer into a new symbol and
    // replaces it with a Symbol layer showing an instance of it.
    SymbolId convertToSymbol(LayerId layer, std::string name, SymbolKind kind);

    const Camera* activeCamera(int frame) const;
    Camera& addCamera(std::string name);

    int frameCount() const { return movie.frameCount > 0 ? movie.frameCount : std::max(1, timeline.length()); }
};

}  // namespace anim
