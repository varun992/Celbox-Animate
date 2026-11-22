// Turns an evaluated frame into colored triangles for the GPU. Shared by the
// Metal (iPad/Mac) and D3D (Windows) renderers so strokes look identical.
//
// M1 scope: vector strokes (pressure-varying width, round caps), simple fills
// (fan triangulation, convex/star-shaped regions), canvas background, camera
// frame and onion skin. Raster layers are drawn by the backend from tiles (M2).
#pragma once
#include <vector>

#include "anim/document.h"

namespace anim::gfx {

struct Vertex {
    float x, y;        // screen pixels
    float r, g, b, a;  // straight alpha
};

struct OnionSkin {
    int before = 0, after = 0;
    Color beforeTint{1.0f, 0.25f, 0.25f, 1.0f};
    Color afterTint{0.25f, 0.6f, 1.0f, 1.0f};
    float opacity = 0.35f;  // nearest onion frame; further frames fade
};

struct TessellateOptions {
    Affine canvasToScreen;     // editor pan/zoom/rotate
    bool drawCanvas = true;
    bool drawCameraFrame = true;
    bool throughCamera = false;  // true: view what the camera sees (playback/export preview)
    OnionSkin onion;
    int roundCapSegments = 8;
};

void tessellateFrame(const Document& doc, int frame, const TessellateOptions& opt, std::vector<Vertex>& out);

// Single stroke (used for the live, not-yet-committed stroke while drawing).
void tessellateStroke(const VectorStroke& s, const Affine& m, Color c, int capSegments, std::vector<Vertex>& out);

}  // namespace anim::gfx
