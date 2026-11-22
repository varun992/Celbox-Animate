# Architecture

## Goal

Ship on iPad first, then Mac and Windows, with the same documents, the same stroke feel and the same rendered output everywhere, without maintaining three copies of the hard parts.

## Decision: shared C++20 core + thin native shells

```
┌────────────── iPad / Mac shell (Swift, SwiftUI + UIKit/AppKit) ──────────────┐
│ Pencil input, gestures, timeline/palette UI, Files/iCloud, StoreKit, export  │
│ Metal renderer (implements the RenderBackend interface)                      │
├─────────────── Windows shell (C++ WinUI 3 or Qt) ────────────────────────────┤
│ Windows Ink / pen input, UI, Store/Stripe, D3D12 renderer                    │
└────────────────────────────────┬─────────────────────────────────────────────┘
                                 │ Swift ↔ C++ interop (Swift 5.9+) / direct C++
┌────────────────────────────────▼─────────────────────────────────────────────┐
│ animcore (C++20, no platform deps)                                            │
│  document · layers/folders · exposure (timeline) · symbols · AnimTrack tweens │
│  palette · drawings (vector + tiled raster) · camera · effects params         │
│  audio timing + lip sync · edit ops + undo · brush input pipeline · assists   │
│  scene evaluation → DisplayList · AI in-between contract · (serialization)    │
└───────────────────────────────────────────────────────────────────────────────┘
```

Why C++ rather than Rust or a cross-platform UI framework:

- **Swift calls C++ directly** (Swift 5.9+ C++ interop), so the iPad and Mac shells use the core without a hand-written C bridge. Rust would need a C ABI layer (cbindgen/UniFFI) on every type the UI touches, which is the biggest cost in a model with this many small types.
- **Windows** can use the core natively from WinUI 3 (C++/WinRT) or Qt.
- **The graphics ecosystem is C++.** Skia, OpenColorIO, FFmpeg, USD and glTF loaders, and Core ML / DirectML runtimes all have first-class C++ APIs. The 3D/Blender milestone (USD, glTF) is much easier from C++.
- **Native UI per platform.** iPad users expect Pencil hover, the Apple Pencil Pro squeeze, and iPadOS gestures. Flutter, React Native and Qt Quick can't match native Pencil latency and feel. Keeping the UI thin and native, and everything else shared, is the usual setup for pro creative apps.

**Shell boundary (M1):** the shells call the core through a small C API (`core/include/celbox_c.h`) built around an editor `Session`. A flat C surface is the most stable thing to bind from Swift (bridging header, no interop flags), from C++/WinRT on Windows, and from C# or Kotlin if needed later. Swift's direct C++ interop is still available when a shell needs deeper access.

Trade-off accepted: C++ needs discipline for memory safety. We limit it with value types, ids instead of pointers across the model, sanitizer CI builds and no raw ownership.

## Core design rules

1. **Ids, not pointers.** Objects reference each other by typed 64-bit ids (`ids.h`). This makes undo snapshots, serialization, collaboration and threading straightforward.
2. **One animatable type.** Every animated value is an `AnimTrack<T>` (`track.h`): transforms, opacity, effect params, camera, audio gain. Tweens, the graph editor, retiming and AI tools all go through one code path.
3. **Exposure vs. content.** A layer's timeline row (`Exposure`) maps frames to keyframes, and keyframes point at a `Drawing`, a `SymbolInstance` or a cel. Holds, linked duplicates and cycles therefore cost nothing, and an independent duplicate is just a clone (raster tiles are copy-on-write).
4. **Palette indirection.** Strokes store a `StyleId`, never a color. Recoloring is O(palette), not O(frames).
5. **Tracked objects.** Strokes/fills can carry an `ObjectId` that is stable across frames. That one field powers per-object recolor, multi-frame selection of "the arm", and stroke matching for AI in-betweening.
6. **Pure evaluation.** `evaluateFrame(doc, frame)` resolves folders, symbols (graphic vs. movie clip clocks), animation folders, transforms, effects and the camera into a flat `DisplayList`. Renderers only consume that list, so Metal, D3D12, the exporter and thumbnails all produce the same picture. It is read-only, so it can run on a background thread for playback caching.
7. **All edits are transactions.** Tools call `ops::*` / `edit::*` inside an `edit::Transaction`, which snapshots what it touches. Undo is the same for every tool, and the transaction log is also the hook for autosave and future collaboration.

## Modules (starter code)

| File | Responsibility |
|---|---|
| `ids.h`, `math.h` | Typed ids, Vec2/Color/Affine |
| `easing.h/.cpp`, `track.h` | Easing curves, `AnimTrack<T>`, `TransformTracks` |
| `palette.h` | Styles (color + texture) |
| `drawing.h/.cpp` | Vector strokes/fills with tracked object ids, tiled COW raster, `DrawingStore` |
| `layer.h/.cpp` | Layer kinds, `Exposure` (frames/keyframes), folders, animation folders, `Timeline` |
| `document.h/.cpp` | Canvas & movie settings, symbols, cameras + cuts, convert-to-symbol |
| `ops.h/.cpp` | Timeline verbs: F5/F6/F7, duplicate, linked duplicate, motion tween, symbol frame math, lip sync apply |
| `evaluate.h/.cpp` | Scene evaluation → `DisplayList` |
| `edit.h/.cpp` | Undo stack, transactions, multi-frame transform, lasso, object restyle |
| `brush.h/.cpp` | Stabilizer, pressure curve, resampling, dab spacing |
| `assist.h` | Grid, line/ellipse/perspective rulers, symmetry |
| `audio.h` | Audio assets/clips, phonemes, mouth chart, analyzer interface |
| `ai.h/.cpp` | In-between request/payload/provider contract, entitlement gate, geometric fallback |
| `json.h/.cpp`, `serialize.h/.cpp` | Dependency-free JSON, `.celbox` package save/load |
| `tessellate.h/.cpp` | Strokes/fills/onion skin/camera frame → GPU triangles (shared by all renderers) |
| `session.h/.cpp` | Editor session: current layer/frame/tool/color, timeline verbs, strokes, undo for every edit |
| `celbox_c.h`, `celbox_c.cpp` | C API used by the platform shells |

## Rendering plan (next)

- `RenderBackend` interface in core: `beginFrame`, `pushGroup(opacity, blend, effects)`, `drawDrawing(id, transform)`, `popGroup`. Metal is first; D3D12 or Vulkan follows for Windows.
- Vector strokes tessellated in core (shared) into triangle strips with per-vertex width; fills triangulated. The GPU only rasterizes.
- Raster layers live as GPU textures per 256px tile; brush dabs are stamped by a compute shader. Tiles sync back to the core's `RasterDrawing` on stroke end for undo and save.
- Effects are implemented per backend from the same `EffectParams` (separable Gaussian, glow = blur + add, etc.), with golden-image tests shared across backends.
- Playback cache: evaluate and render frames ahead on a background queue, invalidated by transaction-touched layers.

## Persistence (M1, implemented in `serialize.h`)

`.celbox` package (a directory, or a zip for sharing): `document.json` (model, versioned schema), `drawings/<id>.json` (vector), `drawings/<id>.raster` (tiles), later `audio/*` and `thumbs/`. A schema version field plus migration functions keep old files opening, and files from a newer version are refused instead of half-loaded. Next steps: write only changed drawings on autosave, compress tiles with zstd, and use a compact binary vector format.

## AI in-betweening (paid) plan

- **Client contract (done):** `InbetweenRequest` (layer, start/end keys, guide frames, step, timing curve, layer-context flag) → `InbetweenPayload` (key drawings with `ObjectId`s) → provider → `GeneratedFrame`s inserted as one undoable transaction.
- **Gate:** `IEntitlements` backed by StoreKit 2 on Apple and the Microsoft Store/Stripe on Windows. The server re-verifies the receipt before running inference, so the client check is only for the UI.
- **Providers:** `cloud` (paid; GPU service, async with progress and cancellation), optional `on-device` (Core ML / DirectML for small rigs), and `geometric` (free, already in core) as a fallback and preview.
- **Data:** send vector strokes with object ids and the layer hierarchy, not flattened bitmaps. Matched strokes make results editable and keep palette links.

## 3D / Blender interop plan

- `LayerKind::Model3D` + `Model3DRef` placeholder exist. The evaluator already emits a `DrawModel3D` command at the right place in the stack.
- Import glTF 2.0 (cgltf / tinygltf) and USDZ (OpenUSD on desktop, RealityKit/ModelIO on iPad), with baked animation clips sampled by frame.
- Camera bridge: map the 2D `Camera` (center, zoom, rotation) to a 3D orthographic or perspective camera. Export the camera and scene timing to Blender (glTF/USD camera, or a small Blender add-on), and re-import on file change.
- Render 3D into an offscreen texture per layer, then composite it like any other group so effects, clipping and blend modes apply.

## Testing and parity

- Core unit tests (`tests/test_main.cpp`, no dependencies) run on Linux, macOS and Windows CI with ASan/UBSan.
- Golden-image tests: render fixed documents through each backend and compare against shared references with a small tolerance.
- Document round-trip tests once serialization lands.

## Threading

The main/UI thread owns the mutable `Document`. Evaluation and rendering read an immutable snapshot (copy-on-write structures are a later optimization; for now, evaluate on the main thread and render the `DisplayList` on the render thread). The brush input pipeline runs on the input thread and hands finished strokes to a transaction.
