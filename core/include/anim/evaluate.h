// Scene evaluation: resolves a document at a frame into a flat, backend-neutral
// display list (groups, drawings, effects, camera). Renderers (Metal, D3D12,
// a CPU exporter, a thumbnail generator) only ever consume this list, which is
// what keeps output identical across iPad, Mac and Windows.
#pragma once
#include <vector>

#include "anim/document.h"

namespace anim {

struct RenderCmd {
    enum class Kind { PushGroup, PopGroup, DrawDrawing, DrawModel3D } kind;
    LayerId layer;
    DrawingId drawing;            // DrawDrawing
    Affine transform;             // local -> output (or canvas if no camera)
    float opacity = 1;
    BlendMode blend = BlendMode::Normal;
    bool clipToBelow = false;
    std::vector<EffectParams> effects;  // PushGroup: applied when the group is composited
};

struct DisplayList {
    std::vector<RenderCmd> cmds;
    Affine view;  // camera matrix used
    int drawCount() const {
        int n = 0;
        for (auto& c : cmds) n += c.kind == RenderCmd::Kind::DrawDrawing;
        return n;
    }
};

struct EvalOptions {
    bool applyCamera = true;     // false = canvas space (editing view)
    bool includeHidden = false;
    int maxSymbolDepth = 32;     // guards against recursive symbols
};

DisplayList evaluateFrame(const Document& doc, int frame, const EvalOptions& opt = {});

}  // namespace anim
