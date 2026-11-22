/* Celbox core C API.
 *
 * The stable boundary between the shared C++ core and the platform shells.
 * Swift imports it through a bridging header; Windows (C++/WinRT or C#) can
 * use it the same way. Plain C types only: no exceptions, no STL across it.
 *
 * Strings returned by the API stay valid until the next call on the same
 * session. All calls must happen on the thread that owns the session.
 */
#ifndef CELBOX_C_H
#define CELBOX_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CbxSession CbxSession;

typedef enum {
    CBX_LAYER_FOLDER = 0,
    CBX_LAYER_VECTOR = 1,
    CBX_LAYER_RASTER = 2,
    CBX_LAYER_SYMBOL = 3,
    CBX_LAYER_AUDIO = 4,
    CBX_LAYER_MODEL3D = 5,
} CbxLayerKind;

typedef enum {
    CBX_CELL_EMPTY = 0,
    CBX_CELL_KEY = 1,
    CBX_CELL_HOLD = 2,
    CBX_CELL_BLANK_KEY = 3,
    CBX_CELL_BLANK_HOLD = 4,
} CbxCellState;

typedef enum { CBX_TOOL_BRUSH = 0, CBX_TOOL_ERASER = 1 } CbxTool;

typedef struct {
    uint64_t id;
    int32_t depth;
    int32_t kind; /* CbxLayerKind */
    bool visible;
    bool locked;
    bool isFolder;
} CbxLayerInfo;

typedef struct {
    float a, b, c, d, tx, ty; /* x' = a*x + c*y + tx ; y' = b*x + d*y + ty */
} CbxAffine;

typedef struct {
    float x, y, r, g, b, a;
} CbxVertex;

typedef struct {
    uint64_t id;
    float r, g, b, a;
} CbxStyleInfo;

typedef struct {
    int32_t canvasWidth, canvasHeight;
    int32_t movieWidth, movieHeight;
    double fps;
    int32_t frameCount; /* 0 = follow the timeline */
    float backgroundR, backgroundG, backgroundB;
} CbxSettings;

/* ---- lifetime ---- */
CbxSession* cbx_session_new(void);
void cbx_session_free(CbxSession* s);
uint64_t cbx_revision(const CbxSession* s);
const char* cbx_core_version(void);

/* ---- settings ---- */
CbxSettings cbx_get_settings(const CbxSession* s);
void cbx_set_settings(CbxSession* s, CbxSettings settings); /* undoable */

/* ---- layers (rows listed top-most first) ---- */
int32_t cbx_layer_count(const CbxSession* s);
CbxLayerInfo cbx_layer_info(const CbxSession* s, int32_t row);
const char* cbx_layer_name(CbxSession* s, int32_t row);
uint64_t cbx_add_layer(CbxSession* s, CbxLayerKind kind);
bool cbx_delete_layer(CbxSession* s, uint64_t layer);
bool cbx_rename_layer(CbxSession* s, uint64_t layer, const char* name);
bool cbx_set_layer_visible(CbxSession* s, uint64_t layer, bool visible);
bool cbx_set_layer_locked(CbxSession* s, uint64_t layer, bool locked);
bool cbx_move_layer(CbxSession* s, uint64_t layer, int32_t delta);
uint64_t cbx_current_layer(const CbxSession* s);
void cbx_set_current_layer(CbxSession* s, uint64_t layer);

/* ---- frames ---- */
int32_t cbx_frame_count(const CbxSession* s);
int32_t cbx_current_frame(const CbxSession* s);
void cbx_set_current_frame(CbxSession* s, int32_t frame);
void cbx_row_states(const CbxSession* s, uint64_t layer, int32_t count, uint8_t* out); /* CbxCellState per frame */
bool cbx_insert_frame(CbxSession* s);          /* hold (F5) */
bool cbx_insert_keyframe(CbxSession* s);       /* copy of current drawing (F6) */
bool cbx_insert_blank_keyframe(CbxSession* s); /* F7 */
bool cbx_duplicate_frame(CbxSession* s);
bool cbx_remove_frame(CbxSession* s);
bool cbx_clear_keyframe(CbxSession* s);

/* ---- drawing (canvas coordinates) ---- */
void cbx_set_tool(CbxSession* s, CbxTool tool);
void cbx_set_brush_size(CbxSession* s, float size);
float cbx_brush_size(const CbxSession* s);
void cbx_set_stabilizer(CbxSession* s, float lazyRadius, int32_t smoothing);
void cbx_stroke_begin(CbxSession* s, float x, float y, float pressure, double time);
void cbx_stroke_add(CbxSession* s, float x, float y, float pressure, double time);
bool cbx_stroke_end(CbxSession* s);
void cbx_stroke_cancel(CbxSession* s);

/* ---- palette ---- */
int32_t cbx_style_count(const CbxSession* s);
CbxStyleInfo cbx_style_info(const CbxSession* s, int32_t index);
uint64_t cbx_add_style(CbxSession* s, float r, float g, float b, float a);
bool cbx_set_style_color(CbxSession* s, uint64_t style, float r, float g, float b, float a);
uint64_t cbx_current_style(const CbxSession* s);
void cbx_set_current_style(CbxSession* s, uint64_t style);

/* ---- onion skin ---- */
void cbx_set_onion(CbxSession* s, int32_t before, int32_t after, float opacity);

/* ---- undo ---- */
bool cbx_undo(CbxSession* s);
bool cbx_redo(CbxSession* s);
bool cbx_can_undo(const CbxSession* s);
bool cbx_can_redo(const CbxSession* s);

/* ---- render: tessellate a frame into screen-space triangles ---- */
int32_t cbx_tessellate(CbxSession* s, int32_t frame, CbxAffine canvasToScreen, bool throughCamera, bool onionSkin);
const CbxVertex* cbx_vertices(const CbxSession* s); /* valid until the next cbx_tessellate */

/* ---- persistence (package = set of named files) ---- */
int32_t cbx_save_begin(CbxSession* s); /* returns number of files */
const char* cbx_save_path(const CbxSession* s, int32_t i);
const uint8_t* cbx_save_data(const CbxSession* s, int32_t i, size_t* length);
void cbx_save_end(CbxSession* s);

void cbx_load_begin(CbxSession* s);
void cbx_load_add(CbxSession* s, const char* path, const uint8_t* data, size_t length);
bool cbx_load_finish(CbxSession* s); /* on failure, cbx_last_error() explains */
const char* cbx_last_error(const CbxSession* s);

#ifdef __cplusplus
}
#endif

#endif /* CELBOX_C_H */
