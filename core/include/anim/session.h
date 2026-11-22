// Editor session: a document plus the editing state every shell needs
// (current layer/frame/color/tool, undo, the live stroke). Platform shells
// talk to this, usually through the C API in celbox_c.h.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "anim/brush.h"
#include "anim/edit.h"
#include "anim/serialize.h"
#include "anim/tessellate.h"

namespace anim {

enum class CellState : uint8_t {
    Empty = 0,       // past the end of the row
    Key = 1,         // keyframe with content
    Hold = 2,        // held frame of a content key
    BlankKey = 3,    // keyframe with nothing drawn
    BlankHold = 4,   // held frame of a blank key
};

struct LayerRow {
    LayerId id;
    int depth;
    LayerKind kind;
    bool visible, locked, isFolder;
    std::string name;
};

enum class Tool { Brush, Eraser };

class Session {
public:
    Session();  // new document with one vector layer and a starter palette

    Document doc;
    edit::UndoStack undo;
    LayerId currentLayer;
    int currentFrame = 0;
    StyleId currentStyle;
    Tool tool = Tool::Brush;
    float brushSize = 6.0f;
    brush::StabilizerSettings stabilizer{0.0f, 4, 1.5f};
    gfx::OnionSkin onion{1, 1};

    // ---- layers ----
    std::vector<LayerRow> rows() const;   // top-most first, folders expanded
    LayerId addLayer(LayerKind kind, std::string name = {});
    bool deleteLayer(LayerId id);
    bool renameLayer(LayerId id, std::string name);
    bool setVisible(LayerId id, bool v);
    bool setLocked(LayerId id, bool v);
    bool moveLayer(LayerId id, int delta);  // +1 = up one slot within its folder
    bool setSettings(const CanvasSettings& canvas, const MovieSettings& movie);

    // ---- frames (act on currentLayer / currentFrame) ----
    CellState cell(LayerId layer, int frame) const;
    void rowStates(LayerId layer, int count, uint8_t* out) const;
    bool insertFrame();
    bool insertKeyframe();
    bool insertBlankKeyframe();
    bool duplicateFrame();
    bool removeFrame();
    bool clearKeyframe();
    int frameCount() const { return doc.frameCount(); }

    // ---- drawing ----
    void strokeBegin(Vec2 canvasPos, float pressure, double t);
    void strokeAdd(Vec2 canvasPos, float pressure, double t);
    bool strokeEnd();            // commits to the current layer/frame (undoable)
    void strokeCancel();
    bool strokeActive() const { return stroking_; }
    const std::vector<StrokePoint>& livePoints() const { return builder_.points(); }

    // ---- palette ----
    StyleId addStyle(std::string name, Color c);
    bool setStyleColor(StyleId id, Color c);  // undoable; updates every frame using it

    // ---- undo ----
    bool undoLast() { bool ok = undo.undo(doc); validateSelection(); if (ok) ++revision_; return ok; }
    bool redoLast() { bool ok = undo.redo(doc); validateSelection(); if (ok) ++revision_; return ok; }

    // ---- render ----
    void tessellate(int frame, const Affine& canvasToScreen, bool throughCamera, bool onionSkin, std::vector<gfx::Vertex>& out) const;

    // ---- persistence ----
    io::Package save() const { return io::save(doc); }
    bool load(const io::Package& pkg, std::string* error);

    // Bumped on every change; shells use it to mark documents dirty / autosave.
    uint64_t revision() const { return revision_; }

private:
    bool editableLayer(Layer*& l) const;
    template <class Fn> bool frameEdit(const char* name, Fn fn);
    template <class Fn> bool structureEdit(const char* name, Fn fn);  // layer tree / settings
    void validateSelection();
    void eraseAlong(const std::vector<StrokePoint>& path);
    void resetSelection();

    brush::StrokeBuilder builder_;
    bool stroking_ = false;
    uint64_t revision_ = 0;
};

}  // namespace anim
