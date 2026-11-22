import SwiftUI
import UniformTypeIdentifiers

extension UTType {
    static let celbox = UTType(exportedAs: "com.illuminartstudios.celbox", conformingTo: .package)
}

/// A `.celbox` document. The model lives in the C++ core; this class adapts it
/// to SwiftUI's document system (Files app, iCloud Drive, autosave, undo).
final class CelboxDocument: ReferenceFileDocument {
    typealias Snapshot = [String: Data]

    static var readableContentTypes: [UTType] { [.celbox] }

    let core: CoreSession

    /// Bumped after every change so SwiftUI views and the canvas refresh.
    @Published private(set) var revision: UInt64 = 0

    init() {
        core = CoreSession()
        revision = core.revision
    }

    required init(configuration: ReadConfiguration) throws {
        core = CoreSession()
        var files: [String: Data] = [:]
        Self.collect(configuration.file, prefix: "", into: &files)
        try core.load(files)
        revision = core.revision
    }

    func snapshot(contentType: UTType) throws -> Snapshot { core.save() }

    func fileWrapper(snapshot: Snapshot, configuration: WriteConfiguration) throws -> FileWrapper {
        Self.directoryWrapper(for: snapshot)
    }

    // MARK: Change tracking + undo

    func changed() {
        revision = core.revision
    }

    /// Runs a core edit; if it changed the document, registers it with the
    /// system UndoManager (so the document autosaves and undo gestures work).
    @discardableResult
    func edit(_ undoManager: UndoManager?, _ name: String, _ action: (CoreSession) -> Bool) -> Bool {
        guard action(core) else { return false }
        registerUndo(undoManager, name)
        changed()
        return true
    }

    private func registerUndo(_ um: UndoManager?, _ name: String) {
        guard let um else { return }
        um.registerUndo(withTarget: self) { doc in
            if doc.core.undo() { doc.changed() }
            doc.registerRedo(um, name)
        }
        um.setActionName(name)
    }

    private func registerRedo(_ um: UndoManager, _ name: String) {
        um.registerUndo(withTarget: self) { doc in
            if doc.core.redo() { doc.changed() }
            doc.registerUndo(um, name)
        }
        um.setActionName(name)
    }

    // MARK: Package <-> FileWrapper

    private static func collect(_ wrapper: FileWrapper, prefix: String, into files: inout [String: Data]) {
        if wrapper.isDirectory {
            for (name, child) in wrapper.fileWrappers ?? [:] {
                collect(child, prefix: prefix.isEmpty ? name : "\(prefix)/\(name)", into: &files)
            }
        } else if wrapper.isRegularFile, let data = wrapper.regularFileContents {
            files[prefix] = data
        }
    }

    private static func directoryWrapper(for files: [String: Data]) -> FileWrapper {
        // Build a nested tree from "a/b/c.json" style paths.
        final class Node { var files: [String: Data] = [:]; var dirs: [String: Node] = [:] }
        let root = Node()
        for (path, data) in files {
            var parts = path.split(separator: "/").map(String.init)
            guard let fileName = parts.popLast() else { continue }
            var node = root
            for p in parts {
                if let next = node.dirs[p] { node = next } else { let n = Node(); node.dirs[p] = n; node = n }
            }
            node.files[fileName] = data
        }
        func wrap(_ n: Node) -> FileWrapper {
            var children: [String: FileWrapper] = [:]
            for (name, data) in n.files {
                let w = FileWrapper(regularFileWithContents: data)
                w.preferredFilename = name
                children[name] = w
            }
            for (name, d) in n.dirs {
                let w = wrap(d)
                w.preferredFilename = name
                children[name] = w
            }
            return FileWrapper(directoryWithFileWrappers: children)
        }
        return wrap(root)
    }
}
