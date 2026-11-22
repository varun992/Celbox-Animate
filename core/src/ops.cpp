#include "anim/ops.h"

#include <algorithm>

namespace anim::ops {
namespace {
bool isDrawingLayer(const Layer& l) { return l.kind == LayerKind::Vector || l.kind == LayerKind::Raster; }

DrawingId newEmptyDrawing(Document& doc, const Layer& l) {
    if (l.kind == LayerKind::Raster) return doc.drawings.create(doc.ids, RasterDrawing(doc.canvas.width, doc.canvas.height));
    return doc.drawings.create(doc.ids, VectorDrawing{});
}

void shiftLayerAnimation(Layer& l, int fromFrame, int delta) {
    l.transform.shiftKeys(fromFrame, delta);
    for (auto& e : l.effects) e.shiftKeys(fromFrame, delta);
}
}  // namespace

DrawingId insertKeyframe(Document& doc, Layer& layer, int frame) {
    Keyframe k;
    if (const Keyframe* cur = layer.exposure.keyAtOrBefore(frame)) {
        k = *cur;
        k.label.clear();
        if (cur->drawing) k.drawing = doc.drawings.clone(doc.ids, cur->drawing);
    } else if (isDrawingLayer(layer)) {
        k.drawing = newEmptyDrawing(doc, layer);
    }
    layer.exposure.setKey(frame, k);
    return k.drawing;
}

DrawingId insertBlankKeyframe(Document& doc, Layer& layer, int frame) {
    Keyframe k;
    if (isDrawingLayer(layer)) k.drawing = newEmptyDrawing(doc, layer);
    layer.exposure.setKey(frame, k);
    return k.drawing;
}

void insertFrames(Layer& layer, int frame, int count) {
    layer.exposure.insertFrames(frame, count);
    shiftLayerAnimation(layer, frame + 1, count);
}

void removeFrames(Layer& layer, int frame, int count) {
    layer.exposure.removeFrames(frame, count);
    shiftLayerAnimation(layer, frame + count, -count);
}

DrawingId duplicateSpan(Document& doc, Layer& layer, int frame) {
    auto span = layer.exposure.spanAt(frame);
    if (!span) return {};
    Keyframe copy = *span->key;
    if (copy.drawing) copy.drawing = doc.drawings.clone(doc.ids, copy.drawing);
    const int len = span->end - span->start;
    layer.exposure.insertFrames(span->end - 1, len);  // make room after the span
    layer.exposure.setKey(span->end, copy);
    shiftLayerAnimation(layer, span->end, len);
    return copy.drawing;
}

bool linkedDuplicate(Layer& layer, int srcFrame, int dstFrame) {
    const Keyframe* src = layer.exposure.keyAt(srcFrame);
    if (!src) return false;
    Keyframe k = *src;
    layer.exposure.setKey(dstFrame, k);
    return true;
}

DrawingId makeUnique(Document& doc, Layer& layer, int frame) {
    auto span = layer.exposure.spanAt(frame);
    if (!span || !span->key->drawing) return {};
    Keyframe* k = layer.exposure.mutableKey(span->start);
    k->drawing = doc.drawings.clone(doc.ids, k->drawing);
    return k->drawing;
}

void setPoseKey(Layer& layer, int frame, const Pose& p, Easing ease) {
    auto& t = layer.transform;
    t.position.setKey(frame, p.position, ease);
    t.rotationDeg.setKey(frame, p.rotationDeg, ease);
    t.scale.setKey(frame, p.scale, ease);
    t.skewDeg.setKey(frame, p.skewDeg, ease);
    t.opacity.setKey(frame, p.opacity, ease);
}

Pose poseAt(const Layer& layer, double frame) {
    const auto& t = layer.transform;
    return {t.position.valueAt(frame), t.rotationDeg.valueAt(frame), t.scale.valueAt(frame),
            t.skewDeg.valueAt(frame), t.opacity.valueAt(frame)};
}

void createMotionTween(Layer& layer, int startFrame, int endFrame, Easing ease) {
    const Pose start = poseAt(layer, startFrame), end = poseAt(layer, endFrame);
    setPoseKey(layer, startFrame, start, ease);
    setPoseKey(layer, endFrame, end, ease);
    if (layer.exposure.length() < endFrame + 1) layer.exposure.setLength(endFrame + 1);
}

int symbolFrame(const SymbolDef& def, const SymbolInstance& inst, int framesIntoSpan, int clockFrames) {
    const int len = def.length();
    const int n = inst.firstFrame + (def.kind == SymbolKind::MovieClip ? clockFrames : framesIntoSpan);
    switch (inst.loop) {
        case LoopMode::SingleFrame: return std::clamp(inst.firstFrame, 0, len - 1);
        case LoopMode::PlayOnce: return std::clamp(n, 0, len - 1);
        case LoopMode::PlayOnceReverse: return std::clamp(len - 1 - n, 0, len - 1);
        case LoopMode::Loop: return ((n % len) + len) % len;
    }
    return 0;
}

bool applyLipSync(Document& doc, const LipSyncTrack& track) {
    Layer* layer = doc.findLayer(track.targetLayer);
    if (!layer || layer->kind != LayerKind::Symbol) return false;
    SymbolId sym;
    for (auto& [_, k] : layer->exposure.keys()) if (k.instance) { sym = k.instance->symbol; break; }
    if (!sym) return false;
    for (const auto& pk : track.keys) {
        Keyframe k;
        k.instance = SymbolInstance{sym, LoopMode::SingleFrame, track.chart.symbolFrame[size_t(pk.phoneme)]};
        k.label = "lipsync";
        layer->exposure.setKey(pk.frame, k);
    }
    return true;
}

}  // namespace anim::ops
