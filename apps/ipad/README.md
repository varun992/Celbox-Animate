# Celbox for iPad

SwiftUI + Metal app on top of the shared C++ core (`../../core`).

## Run it

1. Install Xcode 16 or later and [XcodeGen](https://github.com/yonaskolb/XcodeGen): `brew install xcodegen`
2. Generate the project and open it:
   ```sh
   cd apps/ipad
   xcodegen generate
   open Celbox.xcodeproj
   ```
3. In **Signing & Capabilities**, pick your team (a free Apple ID works for running on your own iPad).
4. Choose your iPad or an iPad simulator and press Run.

`Celbox.xcodeproj` is generated, so don't commit it. Edit `project.yml` instead and run `xcodegen generate` again.

## What works (M1)

- Document browser (Files app, iCloud Drive). Documents are `.celbox` packages and autosave.
- Metal canvas: draw with Apple Pencil (pressure, 240 Hz coalesced samples, stabilizer). There's a toggle to draw with a finger.
- Gestures: one- or two-finger pan, pinch to zoom, rotate, two-finger tap to undo, three-finger tap to redo. Pencil double-tap switches between brush and eraser.
- Brush and stroke eraser, brush size, and a document palette. Editing a color recolors it on every frame.
- Horizontal timeline: layers and folders, show/hide, lock, rename (long-press), reorder, delete.
- Frame tools: Frame (F5 / ⌘5), Key (F6 / ⌘6), Blank (F7 / ⌘7), duplicate, remove, and clear keyframe.
- Onion skin (2 before, 1 after), a camera frame overlay, and a camera view toggle.
- Playback at the document frame rate, with looping.
- Canvas and movie settings. The canvas can be larger than the movie frame.
- Undo and redo for every edit, connected to the system UndoManager.

Keyboard: `space` play/pause, `,` and `.` previous/next frame, `b` brush, `e` eraser.

## Layout

```
Celbox/
  App/        CelboxApp (DocumentGroup), CelboxDocument (package <-> core)
  Core/       CoreSession.swift (Swift wrapper over celbox_c.h), bridging header
  Canvas/     CanvasView (MTKView + Pencil + gestures), CanvasRenderer, Shaders.metal
  Timeline/   TimelineView
  Editor/     EditorView, PaletteBar, SettingsSheet
CelboxTests/  Swift <-> core smoke tests
```
