# Celbox

Celbox, by Illuminart Studios, is an animation app for iPad, with Mac and Windows to follow.

This repository holds `animcore`, the platform-independent C++20 core shared by every Celbox app. It contains the document model, the timeline, symbols, tweens, camera, effects parameters, audio/lip sync, editing and undo, the brush input pipeline, drawing assists, and the AI in-betweening hook.

- [docs/FEATURES.md](docs/FEATURES.md): prioritized feature list mapped to the 8 requirements, with what already exists
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): architecture and the reasons behind it
- [apps/ipad](apps/ipad/README.md): the iPad app (SwiftUI + Metal). See there for how to run it in Xcode.

## Build and test

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/animcore_tests
```

Requires CMake 3.20+ and a C++20 compiler (Xcode 15+, recent Clang/GCC, or MSVC 2022). The core has no third-party dependencies.

## Using it from Swift (iPad/Mac)

Add `core/include` to the Xcode target's header search paths, enable **C++ and Objective-C interoperability = C++/Objective-C++** in Build Settings, and add a module map exposing `anim/*.h`. Swift can then create an `anim.Document` and call `anim.ops.insertKeyframe(...)` directly.

## Milestones

| # | Milestone | Outcome |
|---|---|---|
| M0 | Core skeleton ✅ | Model, timeline ops, symbols, tweens, camera, effects params, lip sync, AI hook, undo |
| M1 | Persistence + iPad shell v0 ✅ | `.celbox` save/load + autosave; SwiftUI app with document browser, canvas (Metal) and horizontal timeline; Pencil vector drawing with stabilizer; playback |
| M2 | Drawing tools | Raster GPU brushes, eraser, paint bucket with gap close, lasso UI, transforms, rulers/symmetry UI, onion skin |
| M3 | Animation depth | Symbol library and edit-in-place, tween UI and graph editor, animation folders UI, camera tool, effects on GPU (blur/glow/shadow) |
| M4 | Audio + export | Audio import/record, waveform, scrub, lip sync editor + on-device phoneme analyzer, MP4/GIF/PNG/ProRes export with audio |
| M5 | Mac app | Same core and Metal renderer, AppKit/SwiftUI shell, keyboard-first workflows |
| M6 | AI in-betweening (paid) | Cloud provider, StoreKit 2 subscriptions, server receipt check, progress/cancel UI |
| M7 | Windows app | WinUI 3 or Qt shell, D3D12 renderer, Windows Ink |
| M8 | 3D / Blender | glTF/USD layers, camera sync, Blender round-trip |
