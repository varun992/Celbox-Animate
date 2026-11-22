import SwiftUI

/// Main editing screen: tool bar, Metal canvas, timeline.
struct EditorView: View {
    @ObservedObject var document: CelboxDocument
    @Environment(\.undoManager) private var undoManager

    @State private var tool: CoreSession.Tool = .brush
    @State private var brushSize: Double = 6
    @State private var onionSkin = true
    @State private var throughCamera = false
    @State private var allowFingerDrawing = false
    @State private var isPlaying = false
    @State private var loopPlayback = true
    @State private var playbackFrame = 0
    @State private var playTimer: Timer?
    @State private var fitRequest = 0
    @State private var showSettings = false
    @State private var timelineHeight: CGFloat = 230

    private var core: CoreSession { document.core }

    var body: some View {
        VStack(spacing: 0) {
            toolBar
            CanvasView(document: document,
                       throughCamera: throughCamera || isPlaying,
                       onionSkin: onionSkin && !isPlaying,
                       allowFingerDrawing: allowFingerDrawing,
                       displayFrame: isPlaying ? playbackFrame : core.currentFrame,
                       fitRequest: fitRequest,
                       undoManager: undoManager,
                       onToolToggle: { setTool(tool == .brush ? .eraser : .brush) })
                .ignoresSafeArea(.container, edges: .horizontal)
            timelineBar
            TimelineView(document: document,
                         playheadFrame: Binding(get: { isPlaying ? playbackFrame : core.currentFrame },
                                                set: { core.currentFrame = $0; document.changed() }),
                         isPlaying: isPlaying,
                         onSelect: { layer, frame in
                             stop()
                             core.currentLayer = layer
                             core.currentFrame = frame
                             document.changed()
                         },
                         onToggleVisible: { id in
                             let row = core.layers().first { $0.id == id }
                             edit("Show/Hide Layer") { $0.setVisible(id, !(row?.visible ?? true)) }
                         },
                         onToggleLock: { id in
                             let row = core.layers().first { $0.id == id }
                             edit("Lock Layer") { $0.setLocked(id, !(row?.locked ?? false)) }
                         },
                         onRename: { id, name in edit("Rename Layer") { $0.renameLayer(id, name) } })
                .frame(height: timelineHeight)
        }
        .background(Color(white: 0.16))
        .navigationBarTitleDisplayMode(.inline)
        .sheet(isPresented: $showSettings) {
            SettingsSheet(document: document, undoManager: undoManager) { fitRequest += 1 }
        }
        .onAppear {
            core.brushSize = Float(brushSize)
            core.setOnion(before: 2, after: 1, opacity: 0.35)
        }
        .onDisappear { stop() }
        // Hardware keyboard shortcuts (Animate-style F5/F6/F7 live on the timeline bar).
        .background(shortcuts)
    }

    // MARK: Top bar

    private var toolBar: some View {
        HStack(spacing: 14) {
            Picker("Tool", selection: Binding(get: { tool }, set: { setTool($0) })) {
                Image(systemName: "paintbrush.pointed").tag(CoreSession.Tool.brush)
                Image(systemName: "eraser").tag(CoreSession.Tool.eraser)
            }
            .pickerStyle(.segmented)
            .frame(width: 110)

            HStack(spacing: 6) {
                Image(systemName: "circle.fill").font(.system(size: 6))
                Slider(value: $brushSize, in: 1...80)
                    .frame(width: 140)
                    .onChange(of: brushSize) { _, v in core.brushSize = Float(v) }
                Image(systemName: "circle.fill").font(.system(size: 14))
            }

            PaletteBar(document: document, undoManager: undoManager)

            Spacer()

            Toggle(isOn: $allowFingerDrawing) { Image(systemName: "hand.draw") }
                .toggleStyle(.button)
                .accessibilityLabel("Draw with finger")
            Button { fitRequest += 1 } label: { Image(systemName: "arrow.up.left.and.down.right.magnifyingglass") }
                .accessibilityLabel("Fit canvas")
            Button { undoManager?.undo() } label: { Image(systemName: "arrow.uturn.backward") }
                .disabled(!(undoManager?.canUndo ?? false))
            Button { undoManager?.redo() } label: { Image(systemName: "arrow.uturn.forward") }
                .disabled(!(undoManager?.canRedo ?? false))
            Button { showSettings = true } label: { Image(systemName: "gearshape") }
                .accessibilityLabel("Canvas and movie settings")
        }
        .font(.title3)
        .padding(.horizontal, 16)
        .padding(.vertical, 8)
        .background(.bar)
    }

    // MARK: Timeline bar

    private var timelineBar: some View {
        HStack(spacing: 12) {
            Button { togglePlay() } label: { Image(systemName: isPlaying ? "pause.fill" : "play.fill") }
                .keyboardShortcut(.space, modifiers: [])
            Toggle(isOn: $loopPlayback) { Image(systemName: "repeat") }.toggleStyle(.button)
            Text("\((isPlaying ? playbackFrame : core.currentFrame) + 1) / \(core.frameCount)")
                .monospacedDigit().font(.callout).frame(width: 80)
            Divider().frame(height: 20)

            Button("Frame") { edit("Insert Frame") { $0.insertFrame() } }
                .help("Insert frame (F5)")
            Button("Key") { edit("Insert Keyframe") { $0.insertKeyframe() } }
                .help("Insert keyframe (F6)")
            Button("Blank") { edit("Insert Blank Keyframe") { $0.insertBlankKeyframe() } }
                .help("Insert blank keyframe (F7)")
            Button { edit("Duplicate Frame") { $0.duplicateFrame() } } label: { Image(systemName: "plus.square.on.square") }
                .accessibilityLabel("Duplicate frame")
            Button { edit("Remove Frame") { $0.removeFrame() } } label: { Image(systemName: "minus.square") }
                .accessibilityLabel("Remove frame")
            Button { edit("Clear Keyframe") { $0.clearKeyframe() } } label: { Image(systemName: "circle.slash") }
                .accessibilityLabel("Clear keyframe")
            Divider().frame(height: 20)

            Toggle(isOn: $onionSkin) { Image(systemName: "square.3.layers.3d.down.right") }
                .toggleStyle(.button).accessibilityLabel("Onion skin")
            Toggle(isOn: $throughCamera) { Image(systemName: "video") }
                .toggleStyle(.button).accessibilityLabel("Camera view")

            Spacer()

            Menu {
                Button { edit("Add Layer") { $0.addLayer(CBX_LAYER_VECTOR) != 0 } } label: { Label("Vector Layer", systemImage: "scribble.variable") }
                Button { edit("Add Folder") { $0.addLayer(CBX_LAYER_FOLDER) != 0 } } label: { Label("Folder", systemImage: "folder") }
            } label: { Image(systemName: "plus.rectangle.on.rectangle") }
            Button { let id = core.currentLayer; edit("Move Layer Up") { $0.moveLayer(id, by: 1) } } label: { Image(systemName: "arrow.up") }
            Button { let id = core.currentLayer; edit("Move Layer Down") { $0.moveLayer(id, by: -1) } } label: { Image(systemName: "arrow.down") }
            Button(role: .destructive) { let id = core.currentLayer; edit("Delete Layer") { $0.deleteLayer(id) } } label: { Image(systemName: "trash") }
        }
        .buttonStyle(.bordered)
        .controlSize(.small)
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(Color(white: 0.12))
        .foregroundStyle(.white)
    }

    /// Invisible buttons that carry keyboard shortcuts.
    private var shortcuts: some View {
        Group {
            Button("") { edit("Insert Frame") { $0.insertFrame() } }.keyboardShortcut("5", modifiers: .command)
            Button("") { edit("Insert Keyframe") { $0.insertKeyframe() } }.keyboardShortcut("6", modifiers: .command)
            Button("") { edit("Insert Blank Keyframe") { $0.insertBlankKeyframe() } }.keyboardShortcut("7", modifiers: .command)
            Button("") { step(-1) }.keyboardShortcut(",", modifiers: [])
            Button("") { step(1) }.keyboardShortcut(".", modifiers: [])
            Button("") { setTool(.brush) }.keyboardShortcut("b", modifiers: [])
            Button("") { setTool(.eraser) }.keyboardShortcut("e", modifiers: [])
        }
        .opacity(0)
        .allowsHitTesting(false)
    }

    // MARK: Actions

    private func edit(_ name: String, _ action: (CoreSession) -> Bool) {
        stop()
        document.edit(undoManager, name, action)
    }

    private func setTool(_ t: CoreSession.Tool) {
        tool = t
        core.tool = t
    }

    private func step(_ d: Int) {
        stop()
        core.currentFrame = max(0, core.currentFrame + d)
        document.changed()
    }

    private func togglePlay() { isPlaying ? stop() : play() }

    private func play() {
        let fps = max(1, core.settings.fps)
        let count = max(1, core.frameCount)
        playbackFrame = core.currentFrame >= count - 1 ? 0 : core.currentFrame
        isPlaying = true
        playTimer?.invalidate()
        let timer = Timer(timeInterval: 1.0 / fps, repeats: true) { _ in
            let next = playbackFrame + 1
            if next >= count {
                if loopPlayback { playbackFrame = 0 } else { stop() }
            } else {
                playbackFrame = next
            }
        }
        RunLoop.main.add(timer, forMode: .common)
        playTimer = timer
    }

    private func stop() {
        guard isPlaying else { return }
        playTimer?.invalidate()
        playTimer = nil
        isPlaying = false
    }
}
