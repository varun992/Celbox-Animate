// Editing layer: undo/redo, multi-frame selection & transform, lasso, and
// tracked-object recolor. Every user-visible mutation goes through a
// Transaction so undo is uniform across tools and platforms.
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "anim/document.h"

namespace anim::edit {

// ---- Undo -----------------------------------------------------------------

class Command {
public:
    virtual ~Command() = default;
    virtual void apply(Document& doc) = 0;
    virtual void revert(Document& doc) = 0;
    virtual const std::string& name() const = 0;
};

class UndoStack {
public:
    explicit UndoStack(size_t limit = 200) : limit_(limit) {}
    // Records an already-applied command.
    void pushApplied(std::unique_ptr<Command> c);
    bool undo(Document& doc);
    bool redo(Document& doc);
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    const std::string* nextUndoName() const { return undo_.empty() ? nullptr : &undo_.back()->name(); }

private:
    size_t limit_;
    std::vector<std::unique_ptr<Command>> undo_, redo_;
};

// Snapshot transaction: declare what you are about to touch, mutate the
// document with any ops, then commit. Before/after copies of touched layers
// (shallow: exposure + animation, not children) and drawings are stored.
class Transaction {
public:
    Transaction(Document& doc, std::string name) : doc_(doc), name_(std::move(name)) {}
    void touchLayer(LayerId id);
    void touchDrawing(DrawingId id);   // existing drawing about to change
    void touchPalette();
    void commit(UndoStack& stack);     // call after mutating

private:
    friend class SnapshotCommand;
    struct LayerState { Exposure exposure; TransformTracks transform; std::vector<Effect> effects; };
    Document& doc_;
    std::string name_;
    std::unordered_map<LayerId, LayerState> layersBefore_;
    std::unordered_map<DrawingId, std::optional<Drawing>> drawingsBefore_;
    std::optional<Palette> paletteBefore_;
    uint64_t idWatermark_ = 0;
    bool started_ = false;
    void start();
};

// ---- Multi-frame selection -------------------------------------------------

struct FrameSelection {
    std::vector<LayerId> layers;
    int firstFrame = 0, lastFrame = 0;  // inclusive
};

// Unique drawings shown in the selection (linked duplicates counted once).
std::vector<DrawingId> drawingsIn(const Document& doc, const FrameSelection& sel);

// Multi-frame transform/move of whole drawings (vector).
void transformFrames(Document& doc, const FrameSelection& sel, const Affine& m, UndoStack* undo = nullptr);

// ---- Lasso ------------------------------------------------------------------

enum class LassoMode { AnyPointInside, AllPointsInside };
bool pointInPolygon(Vec2 p, const std::vector<Vec2>& poly);
std::vector<size_t> lassoStrokes(const VectorDrawing& d, const std::vector<Vec2>& poly, LassoMode mode);

// Lasso-transform across frames: in every drawing of the selection, strokes
// inside the lasso are transformed.
void lassoTransformFrames(Document& doc, const FrameSelection& sel, const std::vector<Vec2>& poly,
                          const Affine& m, LassoMode mode, UndoStack* undo = nullptr);

// ---- Tracked objects --------------------------------------------------------

// Re-style every stroke/fill of a tracked object across a frame range
// (palette edits are global; this is the per-object variant).
int restyleObject(Document& doc, const FrameSelection& sel, ObjectId obj, StyleId style, UndoStack* undo = nullptr);

}  // namespace anim::edit
