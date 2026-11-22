import Foundation
import CoreGraphics

/// Swift wrapper around the shared C++ core (`celbox_c.h`).
/// Owns one `CbxSession`. Main-thread only, like the core session.
final class CoreSession {
    let handle: OpaquePointer

    init() {
        guard let h = cbx_session_new() else { fatalError("Celbox core failed to start") }
        handle = h
    }

    deinit { cbx_session_free(handle) }

    var revision: UInt64 { cbx_revision(handle) }

    // MARK: Settings

    var settings: CbxSettings {
        get { cbx_get_settings(handle) }
        set { cbx_set_settings(handle, newValue) }
    }

    // MARK: Layers

    struct LayerRow: Identifiable, Equatable {
        let id: UInt64
        let depth: Int
        let kind: Int32
        let visible: Bool
        let locked: Bool
        let isFolder: Bool
        let name: String
    }

    func layers() -> [LayerRow] {
        let n = cbx_layer_count(handle)
        return (0..<n).map { i in
            let info = cbx_layer_info(handle, i)
            let name = String(cString: cbx_layer_name(handle, i))
            return LayerRow(id: info.id, depth: Int(info.depth), kind: info.kind,
                            visible: info.visible, locked: info.locked, isFolder: info.isFolder, name: name)
        }
    }

    @discardableResult func addLayer(_ kind: CbxLayerKind) -> UInt64 { cbx_add_layer(handle, kind) }
    @discardableResult func deleteLayer(_ id: UInt64) -> Bool { cbx_delete_layer(handle, id) }
    @discardableResult func renameLayer(_ id: UInt64, _ name: String) -> Bool { cbx_rename_layer(handle, id, name) }
    @discardableResult func setVisible(_ id: UInt64, _ v: Bool) -> Bool { cbx_set_layer_visible(handle, id, v) }
    @discardableResult func setLocked(_ id: UInt64, _ v: Bool) -> Bool { cbx_set_layer_locked(handle, id, v) }
    @discardableResult func moveLayer(_ id: UInt64, by delta: Int32) -> Bool { cbx_move_layer(handle, id, delta) }

    var currentLayer: UInt64 {
        get { cbx_current_layer(handle) }
        set { cbx_set_current_layer(handle, newValue) }
    }

    // MARK: Frames

    var frameCount: Int { Int(cbx_frame_count(handle)) }

    var currentFrame: Int {
        get { Int(cbx_current_frame(handle)) }
        set { cbx_set_current_frame(handle, Int32(max(0, newValue))) }
    }

    func rowStates(layer: UInt64, count: Int) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: max(0, count))
        out.withUnsafeMutableBufferPointer { buf in
            cbx_row_states(handle, layer, Int32(buf.count), buf.baseAddress)
        }
        return out
    }

    @discardableResult func insertFrame() -> Bool { cbx_insert_frame(handle) }
    @discardableResult func insertKeyframe() -> Bool { cbx_insert_keyframe(handle) }
    @discardableResult func insertBlankKeyframe() -> Bool { cbx_insert_blank_keyframe(handle) }
    @discardableResult func duplicateFrame() -> Bool { cbx_duplicate_frame(handle) }
    @discardableResult func removeFrame() -> Bool { cbx_remove_frame(handle) }
    @discardableResult func clearKeyframe() -> Bool { cbx_clear_keyframe(handle) }

    // MARK: Drawing

    enum Tool { case brush, eraser }

    var tool: Tool = .brush {
        didSet { cbx_set_tool(handle, tool == .eraser ? CBX_TOOL_ERASER : CBX_TOOL_BRUSH) }
    }

    var brushSize: Float {
        get { cbx_brush_size(handle) }
        set { cbx_set_brush_size(handle, newValue) }
    }

    func setStabilizer(lazyRadius: Float, smoothing: Int) {
        cbx_set_stabilizer(handle, lazyRadius, Int32(smoothing))
    }

    func strokeBegin(_ p: CGPoint, pressure: CGFloat, time: TimeInterval) {
        cbx_stroke_begin(handle, Float(p.x), Float(p.y), Float(pressure), time)
    }
    func strokeAdd(_ p: CGPoint, pressure: CGFloat, time: TimeInterval) {
        cbx_stroke_add(handle, Float(p.x), Float(p.y), Float(pressure), time)
    }
    @discardableResult func strokeEnd() -> Bool { cbx_stroke_end(handle) }
    func strokeCancel() { cbx_stroke_cancel(handle) }

    // MARK: Palette

    struct Swatch: Identifiable, Equatable {
        let id: UInt64
        let r, g, b, a: Float
    }

    func palette() -> [Swatch] {
        (0..<cbx_style_count(handle)).map { i in
            let s = cbx_style_info(handle, i)
            return Swatch(id: s.id, r: s.r, g: s.g, b: s.b, a: s.a)
        }
    }
    @discardableResult func addStyle(r: Float, g: Float, b: Float, a: Float = 1) -> UInt64 { cbx_add_style(handle, r, g, b, a) }
    @discardableResult func setStyleColor(_ id: UInt64, r: Float, g: Float, b: Float, a: Float = 1) -> Bool {
        cbx_set_style_color(handle, id, r, g, b, a)
    }
    var currentStyle: UInt64 {
        get { cbx_current_style(handle) }
        set { cbx_set_current_style(handle, newValue) }
    }

    func setOnion(before: Int, after: Int, opacity: Float) {
        cbx_set_onion(handle, Int32(before), Int32(after), opacity)
    }

    // MARK: Undo

    @discardableResult func undo() -> Bool { cbx_undo(handle) }
    @discardableResult func redo() -> Bool { cbx_redo(handle) }

    // MARK: Render

    /// Tessellates `frame` and hands the triangle vertices to `body` (valid only inside it).
    func withTessellation<R>(frame: Int, canvasToScreen m: CGAffineTransform, throughCamera: Bool, onion: Bool,
                             _ body: (UnsafeBufferPointer<CbxVertex>) -> R) -> R {
        let affine = CbxAffine(a: Float(m.a), b: Float(m.b), c: Float(m.c), d: Float(m.d), tx: Float(m.tx), ty: Float(m.ty))
        let count = Int(cbx_tessellate(handle, Int32(frame), affine, throughCamera, onion))
        return body(UnsafeBufferPointer(start: cbx_vertices(handle), count: count))
    }

    // MARK: Persistence

    /// Package files keyed by relative path ("document.json", "drawings/12.json", ...).
    func save() -> [String: Data] {
        var files: [String: Data] = [:]
        let n = cbx_save_begin(handle)
        for i in 0..<n {
            var len = 0
            let path = String(cString: cbx_save_path(handle, i))
            if let ptr = cbx_save_data(handle, i, &len) {
                files[path] = Data(bytes: ptr, count: len)
            } else {
                files[path] = Data()
            }
        }
        cbx_save_end(handle)
        return files
    }

    struct LoadError: LocalizedError {
        let message: String
        var errorDescription: String? { message }
    }

    func load(_ files: [String: Data]) throws {
        cbx_load_begin(handle)
        for (path, data) in files {
            data.withUnsafeBytes { raw in
                cbx_load_add(handle, path, raw.bindMemory(to: UInt8.self).baseAddress, raw.count)
            }
        }
        if !cbx_load_finish(handle) {
            throw LoadError(message: String(cString: cbx_last_error(handle)))
        }
    }
}
