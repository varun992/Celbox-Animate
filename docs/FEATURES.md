# Feature list

Priorities: **P0** = needed for a first usable release (MVP), **P1** = needed to compete with the desktop tools, **P2** = later / differentiators.
"Core" says whether the logic lives in the shared C++ core (same on iPad, Mac, Windows) or in the platform UI shell. ✅ marks things the starter core already implements and tests.

Sources reviewed: the [OpenToonz manual](https://opentoonz.readthedocs.io/en/latest/), Adobe Animate's timeline, symbol and tween model (from its documentation and common usage), and Clip Studio Paint's [animation folders and cels](https://help.clip-studio.com/en-us/manual_en/600_animation/Animation_folders_and_cels.htm), [Timeline palette](https://help.clip-studio.com/en-us/manual_en/600_animation/Timeline_Palette.htm), [keyframes](https://help.clip-studio.com/en-us/manual_en/600_animation/Using_keyframes.htm) and [camera movement](https://tips.clip-studio.com/en-us/articles/2046). The design here is original. No code was taken from any of them.

## What we borrow from each tool

| Idea | From | Why it matters here |
|---|---|---|
| Horizontal layer timeline, F5/F6/F7 frame verbs, frame labels | Animate | Fastest mental model for keyframe animation (req. 1) |
| Graphic vs Movie Clip symbols with own timelines, loop modes, "Convert to Symbol" | Animate | Reusable animated parts (req. 1) |
| Motion tweens on any object, easing presets, motion editor | Animate | Req. 4 |
| Shared studio palette: strokes reference palette styles | OpenToonz | Change a color once and every frame updates (req. 1) |
| Xsheet-style drawing reuse (same drawing exposed many times) | OpenToonz | Cheap holds and cycles; "linked duplicate" |
| FX schematic and per-layer animatable FX | OpenToonz | Blur/glow etc. (req. 2) |
| Plastic/mesh deformation, cameras with multiple camera cuts | OpenToonz | Puppet rigs, camera work (req. 5) |
| Animation folders + cels, nested folders with their own transform keys | Clip Studio Paint | Organizing & animating inside folders (req. 2) |
| Rulers, perspective rulers, symmetry, vector eraser to intersection, close-gap fill | Clip Studio Paint | Drawing tools (req. 3) |

## 1. Animation mechanics

| Feature | Pri | Core | Status |
|---|---|---|---|
| Horizontal timeline: layers as rows, frames as columns, playhead, zoomable | P0 | UI (model in core) | ✅ model (`Exposure`) |
| Insert frame (hold), keyframe (copy previous), blank keyframe, remove frames | P0 | Core | ✅ `ops::insertFrames / insertKeyframe / insertBlankKeyframe / removeFrames` |
| Duplicate frame/span as an independent copy | P0 | Core | ✅ `ops::duplicateSpan` |
| Linked duplicate (reuse drawing) and "make unique" | P0 | Core | ✅ `ops::linkedDuplicate / makeUnique` |
| Drag keyframes, drag span ends to retime, multi-select cells | P0 | UI + core ops | planned |
| Onion skin (before/after count, tint, opacity, keys only option) | P0 | Core eval + GPU | planned |
| Playback with frame-rate lock, loop range, scrubbing with audio | P0 | Shell | planned |
| Symbols: Graphic (synced) and Movie Clip (own clock), loop/play once/single frame/reverse | P0 | Core | ✅ `SymbolDef`, `ops::symbolFrame` |
| Convert to symbol, edit symbol in place, symbol library | P0 | Core + UI | ✅ convert; library UI planned |
| Tracked objects: stable `ObjectId` on strokes/fills across frames | P0 | Core | ✅ (used by recolor and AI) |
| Palette styles referenced by strokes; change once, updates all frames | P0 | Core | ✅ `Palette` |
| Recolor a tracked object across a frame range | P1 | Core | ✅ `edit::restyleObject` |
| Multi-frame edit: select a range of frames/layers and move/scale/rotate together | P0 | Core | ✅ `edit::transformFrames` |
| Multi-frame lasso transform | P1 | Core | ✅ `edit::lassoTransformFrames` |
| Frame labels, comments, markers | P1 | Core | ✅ label field |
| Timing chart / spacing tool for in-betweens | P1 | Core | ✅ easing on AI requests |
| Shape tweens (vector morph) | P2 | Core | baseline in geometric in-betweener |
| Mesh/puppet deformation (bones, pins) | P2 | Core | planned |

## 2. Organization, brushes, effects

| Feature | Pri | Core | Status |
|---|---|---|---|
| Nested folders with their own animatable transform/opacity/blend | P0 | Core | ✅ `LayerKind::Folder` |
| Animation folders with cels (Clip Studio style) | P0 | Core | ✅ `Layer::animationFolder` |
| Drag layers into/out of folders, reorder, lock, hide, color tags | P0 | Core + UI | ✅ `Timeline::move` |
| Clipping masks and blend modes | P1 | Core + GPU | ✅ model |
| Vector layers (pressure-width centerline strokes, fills) | P0 | Core | ✅ model |
| Raster layers, tiled, copy-on-write so duplicates are cheap | P0 | Core | ✅ `RasterDrawing` |
| Stroke stabilizer (pulled-string + smoothing), pressure curve, even resampling | P0 | Core | ✅ `brush::StrokeBuilder` |
| GPU brush stamping (dabs, grain texture, tilt, wet mixing) | P0 | Metal / D3D | dab generator ✅, GPU planned |
| Brush presets, import (.abr later), custom brush editor | P1 | Core + UI | planned |
| Vector eraser (stroke / to intersection), vector smoothing | P1 | Core | planned |
| Animatable effects: blur, glow, drop shadow, color adjust, motion blur | P1 | Core params + GPU | ✅ params (`Effect`) |

## 3. Drawing tools

| Feature | Pri | Core | Status |
|---|---|---|---|
| Grid with snapping | P0 | Core | ✅ `assist::Grid` |
| Line ruler, ellipse ruler, perspective ruler (1-3 point) | P1 | Core | ✅ line/ellipse/1-point |
| Symmetry: mirror, radial, kaleidoscope | P1 | Core | ✅ `assist::Symmetry` |
| Lasso, rectangle, magic-wand selection; single frame and multi-frame | P0 | Core | ✅ lasso (vector) |
| Transform: move/scale/rotate/skew/distort with pivot | P0 | Core | ✅ affine |
| Paint bucket with gap closing and tolerance (raster + vector regions) | P0 | Core | planned (M2) |
| Gradient tool (linear/radial), as fills or palette styles | P1 | Core + GPU | planned |
| Textures on palette styles and paper texture overlay | P1 | Core + GPU | ✅ `TextureRef` model |
| Eyedropper, color wheel, swatches, recent colors | P0 | UI | planned |
| Reference layers, light table | P1 | Core | planned |

## 4. Tweens

| Feature | Pri | Core | Status |
|---|---|---|---|
| Motion tween on any layer, folder or symbol (position, rotation, scale, skew, pivot, opacity) | P0 | Core | ✅ `ops::createMotionTween`, `TransformTracks` |
| Easing presets and custom cubic bezier | P0 | Core | ✅ `Easing` |
| Hold (stepped) keys | P0 | Core | ✅ |
| Graph / motion editor UI | P1 | UI | planned |
| Motion paths (move along a curve, orient to path) | P1 | Core | planned |
| Sub-frame evaluation (motion blur, audio sync) | P1 | Core | ✅ fractional frames |

## 5. Camera

| Feature | Pri | Core | Status |
|---|---|---|---|
| Animated camera: pan, zoom, rotate, with tweens | P0 | Core | ✅ `Camera` |
| Camera frame overlay on the canvas | P0 | Core + UI | ✅ `frameCorners` |
| Multiple cameras with cuts | P1 | Core | ✅ `cameraCuts` |
| Camera shake preset, parallax / multiplane depth | P2 | Core | planned |

## 6. Canvas and movie settings

| Feature | Pri | Core | Status |
|---|---|---|---|
| Canvas size separate from output size (bigger canvas for camera pans) | P0 | Core | ✅ `CanvasSettings` / `MovieSettings` |
| FPS, duration, resolution presets, background color | P0 | Core | ✅ |
| Resize canvas (anchor), crop to camera | P1 | Core | planned |

## 7. App hygiene, AI, cross-platform

| Feature | Pri | Core | Status |
|---|---|---|---|
| Undo/redo for every edit | P0 | Core | ✅ `edit::Transaction` / `UndoStack` |
| Save/load (`.anim` package), autosave, crash recovery, versions | P0 | Core | planned (M1) |
| Export MP4/H.264/HEVC, GIF, PNG sequence, ProRes with alpha | P0 | Shell encoders, core frames | planned |
| Import images, video reference, PSD (layered) | P1 | Core + shell | planned |
| iPad: Pencil pressure/tilt, hover preview, squeeze/double-tap, palm rejection, gestures (two-finger undo, three-finger redo, pinch/rotate canvas) | P0 | Shell | planned |
| Keyboard shortcuts (iPad Magic Keyboard, Mac, Windows) | P1 | Shell (shared keymap file) | planned |
| Files app / iCloud Drive, document browser, thumbnails | P0 | Shell | planned |
| Preferences, onboarding, templates, accessibility (VoiceOver, dynamic type) | P1 | Shell | planned |
| Telemetry-free crash reports, performance HUD | P1 | Shell | planned |
| **AI in-betweening (paid)**: first/last + optional guide frames + layer structure in, generated frames out, undoable | P2 | Core contract, cloud provider | ✅ hook (`ai::InbetweenService`) + free geometric fallback |
| Subscriptions / StoreKit 2 entitlements, server receipt check | P2 | Shell + server | ✅ entitlement interface |

## 8. Audio, lip sync, SFX

| Feature | Pri | Core | Status |
|---|---|---|---|
| Audio layers with clips: trim, move, gain envelope, mute, role (dialogue/SFX/music) | P0 | Core | ✅ `AudioClip` |
| Waveform in the timeline, scrub audio | P0 | Core peaks + shell playback | ✅ peaks field |
| Record voice-over in app | P1 | Shell | planned |
| Lip sync: phoneme track drives a mouth symbol through a mouth chart | P1 | Core | ✅ `LipSyncTrack`, `ops::applyLipSync` |
| Automatic phoneme detection (on-device) | P1 | Pluggable analyzer | ✅ `ILipSyncAnalyzer` interface |
| SFX library, fades, audio mixdown on export | P1 | Shell | planned |

## Future: 3D objects with Blender round-trip

| Feature | Pri | Core | Status |
|---|---|---|---|
| 3D layer type holding a glTF or USD(Z) asset with baked animation | P2 | Core + GPU | ✅ placeholder (`LayerKind::Model3D`, `Model3DRef`) |
| Camera sync: 2D camera drives a 3D camera (and import Blender camera) | P2 | Core | planned |
| Export scene camera + timing to Blender; re-import updated asset on change | P2 | Core + shell | planned |
| Toon shading / line rendering so 3D matches 2D art | P2 | GPU | planned |
