#include "anim/evaluate.h"

#include "anim/ops.h"

namespace anim {
namespace {

struct Ctx {
    int localFrame;   // frame within the timeline being evaluated
    int globalFrame;  // document frame being rendered
    int entryGlobal;  // document frame at which this timeline started showing
    Affine xform;
    int depth;
};

class Evaluator {
public:
    Evaluator(const Document& d, const EvalOptions& o, DisplayList& out) : doc_(d), opt_(o), out_(out) {}

    void layer(const Layer& l, const Ctx& ctx) {
        if (!l.visible && !opt_.includeHidden) return;
        const double f = ctx.localFrame;
        const float opacity = l.transform.opacity.valueAt(f);
        const Affine m = ctx.xform * l.transform.matrixAt(f);

        const bool group = l.isFolder() || !l.effects.empty() || l.blend != BlendMode::Normal || opacity < 1.0f;
        if (group) {
            RenderCmd push{RenderCmd::Kind::PushGroup, l.id};
            push.opacity = opacity; push.blend = l.blend; push.clipToBelow = l.clipToBelow;
            for (const auto& e : l.effects) if (e.enabled) push.effects.push_back(e.evaluate(f));
            out_.cmds.push_back(std::move(push));
        }
        const float drawOpacity = group ? 1.0f : opacity;
        Ctx child{ctx.localFrame, ctx.globalFrame, ctx.entryGlobal, m, ctx.depth};

        switch (l.kind) {
            case LayerKind::Folder:
                if (l.animationFolder) {
                    if (auto* k = l.exposure.keyAt(ctx.localFrame); k && k->cel)
                        for (const auto& c : l.children)
                            if (c->id == k->cel) {
                                // Cels are usually single drawings: hold their last frame.
                                Ctx cel = child;
                                cel.localFrame = std::min(cel.localFrame, std::max(0, c->contentLength() - 1));
                                layer(*c, cel);
                            }
                } else {
                    for (const auto& c : l.children) layer(*c, child);
                }
                break;
            case LayerKind::Vector:
            case LayerKind::Raster:
                if (auto* k = l.exposure.keyAt(ctx.localFrame); k && k->drawing) {
                    RenderCmd d{RenderCmd::Kind::DrawDrawing, l.id, k->drawing, m, drawOpacity};
                    d.clipToBelow = l.clipToBelow && !group;
                    out_.cmds.push_back(d);
                }
                break;
            case LayerKind::Symbol:
                symbolLayer(l, child);
                break;
            case LayerKind::Model3D:
                if (l.model) out_.cmds.push_back({RenderCmd::Kind::DrawModel3D, l.id, {}, m, drawOpacity});
                break;
            case LayerKind::Audio:
                break;
        }
        if (group) out_.cmds.push_back({RenderCmd::Kind::PopGroup, l.id});
    }

private:
    void symbolLayer(const Layer& l, const Ctx& ctx) {
        if (ctx.depth >= opt_.maxSymbolDepth) return;
        auto span = l.exposure.spanAt(ctx.localFrame);
        if (!span || !span->key->instance) return;
        const SymbolInstance& inst = *span->key->instance;
        const SymbolDef* def = doc_.symbol(inst.symbol);
        if (!def) return;
        const int into = ctx.localFrame - span->start;
        // A movie clip keeps its own clock: frames since this span began on the
        // document clock, even when the parent timeline is holding still.
        const int entry = ctx.entryGlobal + span->start;
        const int clock = std::max(0, ctx.globalFrame - entry);
        const int sf = ops::symbolFrame(*def, inst, into, clock);
        Ctx inner{sf, ctx.globalFrame, entry, ctx.xform * Affine::translate(def->registration * -1.0f), ctx.depth + 1};
        for (const auto& c : def->timeline.root().children) layer(*c, inner);
    }

    const Document& doc_;
    const EvalOptions& opt_;
    DisplayList& out_;
};

}  // namespace

DisplayList evaluateFrame(const Document& doc, int frame, const EvalOptions& opt) {
    DisplayList out;
    if (opt.applyCamera) {
        if (const Camera* cam = doc.activeCamera(frame)) out.view = cam->viewMatrix(frame, doc.movie);
    }
    Evaluator ev(doc, opt, out);
    Ctx root{frame, frame, 0, out.view, 0};
    for (const auto& c : doc.timeline.root().children) ev.layer(*c, root);
    return out;
}

}  // namespace anim
