#include "anim/edit.h"

#include <algorithm>
#include <unordered_set>

namespace anim::edit {

void UndoStack::pushApplied(std::unique_ptr<Command> c) {
    undo_.push_back(std::move(c));
    redo_.clear();
    if (undo_.size() > limit_) undo_.erase(undo_.begin());
}
bool UndoStack::undo(Document& doc) {
    if (undo_.empty()) return false;
    auto c = std::move(undo_.back());
    undo_.pop_back();
    c->revert(doc);
    redo_.push_back(std::move(c));
    return true;
}
bool UndoStack::redo(Document& doc) {
    if (redo_.empty()) return false;
    auto c = std::move(redo_.back());
    redo_.pop_back();
    c->apply(doc);
    undo_.push_back(std::move(c));
    return true;
}

// Stores before/after and swaps between them.
class SnapshotCommand final : public Command {
public:
    using LayerState = Transaction::LayerState;
    std::string name_;
    std::unordered_map<LayerId, LayerState> layersBefore, layersAfter;
    std::unordered_map<DrawingId, std::optional<Drawing>> drawingsBefore, drawingsAfter;
    std::optional<Palette> paletteBefore, paletteAfter;

    const std::string& name() const override { return name_; }
    void apply(Document& doc) override { restore(doc, layersAfter, drawingsAfter, paletteAfter); }
    void revert(Document& doc) override { restore(doc, layersBefore, drawingsBefore, paletteBefore); }

private:
    static void restore(Document& doc, const std::unordered_map<LayerId, LayerState>& layers,
                        const std::unordered_map<DrawingId, std::optional<Drawing>>& drawings,
                        const std::optional<Palette>& pal) {
        for (const auto& [id, st] : layers)
            if (Layer* l = doc.findLayer(id)) { l->exposure = st.exposure; l->transform = st.transform; l->effects = st.effects; }
        for (const auto& [id, d] : drawings) {
            if (d) doc.drawings.put(*d); else doc.drawings.erase(id);
        }
        if (pal) doc.palette = *pal;
    }
};

void Transaction::start() {
    if (started_) return;
    started_ = true;
    idWatermark_ = doc_.ids.peek();
}

void Transaction::touchLayer(LayerId id) {
    start();
    if (layersBefore_.count(id)) return;
    if (Layer* l = doc_.findLayer(id)) layersBefore_[id] = {l->exposure, l->transform, l->effects};
}
void Transaction::touchDrawing(DrawingId id) {
    start();
    if (drawingsBefore_.count(id)) return;
    const Drawing* d = doc_.drawings.get(id);
    drawingsBefore_[id] = d ? std::optional<Drawing>(*d) : std::nullopt;
}
void Transaction::touchPalette() {
    start();
    if (!paletteBefore_) paletteBefore_ = doc_.palette;
}

void Transaction::commit(UndoStack& stack) {
    auto cmd = std::make_unique<SnapshotCommand>();
    cmd->name_ = name_;
    cmd->layersBefore = layersBefore_;
    for (const auto& [id, _] : layersBefore_)
        if (Layer* l = doc_.findLayer(id)) cmd->layersAfter[id] = {l->exposure, l->transform, l->effects};
    // Any drawing created during the transaction (id >= watermark) referenced by
    // a touched layer is recorded as "absent before".
    for (const auto& [id, st] : cmd->layersAfter)
        for (const auto& [_, k] : st.exposure.keys())
            if (k.drawing && k.drawing.value >= idWatermark_ && !drawingsBefore_.count(k.drawing))
                drawingsBefore_[k.drawing] = std::nullopt;
    cmd->drawingsBefore = drawingsBefore_;
    for (const auto& [id, _] : drawingsBefore_) {
        const Drawing* d = doc_.drawings.get(id);
        cmd->drawingsAfter[id] = d ? std::optional<Drawing>(*d) : std::nullopt;
    }
    if (paletteBefore_) { cmd->paletteBefore = paletteBefore_; cmd->paletteAfter = doc_.palette; }
    stack.pushApplied(std::move(cmd));
}

std::vector<DrawingId> drawingsIn(const Document& doc, const FrameSelection& sel) {
    std::vector<DrawingId> out;
    std::unordered_set<DrawingId> seen;
    for (LayerId lid : sel.layers) {
        const Layer* l = const_cast<Document&>(doc).findLayer(lid);
        if (!l) continue;
        for (int f = sel.firstFrame; f <= sel.lastFrame; ++f)
            if (auto* k = l->exposure.keyAt(f); k && k->drawing && seen.insert(k->drawing).second) out.push_back(k->drawing);
    }
    return out;
}

namespace {
template <class Fn>
void forEachVector(Document& doc, const FrameSelection& sel, const char* name, UndoStack* undo, Fn fn) {
    Transaction tx(doc, name);
    for (DrawingId id : drawingsIn(doc, sel)) {
        Drawing* d = doc.drawings.get(id);
        if (!d || !d->vector()) continue;
        if (undo) tx.touchDrawing(id);
        fn(*d->vector());
    }
    if (undo) tx.commit(*undo);
}
}  // namespace

void transformFrames(Document& doc, const FrameSelection& sel, const Affine& m, UndoStack* undo) {
    forEachVector(doc, sel, "Multi-frame transform", undo, [&](VectorDrawing& v) { v.transform(m); });
}

bool pointInPolygon(Vec2 p, const std::vector<Vec2>& poly) {
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const Vec2 a = poly[i], b = poly[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
    }
    return inside;
}

std::vector<size_t> lassoStrokes(const VectorDrawing& d, const std::vector<Vec2>& poly, LassoMode mode) {
    std::vector<size_t> out;
    if (poly.size() < 3) return out;
    for (size_t i = 0; i < d.strokes.size(); ++i) {
        const auto& pts = d.strokes[i].points;
        auto in = [&](const StrokePoint& p) { return pointInPolygon(p.pos, poly); };
        const bool hit = mode == LassoMode::AnyPointInside ? std::any_of(pts.begin(), pts.end(), in)
                                                           : !pts.empty() && std::all_of(pts.begin(), pts.end(), in);
        if (hit) out.push_back(i);
    }
    return out;
}

void lassoTransformFrames(Document& doc, const FrameSelection& sel, const std::vector<Vec2>& poly, const Affine& m,
                          LassoMode mode, UndoStack* undo) {
    forEachVector(doc, sel, "Multi-frame lasso transform", undo, [&](VectorDrawing& v) {
        for (size_t i : lassoStrokes(v, poly, mode))
            for (auto& p : v.strokes[i].points) p.pos = m.apply(p.pos);
    });
}

int restyleObject(Document& doc, const FrameSelection& sel, ObjectId obj, StyleId style, UndoStack* undo) {
    int changed = 0;
    forEachVector(doc, sel, "Restyle object", undo, [&](VectorDrawing& v) {
        for (auto& s : v.strokes) if (s.object == obj) { s.style = style; ++changed; }
        for (auto& f : v.fills) if (f.object == obj) { f.style = style; ++changed; }
    });
    return changed;
}

}  // namespace anim::edit
