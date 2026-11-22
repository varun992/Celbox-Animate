import SwiftUI

/// Horizontal, Animate-style timeline: layer names on the left, one row of
/// frame cells per layer, frame ruler and playhead on top.
struct TimelineView: View {
    @ObservedObject var document: CelboxDocument
    @Binding var playheadFrame: Int
    var isPlaying: Bool
    var onSelect: (_ layer: UInt64, _ frame: Int) -> Void
    var onToggleVisible: (_ layer: UInt64) -> Void
    var onToggleLock: (_ layer: UInt64) -> Void
    var onRename: (_ layer: UInt64, _ name: String) -> Void

    static let cellWidth: CGFloat = 22
    static let rowHeight: CGFloat = 34
    static let rulerHeight: CGFloat = 24
    static let headerWidth: CGFloat = 210

    @State private var renaming: CoreSession.LayerRow?
    @State private var renameText = ""

    private var rows: [CoreSession.LayerRow] {
        _ = document.revision
        return document.core.layers()
    }

    /// Show a comfortable amount past the last used frame.
    private var visibleFrames: Int { max(document.core.frameCount + 24, 60) }

    var body: some View {
        let rows = rows
        let frames = visibleFrames
        ScrollView(.vertical) {
            HStack(alignment: .top, spacing: 0) {
                VStack(spacing: 0) {
                    Color.clear.frame(height: Self.rulerHeight)
                    ForEach(rows) { row in layerHeader(row) }
                }
                .frame(width: Self.headerWidth)
                .background(Color(white: 0.13))

                ScrollView(.horizontal) {
                    VStack(alignment: .leading, spacing: 0) {
                        ruler(frames: frames)
                        ForEach(rows) { row in frameRow(row, frames: frames) }
                    }
                    .overlay(alignment: .topLeading) { playhead(rowsCount: rows.count) }
                }
            }
        }
        .background(Color(white: 0.1))
        .alert("Rename Layer", isPresented: Binding(get: { renaming != nil }, set: { if !$0 { renaming = nil } })) {
            TextField("Name", text: $renameText)
            Button("Rename") { if let r = renaming { onRename(r.id, renameText) }; renaming = nil }
            Button("Cancel", role: .cancel) { renaming = nil }
        }
    }

    // MARK: Pieces

    private func layerHeader(_ row: CoreSession.LayerRow) -> some View {
        let selected = row.id == document.core.currentLayer
        return HStack(spacing: 6) {
            Button { onToggleVisible(row.id) } label: {
                Image(systemName: row.visible ? "eye" : "eye.slash").frame(width: 22)
            }
            Button { onToggleLock(row.id) } label: {
                Image(systemName: row.locked ? "lock.fill" : "lock.open").frame(width: 18)
                    .opacity(row.locked ? 1 : 0.4)
            }
            Image(systemName: row.isFolder ? "folder" : "scribble.variable")
                .foregroundStyle(.secondary)
                .padding(.leading, CGFloat(row.depth) * 12)
            Text(row.name).lineLimit(1).font(.callout)
            Spacer(minLength: 0)
        }
        .buttonStyle(.plain)
        .foregroundStyle(.white)
        .padding(.horizontal, 8)
        .frame(height: Self.rowHeight)
        .background(selected ? Color.accentColor.opacity(0.35) : Color.clear)
        .contentShape(Rectangle())
        .onTapGesture { onSelect(row.id, playheadFrame) }
        .onLongPressGesture { renameText = row.name; renaming = row }
        .overlay(alignment: .bottom) { Divider().background(Color.black) }
    }

    private func ruler(frames: Int) -> some View {
        Canvas { ctx, size in
            for f in 0..<frames {
                let x = CGFloat(f) * Self.cellWidth
                let major = (f + 1) % 5 == 0 || f == 0
                var tick = Path()
                tick.move(to: CGPoint(x: x + Self.cellWidth / 2, y: size.height))
                tick.addLine(to: CGPoint(x: x + Self.cellWidth / 2, y: size.height - (major ? 8 : 4)))
                ctx.stroke(tick, with: .color(.gray), lineWidth: 1)
                if major {
                    ctx.draw(Text("\(f + 1)").font(.system(size: 9)).foregroundColor(.gray),
                             at: CGPoint(x: x + Self.cellWidth / 2, y: 8))
                }
            }
        }
        .frame(width: CGFloat(frames) * Self.cellWidth, height: Self.rulerHeight)
        .contentShape(Rectangle())
        .gesture(DragGesture(minimumDistance: 0).onChanged { v in
            let f = max(0, Int(v.location.x / Self.cellWidth))
            if f != playheadFrame { onSelect(document.core.currentLayer, f) }
        })
    }

    private func frameRow(_ row: CoreSession.LayerRow, frames: Int) -> some View {
        let states = row.isFolder ? [UInt8](repeating: 0, count: frames) : document.core.rowStates(layer: row.id, count: frames)
        let selectedLayer = row.id == document.core.currentLayer
        let current = playheadFrame
        return Canvas { ctx, size in
            let h = size.height
            for f in 0..<frames {
                let rect = CGRect(x: CGFloat(f) * Self.cellWidth, y: 0, width: Self.cellWidth, height: h)
                let state = CbxCellState(rawValue: UInt32(states[f]))
                let isUsed = state != CBX_CELL_EMPTY
                var fill = Color(white: (f + 1) % 5 == 0 ? 0.2 : 0.17)
                if isUsed { fill = Color(white: 0.32) }
                if selectedLayer && f == current { fill = Color.accentColor.opacity(0.6) }
                ctx.fill(Path(rect.insetBy(dx: 0.5, dy: 0.5)), with: .color(fill))
                let dot = CGRect(x: rect.midX - 4, y: h - 13, width: 8, height: 8)
                switch state {
                case CBX_CELL_KEY:
                    ctx.fill(Path(ellipseIn: dot), with: .color(.white))
                case CBX_CELL_BLANK_KEY:
                    ctx.stroke(Path(ellipseIn: dot), with: .color(.white), lineWidth: 1.2)
                default:
                    break
                }
                // End-of-span marker: a small bar on the last frame of a hold.
                if isUsed, f + 1 < frames {
                    let next = CbxCellState(rawValue: UInt32(states[f + 1]))
                    if next == CBX_CELL_EMPTY || next == CBX_CELL_KEY || next == CBX_CELL_BLANK_KEY,
                       state == CBX_CELL_HOLD || state == CBX_CELL_BLANK_HOLD {
                        ctx.fill(Path(CGRect(x: rect.midX - 3, y: h - 14, width: 6, height: 10)), with: .color(.white.opacity(0.8)))
                    }
                }
            }
        }
        .frame(width: CGFloat(frames) * Self.cellWidth, height: Self.rowHeight)
        .contentShape(Rectangle())
        .gesture(DragGesture(minimumDistance: 0).onChanged { v in
            let f = max(0, Int(v.location.x / Self.cellWidth))
            onSelect(row.id, f)
        })
    }

    private func playhead(rowsCount: Int) -> some View {
        Rectangle()
            .fill(Color.red)
            .frame(width: 2, height: Self.rulerHeight + CGFloat(rowsCount) * Self.rowHeight)
            .offset(x: CGFloat(playheadFrame) * Self.cellWidth + Self.cellWidth / 2 - 1)
            .allowsHitTesting(false)
            .animation(isPlaying ? nil : .easeOut(duration: 0.08), value: playheadFrame)
    }
}
