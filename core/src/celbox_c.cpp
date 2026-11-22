#include "celbox_c.h"

#include <algorithm>
#include <exception>
#include <string>
#include <vector>

#include "anim/session.h"

using namespace anim;

struct CbxSession {
    Session session;
    std::string scratch;  // backing store for returned strings
    std::string error;
    std::vector<gfx::Vertex> vertices;
    std::vector<std::pair<std::string, io::Bytes>> saveFiles;
    io::Package loading;
};

static_assert(sizeof(CbxVertex) == sizeof(gfx::Vertex), "vertex layout must match");

namespace {
template <class Tag> Id<Tag> id(uint64_t v) { return Id<Tag>{v}; }
Affine toAffine(CbxAffine m) { return {m.a, m.b, m.c, m.d, m.tx, m.ty}; }
}  // namespace

extern "C" {

CbxSession* cbx_session_new(void) {
    try { return new CbxSession(); } catch (...) { return nullptr; }
}
void cbx_session_free(CbxSession* s) { delete s; }
uint64_t cbx_revision(const CbxSession* s) { return s->session.revision(); }
const char* cbx_core_version(void) { return "0.2.0"; }

CbxSettings cbx_get_settings(const CbxSession* s) {
    const Document& d = s->session.doc;
    return {d.canvas.width, d.canvas.height, d.movie.width, d.movie.height, d.movie.fps, d.movie.frameCount,
            d.canvas.background.r, d.canvas.background.g, d.canvas.background.b};
}
void cbx_set_settings(CbxSession* s, CbxSettings v) {
    CanvasSettings canvas = s->session.doc.canvas;
    MovieSettings movie = s->session.doc.movie;
    canvas.width = std::clamp(v.canvasWidth, 16, 16384);
    canvas.height = std::clamp(v.canvasHeight, 16, 16384);
    movie.width = std::clamp(v.movieWidth, 16, 8192);
    movie.height = std::clamp(v.movieHeight, 16, 8192);
    movie.fps = v.fps > 0 ? std::min(v.fps, 120.0) : 24;
    movie.frameCount = std::max(0, v.frameCount);
    canvas.background = {v.backgroundR, v.backgroundG, v.backgroundB, 1};
    s->session.setSettings(canvas, movie);
}

int32_t cbx_layer_count(const CbxSession* s) { return int32_t(s->session.rows().size()); }
CbxLayerInfo cbx_layer_info(const CbxSession* s, int32_t row) {
    auto rows = s->session.rows();
    if (row < 0 || row >= int32_t(rows.size())) return {};
    const auto& r = rows[size_t(row)];
    return {r.id.value, r.depth, int32_t(r.kind), r.visible, r.locked, r.isFolder};
}
const char* cbx_layer_name(CbxSession* s, int32_t row) {
    auto rows = s->session.rows();
    s->scratch = row >= 0 && row < int32_t(rows.size()) ? rows[size_t(row)].name : std::string();
    return s->scratch.c_str();
}
uint64_t cbx_add_layer(CbxSession* s, CbxLayerKind kind) { return s->session.addLayer(LayerKind(kind)).value; }
bool cbx_delete_layer(CbxSession* s, uint64_t l) { return s->session.deleteLayer(id<LayerTag>(l)); }
bool cbx_rename_layer(CbxSession* s, uint64_t l, const char* name) { return s->session.renameLayer(id<LayerTag>(l), name ? name : ""); }
bool cbx_set_layer_visible(CbxSession* s, uint64_t l, bool v) { return s->session.setVisible(id<LayerTag>(l), v); }
bool cbx_set_layer_locked(CbxSession* s, uint64_t l, bool v) { return s->session.setLocked(id<LayerTag>(l), v); }
bool cbx_move_layer(CbxSession* s, uint64_t l, int32_t delta) { return s->session.moveLayer(id<LayerTag>(l), delta); }
uint64_t cbx_current_layer(const CbxSession* s) { return s->session.currentLayer.value; }
void cbx_set_current_layer(CbxSession* s, uint64_t l) { s->session.currentLayer = id<LayerTag>(l); }

int32_t cbx_frame_count(const CbxSession* s) { return s->session.frameCount(); }
int32_t cbx_current_frame(const CbxSession* s) { return s->session.currentFrame; }
void cbx_set_current_frame(CbxSession* s, int32_t f) { s->session.currentFrame = std::max(0, f); }
void cbx_row_states(const CbxSession* s, uint64_t l, int32_t count, uint8_t* out) {
    if (out && count > 0) s->session.rowStates(id<LayerTag>(l), count, out);
}
bool cbx_insert_frame(CbxSession* s) { return s->session.insertFrame(); }
bool cbx_insert_keyframe(CbxSession* s) { return s->session.insertKeyframe(); }
bool cbx_insert_blank_keyframe(CbxSession* s) { return s->session.insertBlankKeyframe(); }
bool cbx_duplicate_frame(CbxSession* s) { return s->session.duplicateFrame(); }
bool cbx_remove_frame(CbxSession* s) { return s->session.removeFrame(); }
bool cbx_clear_keyframe(CbxSession* s) { return s->session.clearKeyframe(); }

void cbx_set_tool(CbxSession* s, CbxTool t) { s->session.tool = t == CBX_TOOL_ERASER ? Tool::Eraser : Tool::Brush; }
void cbx_set_brush_size(CbxSession* s, float size) { s->session.brushSize = std::clamp(size, 0.5f, 500.0f); }
float cbx_brush_size(const CbxSession* s) { return s->session.brushSize; }
void cbx_set_stabilizer(CbxSession* s, float lazy, int32_t smoothing) {
    s->session.stabilizer.lazyRadius = std::max(0.0f, lazy);
    s->session.stabilizer.smoothingWindow = std::clamp(smoothing, 0, 64);
}
void cbx_stroke_begin(CbxSession* s, float x, float y, float p, double t) { s->session.strokeBegin({x, y}, p, t); }
void cbx_stroke_add(CbxSession* s, float x, float y, float p, double t) { s->session.strokeAdd({x, y}, p, t); }
bool cbx_stroke_end(CbxSession* s) { return s->session.strokeEnd(); }
void cbx_stroke_cancel(CbxSession* s) { s->session.strokeCancel(); }

int32_t cbx_style_count(const CbxSession* s) { return int32_t(s->session.doc.palette.styles().size()); }
CbxStyleInfo cbx_style_info(const CbxSession* s, int32_t i) {
    const auto& st = s->session.doc.palette.styles();
    if (i < 0 || i >= int32_t(st.size())) return {};
    const auto& x = st[size_t(i)];
    return {x.id.value, x.color.r, x.color.g, x.color.b, x.color.a};
}
uint64_t cbx_add_style(CbxSession* s, float r, float g, float b, float a) {
    return s->session.addStyle("Color " + std::to_string(cbx_style_count(s) + 1), {r, g, b, a}).value;
}
bool cbx_set_style_color(CbxSession* s, uint64_t st, float r, float g, float b, float a) {
    return s->session.setStyleColor(id<StyleTag>(st), {r, g, b, a});
}
uint64_t cbx_current_style(const CbxSession* s) { return s->session.currentStyle.value; }
void cbx_set_current_style(CbxSession* s, uint64_t st) { s->session.currentStyle = id<StyleTag>(st); }

void cbx_set_onion(CbxSession* s, int32_t before, int32_t after, float opacity) {
    s->session.onion.before = std::clamp(before, 0, 10);
    s->session.onion.after = std::clamp(after, 0, 10);
    s->session.onion.opacity = std::clamp(opacity, 0.0f, 1.0f);
}

bool cbx_undo(CbxSession* s) { return s->session.undoLast(); }
bool cbx_redo(CbxSession* s) { return s->session.redoLast(); }
bool cbx_can_undo(const CbxSession* s) { return s->session.undo.canUndo(); }
bool cbx_can_redo(const CbxSession* s) { return s->session.undo.canRedo(); }

int32_t cbx_tessellate(CbxSession* s, int32_t frame, CbxAffine m, bool throughCamera, bool onion) {
    s->session.tessellate(frame, toAffine(m), throughCamera, onion, s->vertices);
    return int32_t(s->vertices.size());
}
const CbxVertex* cbx_vertices(const CbxSession* s) {
    return reinterpret_cast<const CbxVertex*>(s->vertices.data());
}

int32_t cbx_save_begin(CbxSession* s) {
    s->saveFiles.clear();
    for (auto& [p, b] : s->session.save()) s->saveFiles.emplace_back(p, std::move(b));
    return int32_t(s->saveFiles.size());
}
const char* cbx_save_path(const CbxSession* s, int32_t i) {
    return i >= 0 && i < int32_t(s->saveFiles.size()) ? s->saveFiles[size_t(i)].first.c_str() : "";
}
const uint8_t* cbx_save_data(const CbxSession* s, int32_t i, size_t* len) {
    if (i < 0 || i >= int32_t(s->saveFiles.size())) { if (len) *len = 0; return nullptr; }
    if (len) *len = s->saveFiles[size_t(i)].second.size();
    return s->saveFiles[size_t(i)].second.data();
}
void cbx_save_end(CbxSession* s) { s->saveFiles.clear(); }

void cbx_load_begin(CbxSession* s) { s->loading.clear(); }
void cbx_load_add(CbxSession* s, const char* path, const uint8_t* data, size_t len) {
    if (path) s->loading[path] = io::Bytes(data, data + len);
}
bool cbx_load_finish(CbxSession* s) {
    s->error.clear();
    bool ok = false;
    try { ok = s->session.load(s->loading, &s->error); } catch (const std::exception& e) { s->error = e.what(); }
    s->loading.clear();
    return ok;
}
const char* cbx_last_error(const CbxSession* s) { return s->error.c_str(); }

}  // extern "C"
