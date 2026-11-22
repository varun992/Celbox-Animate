import SwiftUI

/// Canvas and movie settings. The canvas can be larger than the movie frame so
/// the camera can pan across it.
struct SettingsSheet: View {
    @ObservedObject var document: CelboxDocument
    var undoManager: UndoManager?
    var onApplied: () -> Void
    @Environment(\.dismiss) private var dismiss

    @State private var canvasW = 3840
    @State private var canvasH = 2160
    @State private var movieW = 1920
    @State private var movieH = 1080
    @State private var fps = 24.0
    @State private var fixedLength = false
    @State private var frameCount = 48
    @State private var background = Color.white

    private struct Preset: Hashable { let name: String; let w: Int; let h: Int }
    private let presets = [Preset(name: "HD 1080p", w: 1920, h: 1080), Preset(name: "4K UHD", w: 3840, h: 2160),
                           Preset(name: "Square 1080", w: 1080, h: 1080), Preset(name: "Vertical 1080×1920", w: 1080, h: 1920)]

    var body: some View {
        NavigationStack {
            Form {
                Section("Movie (export frame)") {
                    Menu("Presets") {
                        ForEach(presets, id: \.self) { p in
                            Button(p.name) {
                                movieW = p.w; movieH = p.h
                                canvasW = max(canvasW, p.w); canvasH = max(canvasH, p.h)
                            }
                        }
                    }
                    Stepper("Width: \(movieW) px", value: $movieW, in: 16...8192, step: 16)
                    Stepper("Height: \(movieH) px", value: $movieH, in: 16...8192, step: 16)
                    Picker("Frame rate", selection: $fps) {
                        ForEach([8.0, 12.0, 15.0, 23.976, 24.0, 25.0, 30.0, 60.0], id: \.self) { Text(String(format: "%g fps", $0)).tag($0) }
                    }
                    Toggle("Fixed length", isOn: $fixedLength)
                    if fixedLength {
                        Stepper("Length: \(frameCount) frames (\(String(format: "%.1f", Double(frameCount) / fps)) s)",
                                value: $frameCount, in: 1...100_000)
                    }
                }
                Section {
                    Stepper("Width: \(canvasW) px", value: $canvasW, in: 16...16384, step: 16)
                    Stepper("Height: \(canvasH) px", value: $canvasH, in: 16...16384, step: 16)
                    Button("Match movie size") { canvasW = movieW; canvasH = movieH }
                    Button("Double the movie width (for pans)") { canvasW = movieW * 2; canvasH = movieH }
                    ColorPicker("Background", selection: $background, supportsOpacity: false)
                } header: {
                    Text("Canvas (drawing area)")
                } footer: {
                    Text("Make the canvas bigger than the movie to move the camera across a wide background.")
                }
            }
            .navigationTitle("Canvas & Movie")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Cancel") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) { Button("Apply") { apply(); dismiss() } }
            }
        }
        .onAppear(perform: load)
    }

    private func load() {
        let s = document.core.settings
        canvasW = Int(s.canvasWidth); canvasH = Int(s.canvasHeight)
        movieW = Int(s.movieWidth); movieH = Int(s.movieHeight)
        fps = s.fps
        fixedLength = s.frameCount > 0
        frameCount = s.frameCount > 0 ? Int(s.frameCount) : max(1, document.core.frameCount)
        background = Color(red: Double(s.backgroundR), green: Double(s.backgroundG), blue: Double(s.backgroundB))
    }

    private func apply() {
        var r: CGFloat = 1, g: CGFloat = 1, b: CGFloat = 1, a: CGFloat = 1
        UIColor(background).getRed(&r, green: &g, blue: &b, alpha: &a)
        let s = CbxSettings(canvasWidth: Int32(canvasW), canvasHeight: Int32(canvasH),
                            movieWidth: Int32(movieW), movieHeight: Int32(movieH),
                            fps: fps, frameCount: fixedLength ? Int32(frameCount) : 0,
                            backgroundR: Float(r), backgroundG: Float(g), backgroundB: Float(b))
        document.edit(undoManager, "Change Settings") { core in
            core.settings = s
            return true
        }
        onApplied()
    }
}
