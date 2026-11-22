#include "anim/ai.h"

#include <algorithm>

#include "anim/brush.h"

namespace anim::ai {
namespace {

const VectorStroke* match(const VectorDrawing& d, const VectorStroke& s, size_t index) {
    if (s.object)
        for (const auto& o : d.strokes) if (o.object == s.object) return &o;
    return index < d.strokes.size() ? &d.strokes[index] : nullptr;
}

VectorDrawing interpolate(const VectorDrawing& a, const VectorDrawing& b, float t) {
    VectorDrawing out;
    for (size_t i = 0; i < a.strokes.size(); ++i) {
        const VectorStroke& sa = a.strokes[i];
        const VectorStroke* sb = match(b, sa, i);
        if (!sb) { if (t < 0.5f) out.strokes.push_back(sa); continue; }  // pops out halfway
        const int n = int(std::max({sa.points.size(), sb->points.size(), size_t(2)}));
        auto pa = brush::resample(sa.points, n), pb = brush::resample(sb->points, n);
        VectorStroke s = sa;
        s.width = lerpValue(sa.width, sb->width, t);
        s.points.resize(size_t(n));
        for (int k = 0; k < n; ++k)
            s.points[size_t(k)] = {lerpValue(pa[size_t(k)].pos, pb[size_t(k)].pos, t),
                                   lerpValue(pa[size_t(k)].pressure, pb[size_t(k)].pressure, t)};
        out.strokes.push_back(std::move(s));
    }
    return out;
}

}  // namespace

InbetweenResult GeometricInbetweenProvider::generate(const InbetweenPayload& p) {
    InbetweenResult r;
    const auto& keys = p.keys;
    for (int f : p.targetFrames) {
        // Surrounding pair of keys (start, guides, end).
        size_t hi = 1;
        while (hi < keys.size() - 1 && keys[hi].first < f) ++hi;
        const auto& k0 = keys[hi - 1];
        const auto& k1 = keys[hi];
        const float lin = float(f - k0.first) / float(k1.first - k0.first);
        r.frames.push_back({f, interpolate(*k0.second, *k1.second, p.request.timing.apply(lin))});
    }
    return r;
}

InbetweenResult InbetweenService::run(Document& doc, const InbetweenRequest& req, const std::string& providerId,
                                      edit::UndoStack* undo) {
    InbetweenResult fail;
    auto it = providers_.find(providerId);
    if (it == providers_.end()) { fail.error = "unknown provider"; return fail; }
    IInbetweenProvider& prov = *it->second;
    const std::string ent = prov.requiredEntitlement();
    if (!ent.empty() && !(entitlements_ && entitlements_->has(ent))) { fail.error = "requires subscription: " + ent; return fail; }

    Layer* layer = doc.findLayer(req.layer);
    if (!layer || layer->kind != LayerKind::Vector) { fail.error = "vector layer required"; return fail; }
    if (req.endFrame - req.startFrame < 2) { fail.error = "no frames between keys"; return fail; }

    InbetweenPayload payload{req, {}, {}};
    std::vector<int> keyFrames = req.guideFrames;
    keyFrames.push_back(req.startFrame);
    keyFrames.push_back(req.endFrame);
    std::sort(keyFrames.begin(), keyFrames.end());
    keyFrames.erase(std::unique(keyFrames.begin(), keyFrames.end()), keyFrames.end());
    for (int f : keyFrames) {
        const Keyframe* k = layer->exposure.isKey(f) ? layer->exposure.keyAt(f) : nullptr;
        const Drawing* d = k ? doc.drawings.get(k->drawing) : nullptr;
        if (!d || !d->vector()) { fail.error = "missing key drawing at frame " + std::to_string(f); return fail; }
        payload.keys.push_back({f, d->vector()});
    }
    for (int f = req.startFrame + req.step; f < req.endFrame; f += std::max(1, req.step))
        if (!std::binary_search(keyFrames.begin(), keyFrames.end(), f)) payload.targetFrames.push_back(f);

    InbetweenResult res = prov.generate(payload);
    if (!res.ok()) return res;

    edit::Transaction tx(doc, "AI in-between");
    tx.touchLayer(layer->id);
    for (auto& g : res.frames) {
        Keyframe k;
        k.drawing = doc.drawings.create(doc.ids, g.drawing);
        k.label = "inbetween";
        layer->exposure.setKey(g.frame, k);
    }
    if (undo) tx.commit(*undo);
    return res;
}

}  // namespace anim::ai
