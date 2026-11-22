import SwiftUI

@main
struct CelboxApp: App {
    var body: some Scene {
        DocumentGroup(newDocument: { CelboxDocument() }) { file in
            EditorView(document: file.document)
        }
    }
}
