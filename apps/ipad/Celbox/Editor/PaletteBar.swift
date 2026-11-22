import SwiftUI
import UIKit

/// Document palette. Strokes reference a palette entry, so editing a color
/// here recolors that color on every frame.
struct PaletteBar: View {
    @ObservedObject var document: CelboxDocument
    var undoManager: UndoManager?

    private var core: CoreSession { document.core }

    var body: some View {
        let swatches: [CoreSession.Swatch] = { _ = document.revision; return core.palette() }()
        let current = core.currentStyle
        HStack(spacing: 6) {
            ForEach(swatches) { s in
                Circle()
                    .fill(Color(red: Double(s.r), green: Double(s.g), blue: Double(s.b), opacity: Double(s.a)))
                    .frame(width: 24, height: 24)
                    .overlay(Circle().stroke(Color.white, lineWidth: s.id == current ? 2.5 : 0.5))
                    .onTapGesture {
                        core.currentStyle = s.id
                        document.changed()
                    }
                    .accessibilityLabel("Color")
                    .accessibilityAddTraits(s.id == current ? .isSelected : [])
            }
            ColorPicker("Edit color", selection: colorBinding(swatches.first { $0.id == current }), supportsOpacity: true)
                .labelsHidden()
                .help("Edit the selected color everywhere it is used")
            Button {
                let c = swatches.first { $0.id == current }
                document.edit(undoManager, "Add Color") { core in
                    let id = core.addStyle(r: c?.r ?? 0, g: c?.g ?? 0, b: c?.b ?? 0, a: c?.a ?? 1)
                    core.currentStyle = id
                    return id != 0
                }
            } label: { Image(systemName: "plus.circle") }
            .accessibilityLabel("Add color")
        }
    }

    private func colorBinding(_ swatch: CoreSession.Swatch?) -> Binding<Color> {
        Binding(
            get: {
                guard let s = swatch else { return .black }
                return Color(red: Double(s.r), green: Double(s.g), blue: Double(s.b), opacity: Double(s.a))
            },
            set: { newColor in
                guard let s = swatch else { return }
                var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 1
                UIColor(newColor).getRed(&r, green: &g, blue: &b, alpha: &a)
                document.edit(undoManager, "Change Color") {
                    $0.setStyleColor(s.id, r: Float(r), g: Float(g), b: Float(b), a: Float(a))
                }
            })
    }
}
