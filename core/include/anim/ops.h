// Timeline editing operations (the verbs behind the timeline UI). They mutate
// the document directly; wrap them in an edit::Transaction for undo.
#pragma once
#include "anim/document.h"

namespace anim::ops {

// F6: new keyframe at `frame` whose drawing is an independent copy of the one
// currently showing (or blank if nothing shows). Returns the new drawing id.
DrawingId insertKeyframe(Document& doc, Layer& layer, int frame);

// F7: blank keyframe. For drawing layers a fresh empty drawing is created so
// the user can draw immediately.
DrawingId insertBlankKeyframe(Document& doc, Layer& layer, int frame);

// F5: hold the current content `count` more frames.
void insertFrames(Layer& layer, int frame, int count = 1);
void removeFrames(Layer& layer, int frame, int count = 1);

// Duplicate the span containing `frame` right after itself, as an independent
// copy. Later keys shift right. Returns the new drawing (if any).
DrawingId duplicateSpan(Document& doc, Layer& layer, int frame);

// Linked duplicate: new key at `dstFrame` that reuses the same drawing.
// Editing either edits both (xsheet-style cell reuse; cheap holds/cycles).
bool linkedDuplicate(Layer& layer, int srcFrame, int dstFrame);

// Converts a linked key into an independent copy ("make unique").
DrawingId makeUnique(Document& doc, Layer& layer, int frame);

// Motion tween over the span containing `frame`: puts transform keys at the
// span start and at the start of the next span, so the user only edits the
// end pose. Works on any layer kind, including folders and symbol layers.
void createMotionTween(Layer& layer, int startFrame, int endFrame, Easing ease = Easing::linear());

// Sets a full transform pose key on all transform tracks.
struct Pose { Vec2 position{0, 0}; float rotationDeg = 0; Vec2 scale{1, 1}; float skewDeg = 0; float opacity = 1; };
void setPoseKey(Layer& layer, int frame, const Pose& pose, Easing ease = Easing::linear());
Pose poseAt(const Layer& layer, double frame);

// Symbol frame resolution. `parentLocalFrame` is the frame inside the parent
// timeline; `clockFrames` is how many document frames have elapsed since the
// instance appeared (used by movie clips).
int symbolFrame(const SymbolDef& def, const SymbolInstance& inst, int framesIntoSpan, int clockFrames);

// Writes a lip sync track onto its target symbol layer as single-frame keys.
bool applyLipSync(Document& doc, const LipSyncTrack& track);

}  // namespace anim::ops
