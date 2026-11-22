#include "anim/tessellate.h"

#include <algorithm>
#include <cmath>

#include "anim/evaluate.h"

namespace anim::gfx {
namespace {

void tri(std::vector<Vertex>& out, Vec2 a, Vec2 b, Vec2 c, Color col) {
    out.push_back({a.x, a.y, col.r, col.g, col.b, col.a});
    out.push_back({b.x, b.y, col.r, col.g, col.b, col.a});
    out.push_back({c.x, c.y, col.r, col.g, col.b, col.a});
}
void quad(std::vector<Vertex>& out, Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color col) {
    tri(out, a, b, c, col);
    tri(out, a, c, d, col);
}
void disc(std::vector<Vertex>& out, Vec2 c, float r, Color col, int segs) {
    const int n = std::max(6, segs * 2);
    for (int i = 0; i < n; ++i) {
        const float a0 = float(2 * kPi * i / n), a1 = float(2 * kPi * (i + 1) / n);
        tri(out, c, {c.x + r * std::cos(a0), c.y + r * std::sin(a0)}, {c.x + r * std::cos(a1), c.y + r * std::sin(a1)}, col);
    }
}
float matScale(const Affine& m) { return std::sqrt(std::abs(m.determinant())); }

void line(std::vector<Vertex>& out, Vec2 a, Vec2 b, float w, Color col) {
    Vec2 d = b - a;
    const float len = d.length();
    if (len < 1e-6f) return;
    const Vec2 n{-d.y / len * w * 0.5f, d.x / len * w * 0.5f};
    quad(out, a + n, b + n, b - n, a - n, col);
}

struct Ctx {
    const Document& doc;
    const TessellateOptions& opt;
    std::vector<Vertex>& out;
};

void drawDrawing(const Ctx& ctx, const Drawing& d, const Affine& m, float opacity, const Color* tint) {
    const VectorDrawing* v = d.vector();
    if (!v) return;
    auto colorOf = [&](StyleId s) {
        Color c = tint ? *tint : ctx.doc.palette.resolve(s);
        c.a *= opacity;
        return c;
    };
    for (const auto& f : v->fills) {
        if (f.contour.size() < 3) continue;
        const Color c = colorOf(f.style);
        const Vec2 p0 = m.apply(f.contour[0]);
        for (size_t i = 1; i + 1 < f.contour.size(); ++i) tri(ctx.out, p0, m.apply(f.contour[i]), m.apply(f.contour[i + 1]), c);
    }
    for (const auto& s : v->strokes) tessellateStroke(s, m, colorOf(s.style), ctx.opt.roundCapSegments, ctx.out);
}

void drawList(const Ctx& ctx, const DisplayList& dl, const Affine& post, float baseOpacity, const Color* tint) {
    std::vector<float> stack{baseOpacity};
    for (const auto& c : dl.cmds) {
        switch (c.kind) {
            case RenderCmd::Kind::PushGroup: stack.push_back(stack.back() * c.opacity); break;
            case RenderCmd::Kind::PopGroup: if (stack.size() > 1) stack.pop_back(); break;
            case RenderCmd::Kind::DrawDrawing:
                if (const Drawing* d = ctx.doc.drawings.get(c.drawing)) drawDrawing(ctx, *d, post * c.transform, stack.back() * c.opacity, tint);
                break;
            case RenderCmd::Kind::DrawModel3D: break;  // rendered by the 3D pass (future)
        }
    }
}

}  // namespace

void tessellateStroke(const VectorStroke& s, const Affine& m, Color c, int capSegments, std::vector<Vertex>& out) {
    if (s.points.empty()) return;
    const float scale = matScale(m);
    auto radius = [&](const StrokePoint& p) { return std::max(0.35f, s.width * p.pressure * 0.5f * scale); };
    if (s.points.size() == 1) { disc(out, m.apply(s.points[0].pos), radius(s.points[0]), c, capSegments); return; }

    // Offset polyline with per-point width; joins are covered by small discs at
    // sharp corners so there are no cracks.
    std::vector<Vec2> pts;
    std::vector<float> rad;
    pts.reserve(s.points.size());
    for (const auto& p : s.points) {
        const Vec2 q = m.apply(p.pos);
        if (!pts.empty() && distance(pts.back(), q) < 0.05f) continue;
        pts.push_back(q);
        rad.push_back(radius(p));
    }
    if (pts.size() == 1) { disc(out, pts[0], rad[0], c, capSegments); return; }
    auto normalAt = [&](size_t i) {
        Vec2 d = i + 1 < pts.size() ? pts[i + 1] - pts[i] : pts[i] - pts[i - 1];
        if (i > 0 && i + 1 < pts.size()) d = pts[i + 1] - pts[i - 1];
        const float len = std::max(1e-6f, d.length());
        return Vec2{-d.y / len, d.x / len};
    };
    Vec2 prevL = pts[0] + normalAt(0) * rad[0], prevR = pts[0] - normalAt(0) * rad[0];
    for (size_t i = 1; i < pts.size(); ++i) {
        const Vec2 n = normalAt(i);
        const Vec2 L = pts[i] + n * rad[i], R = pts[i] - n * rad[i];
        quad(out, prevL, L, R, prevR, c);
        prevL = L; prevR = R;
        if (i + 1 < pts.size()) {
            const Vec2 d0 = pts[i] - pts[i - 1], d1 = pts[i + 1] - pts[i];
            const float cosA = dot(d0, d1) / std::max(1e-6f, d0.length() * d1.length());
            if (cosA < 0.7f) disc(out, pts[i], rad[i], c, capSegments);
        }
    }
    disc(out, pts.front(), rad.front(), c, capSegments);
    disc(out, pts.back(), rad.back(), c, capSegments);
}

void tessellateFrame(const Document& doc, int frame, const TessellateOptions& opt, std::vector<Vertex>& out) {
    out.clear();
    Ctx ctx{doc, opt, out};
    Affine post = opt.canvasToScreen;
    if (opt.throughCamera) {
        if (const Camera* cam = doc.activeCamera(frame)) post = opt.canvasToScreen * cam->viewMatrix(frame, doc.movie);
    }
    EvalOptions eo;
    eo.applyCamera = false;

    if (opt.drawCanvas) {
        const float w = float(doc.canvas.width), h = float(doc.canvas.height);
        if (opt.throughCamera) {
            const float mw = float(doc.movie.width), mh = float(doc.movie.height);
            quad(out, opt.canvasToScreen.apply({0, 0}), opt.canvasToScreen.apply({mw, 0}), opt.canvasToScreen.apply({mw, mh}),
                 opt.canvasToScreen.apply({0, mh}), doc.canvas.background);
        } else {
            quad(out, post.apply({0, 0}), post.apply({w, 0}), post.apply({w, h}), post.apply({0, h}), doc.canvas.background);
        }
    }

    // Onion skin: furthest first so nearer frames sit on top.
    const OnionSkin& on = opt.onion;
    auto onion = [&](int f, int dist, int count, const Color& tint) {
        if (f < 0) return;
        const float fade = on.opacity * (1.0f - float(dist - 1) / float(std::max(1, count)));
        drawList(ctx, evaluateFrame(doc, f, eo), post, fade, &tint);
    };
    for (int i = on.before; i >= 1; --i) onion(frame - i, i, on.before, on.beforeTint);
    for (int i = on.after; i >= 1; --i) onion(frame + i, i, on.after, on.afterTint);

    drawList(ctx, evaluateFrame(doc, frame, eo), post, 1.0f, nullptr);

    if (opt.drawCameraFrame && !opt.throughCamera) {
        if (const Camera* cam = doc.activeCamera(frame)) {
            auto c = cam->frameCorners(frame, doc.movie);
            const Color col{0.1f, 0.45f, 1.0f, 0.9f};
            for (int i = 0; i < 4; ++i) line(out, post.apply(c[size_t(i)]), post.apply(c[size_t((i + 1) % 4)]), 2.0f, col);
        }
    }
}

}  // namespace anim::gfx
