#include "anim/session.h"

#include <algorithm>
#include <memory>

#include "anim/ops.h"

namespace anim {

namespace {
// Undo for structural edits: snapshots the layer tree (drawings are shared by
// id and stay in the store, so this is cheap) plus canvas/movie settings.
class StructureCommand final : public edit::Command {
public:
    std::string label;
    std::unique_ptr<Layer> before, after;
    CanvasSettings canvasBefore, canvasAfter;
    MovieSettings movieBefore, movieAfter;
    const std::string& name() const override { return label; }
    void apply(Document& d) override { d.timeline.replaceRoot(after->clone()); d.canvas = canvasAfter; d.movie = movieAfter; }
    void revert(Document& d) override { d.timeline.replaceRoot(before->clone()); d.canvas = canvasBefore; d.movie = movieBefore; }
};
}  // namespace

template <class Fn>
bool Session::structureEdit(const char* name, Fn fn) {
    auto cmd = std::make_unique<StructureCommand>();
    cmd->label = name;
    cmd->before = doc.timeline.root().clone();
    cmd->canvasBefore = doc.canvas;
    cmd->movieBefore = doc.movie;
    if (!fn()) return false;
    cmd->after = doc.timeline.root().clone();
    cmd->canvasAfter = doc.canvas;
    cmd->movieAfter = doc.movie;
    undo.pushApplied(std::move(cmd));
    ++revision_;
    return true;
}

void Session::validateSelection() {
    Layer* l = doc.timeline.find(currentLayer);
    if (currentLayer && l) return;
    auto r = rows();
    currentLayer = {};
    for (const auto& row : r) if (!row.isFolder) { currentLayer = row.id; break; }
    if (!currentLayer && !r.empty()) currentLayer = r.front().id;
}

bool Session::setSettings(const CanvasSettings& canvas, const MovieSettings& movie) {
    return structureEdit("Change settings", [&] { doc.canvas = canvas; doc.movie = movie; return true; });
}

Session::Session() {
    currentStyle = doc.palette.add(doc.ids, "Ink", Color::black());
    doc.palette.add(doc.ids, "Red", {0.90f, 0.22f, 0.20f, 1});
    doc.palette.add(doc.ids, "Blue", {0.16f, 0.42f, 0.90f, 1});
    doc.palette.add(doc.ids, "Skin", {0.98f, 0.82f, 0.68f, 1});
    Layer* l = doc.addLayer(LayerKind::Vector, "Layer 1");
    ops::insertBlankKeyframe(doc, *l, 0);
    currentLayer = l->id;
}

// ---------------- layers ----------------

namespace {
void collectRows(const Layer& l, int depth, std::vector<LayerRow>& out) {
    // children[0] is bottom-most; the timeline lists top-most first.
    for (auto it = l.children.rbegin(); it != l.children.rend(); ++it) {
        const Layer& c = **it;
        out.push_back({c.id, depth, c.kind, c.visible, c.locked, c.isFolder(), c.name});
        if (c.isFolder()) collectRows(c, depth + 1, out);
    }
}
}  // namespace

std::vector<LayerRow> Session::rows() const {
    std::vector<LayerRow> out;
    collectRows(doc.timeline.root(), 0, out);
    return out;
}

LayerId Session::addLayer(LayerKind kind, std::string name) {
    // New layers go above the current one, inside its folder (or inside the
    // current layer if it is a folder).
    LayerId parent;
    int index = -1;
    if (Layer* cur = doc.timeline.find(currentLayer); cur && currentLayer) {
        if (cur->isFolder()) {
            parent = cur->id;
        } else if (Layer* p = doc.timeline.parentOf(currentLayer)) {
            parent = p == &doc.timeline.root() ? LayerId{} : p->id;
            for (int i = 0; i < int(p->children.size()); ++i) if (p->children[size_t(i)]->id == currentLayer) index = i + 1;
        }
    }
    if (name.empty()) {
        const char* base = kind == LayerKind::Folder ? "Folder " : kind == LayerKind::Raster ? "Paint " : "Layer ";
        name = base + std::to_string(rows().size() + 1);
    }
    LayerId added;
    structureEdit("Add layer", [&] {
        Layer* l = doc.timeline.addLayer(doc.ids, kind, std::move(name), parent, index);
        if (!l) return false;
        if (kind == LayerKind::Vector || kind == LayerKind::Raster) ops::insertBlankKeyframe(doc, *l, 0);
        added = l->id;
        return true;
    });
    if (added) currentLayer = added;
    return added;
}

bool Session::deleteLayer(LayerId id) {
    if (rows().size() <= 1) return false;  // keep at least one layer
    const bool ok = structureEdit("Delete layer", [&] { return doc.timeline.detach(id) != nullptr; });
    validateSelection();
    return ok;
}

bool Session::renameLayer(LayerId id, std::string name) {
    Layer* l = doc.timeline.find(id);
    if (!l || !id) return false;
    return structureEdit("Rename layer", [&] { l->name = std::move(name); return true; });
}
bool Session::setVisible(LayerId id, bool v) {
    Layer* l = doc.timeline.find(id);
    if (!l || !id) return false;
    return structureEdit("Show/hide layer", [&] { l->visible = v; return true; });
}
bool Session::setLocked(LayerId id, bool v) {
    Layer* l = doc.timeline.find(id);
    if (!l || !id) return false;
    return structureEdit("Lock layer", [&] { l->locked = v; return true; });
}
bool Session::moveLayer(LayerId id, int delta) {
    Layer* p = doc.timeline.parentOf(id);
    if (!p) return false;
    auto& ch = p->children;
    auto it = std::find_if(ch.begin(), ch.end(), [&](auto& c) { return c->id == id; });
    const int i = int(it - ch.begin()), j = i + delta;
    if (j < 0 || j >= int(ch.size())) return false;
    return structureEdit("Move layer", [&] { std::swap(ch[size_t(i)], ch[size_t(j)]); return true; });
}

// ---------------- frames ----------------

CellState Session::cell(LayerId layerId, int frame) const {
    const Layer* l = doc.timeline.find(layerId);
    if (!l || !layerId) return CellState::Empty;
    auto span = l->exposure.spanAt(frame);
    if (!span) return CellState::Empty;
    bool hasContent = !span->key->blank();
    if (span->key->drawing) {
        const Drawing* d = doc.drawings.get(span->key->drawing);
        hasContent = d && (!d->vector() || !d->vector()->strokes.empty() || !d->vector()->fills.empty());
    }
    const bool isKey = span->start == frame;
    if (hasContent) return isKey ? CellState::Key : CellState::Hold;
    return isKey ? CellState::BlankKey : CellState::BlankHold;
}

void Session::rowStates(LayerId layer, int count, uint8_t* out) const {
    for (int f = 0; f < count; ++f) out[f] = uint8_t(cell(layer, f));
}

bool Session::editableLayer(Layer*& l) const {
    l = const_cast<Document&>(doc).timeline.find(currentLayer);
    return l && currentLayer && !l->locked && !l->isFolder();
}

template <class Fn>
bool Session::frameEdit(const char* name, Fn fn) {
    Layer* l = nullptr;
    if (!editableLayer(l)) return false;
    edit::Transaction tx(doc, name);
    tx.touchLayer(l->id);
    if (!fn(*l)) return false;
    tx.commit(undo);
    ++revision_;
    return true;
}

bool Session::insertFrame() {
    return frameEdit("Insert frame", [&](Layer& l) {
        if (currentFrame >= l.exposure.length()) {
            // Extending past the end holds the last key up to the playhead.
            if (l.exposure.keys().empty()) return false;
            l.exposure.setLength(currentFrame + 1);
        } else {
            ops::insertFrames(l, currentFrame, 1);
        }
        return true;
    });
}
bool Session::insertKeyframe() {
    return frameEdit("Insert keyframe", [&](Layer& l) { ops::insertKeyframe(doc, l, currentFrame); return true; });
}
bool Session::insertBlankKeyframe() {
    return frameEdit("Insert blank keyframe", [&](Layer& l) { ops::insertBlankKeyframe(doc, l, currentFrame); return true; });
}
bool Session::duplicateFrame() {
    return frameEdit("Duplicate frame", [&](Layer& l) {
        auto span = l.exposure.spanAt(currentFrame);
        if (!span) return false;
        const int len = span->end - span->start;
        ops::duplicateSpan(doc, l, currentFrame);
        currentFrame = span->start + len;  // jump to the copy
        return true;
    });
}
bool Session::removeFrame() {
    return frameEdit("Remove frame", [&](Layer& l) {
        if (currentFrame >= l.exposure.length()) return false;
        ops::removeFrames(l, currentFrame, 1);
        return true;
    });
}
bool Session::clearKeyframe() {
    return frameEdit("Clear keyframe", [&](Layer& l) { return currentFrame > 0 && l.exposure.removeKey(currentFrame); });
}

// ---------------- drawing ----------------

void Session::strokeBegin(Vec2 p, float pressure, double t) {
    builder_ = brush::StrokeBuilder(stabilizer);
    builder_.begin({p, pressure, 0, t});
    stroking_ = true;
}
void Session::strokeAdd(Vec2 p, float pressure, double t) {
    if (stroking_) builder_.add({p, pressure, 0, t});
}
void Session::strokeCancel() { stroking_ = false; }

void Session::eraseAlong(const std::vector<StrokePoint>& path) {
    // Stroke eraser: removes whole strokes touched by the eraser (vector "erase
    // to intersection" is M2).
    Layer* l = doc.timeline.find(currentLayer);
    const Keyframe* k = l ? l->exposure.keyAt(currentFrame) : nullptr;
    Drawing* d = k ? doc.drawings.get(k->drawing) : nullptr;
    if (!d || !d->vector()) return;
    const float r = brushSize * 1.5f;
    std::erase_if(d->vector()->strokes, [&](const VectorStroke& s) {
        for (const auto& e : path)
            for (const auto& p : s.points)
                if (distance(e.pos, p.pos) <= r + s.width * 0.5f) return true;
        return false;
    });
}

bool Session::strokeEnd() {
    if (!stroking_) return false;
    stroking_ = false;
    builder_.end();
    Layer* l = nullptr;
    if (!editableLayer(l) || l->kind != LayerKind::Vector) return false;

    edit::Transaction tx(doc, tool == Tool::Eraser ? "Erase" : "Brush stroke");
    tx.touchLayer(l->id);
    const Keyframe* k = l->exposure.keyAt(currentFrame);
    if (tool == Tool::Eraser) {
        if (!k || !k->drawing) return false;
        tx.touchDrawing(k->drawing);
        eraseAlong(builder_.points());
    } else {
        if (!k || !k->drawing) {
            // Drawing past the end or on an empty key starts a new keyframe here.
            ops::insertBlankKeyframe(doc, *l, currentFrame);
            k = l->exposure.keyAt(currentFrame);
        }
        tx.touchDrawing(k->drawing);
        VectorStroke s;
        s.points = builder_.points();
        s.style = currentStyle;
        s.width = brushSize;
        doc.drawings.get(k->drawing)->vector()->strokes.push_back(std::move(s));
    }
    tx.commit(undo);
    ++revision_;
    return true;
}

// ---------------- palette ----------------

StyleId Session::addStyle(std::string name, Color c) {
    edit::Transaction tx(doc, "Add color");
    tx.touchPalette();
    StyleId id = doc.palette.add(doc.ids, std::move(name), c);
    tx.commit(undo);
    ++revision_;
    return id;
}

bool Session::setStyleColor(StyleId id, Color c) {
    if (!doc.palette.find(id)) return false;
    edit::Transaction tx(doc, "Change color");
    tx.touchPalette();
    if (!doc.palette.setColor(id, c)) return false;
    tx.commit(undo);
    ++revision_;
    return true;
}

// ---------------- render ----------------

void Session::tessellate(int frame, const Affine& m, bool throughCamera, bool onionSkin, std::vector<gfx::Vertex>& out) const {
    gfx::TessellateOptions opt;
    opt.canvasToScreen = m;
    opt.throughCamera = throughCamera;
    if (onionSkin) opt.onion = onion; else opt.onion.before = opt.onion.after = 0;
    gfx::tessellateFrame(doc, frame, opt, out);
    if (stroking_ && !builder_.points().empty()) {
        VectorStroke live;
        live.points = builder_.points();
        live.width = tool == Tool::Eraser ? brushSize * 3 : brushSize;
        Color c = tool == Tool::Eraser ? Color{0.5f, 0.5f, 0.5f, 0.35f} : doc.palette.resolve(currentStyle);
        const Affine view = throughCamera && doc.activeCamera(frame) ? m * doc.activeCamera(frame)->viewMatrix(frame, doc.movie) : m;
        gfx::tessellateStroke(live, view, c, 8, out);
    }
}

// ---------------- persistence ----------------

bool Session::load(const io::Package& pkg, std::string* error) {
    Document loaded;
    if (!io::load(pkg, loaded, error)) return false;
    doc = std::move(loaded);
    undo = edit::UndoStack();
    resetSelection();
    ++revision_;
    return true;
}

void Session::resetSelection() {
    auto r = rows();
    currentLayer = {};
    for (const auto& row : r) if (!row.isFolder) { currentLayer = row.id; break; }
    if (!currentLayer && !r.empty()) currentLayer = r.front().id;
    currentFrame = 0;
    currentStyle = doc.palette.styles().empty() ? StyleId{} : doc.palette.styles().front().id;
}

}  // namespace anim
