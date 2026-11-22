import MetalKit
import SwiftUI
import UIKit

/// Editor view transform (canvas pixels -> view points). Kept outside the
/// document: it is per-window state, not saved in the file.
final class CanvasViewport {
    var zoom: CGFloat = 1
    var rotation: CGFloat = 0   // radians
    var offset: CGPoint = .zero // view-space position of the canvas center
    var fitted = false

    func transform(canvasSize: CGSize) -> CGAffineTransform {
        CGAffineTransform.identity
            .translatedBy(x: offset.x, y: offset.y)
            .rotated(by: rotation)
            .scaledBy(x: zoom, y: zoom)
            .translatedBy(x: -canvasSize.width / 2, y: -canvasSize.height / 2)
    }

    func fit(canvasSize: CGSize, in bounds: CGSize) {
        guard bounds.width > 0, bounds.height > 0 else { return }
        zoom = min(bounds.width / canvasSize.width, bounds.height / canvasSize.height) * 0.92
        rotation = 0
        offset = CGPoint(x: bounds.width / 2, y: bounds.height / 2)
        fitted = true
    }
}

/// Metal canvas with Apple Pencil drawing and finger gestures:
/// - Pencil draws (fingers draw too when `allowFingerDrawing` is on)
/// - one/two-finger pan, pinch zoom, rotate
/// - two-finger tap = undo, three-finger tap = redo
/// - Pencil double-tap / squeeze toggles the eraser
final class CanvasMTKView: MTKView, UIPencilInteractionDelegate {
    var document: CelboxDocument?
    var undoManagerProvider: (() -> UndoManager?)?
    var allowFingerDrawing = false {
        didSet { panGesture?.minimumNumberOfTouches = allowFingerDrawing ? 2 : 1 }
    }
    private weak var panGesture: UIPanGestureRecognizer?
    var throughCamera = false
    var onionSkin = true
    var displayFrame: (() -> Int)?
    var onToolToggle: (() -> Void)?
    let viewport = CanvasViewport()

    private var renderer: CanvasRenderer?
    private var drawingTouch: UITouch?

    init() {
        super.init(frame: .zero, device: MTLCreateSystemDefaultDevice())
        colorPixelFormat = .bgra8Unorm
        sampleCount = CanvasRenderer.sampleCount
        clearColor = MTLClearColor(red: 0.16, green: 0.17, blue: 0.19, alpha: 1)
        enableSetNeedsDisplay = true
        isPaused = true
        isMultipleTouchEnabled = true
        renderer = CanvasRenderer(view: self)
        delegate = renderer
        renderer?.drawFrame = { [weak self] body, pixelSize in self?.provideFrame(body, pixelSize) }
        installGestures()
        let pencil = UIPencilInteraction()
        pencil.delegate = self
        addInteraction(pencil)
    }

    @available(*, unavailable)
    required init(coder: NSCoder) { fatalError("init(coder:) is not supported") }

    private var canvasSize: CGSize {
        guard let s = document?.core.settings else { return CGSize(width: 1920, height: 1080) }
        return CGSize(width: CGFloat(s.canvasWidth), height: CGFloat(s.canvasHeight))
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        if !viewport.fitted { viewport.fit(canvasSize: canvasSize, in: bounds.size) }
        setNeedsDisplay()
    }

    func fitToScreen() {
        viewport.fit(canvasSize: canvasSize, in: bounds.size)
        setNeedsDisplay()
    }

    private func provideFrame(_ body: (UnsafeBufferPointer<CbxVertex>) -> Void, _ pixelSize: CGSize) {
        guard let doc = document else { return }
        let scale = bounds.width > 0 ? pixelSize.width / bounds.width : contentScaleFactor
        let m: CGAffineTransform
        if throughCamera {
            // Fit the movie frame to the view.
            let s = doc.core.settings
            let movie = CGSize(width: CGFloat(s.movieWidth), height: CGFloat(s.movieHeight))
            let z = min(bounds.width / movie.width, bounds.height / movie.height) * 0.95
            m = CGAffineTransform(translationX: (bounds.width - movie.width * z) / 2, y: (bounds.height - movie.height * z) / 2)
                .scaledBy(x: z, y: z)
        } else {
            m = viewport.transform(canvasSize: canvasSize)
        }
        let toPixels = m.concatenating(CGAffineTransform(scaleX: scale, y: scale))
        let frame = displayFrame?() ?? doc.core.currentFrame
        doc.core.withTessellation(frame: frame, canvasToScreen: toPixels, throughCamera: throughCamera,
                                  onion: onionSkin && !throughCamera, body)
    }

    // MARK: Drawing input

    private func canvasPoint(_ p: CGPoint) -> CGPoint {
        p.applying(viewport.transform(canvasSize: canvasSize).inverted())
    }

    private func pressure(_ t: UITouch) -> CGFloat {
        guard t.type == .pencil, t.maximumPossibleForce > 0 else { return 1 }
        return min(1, t.force / t.maximumPossibleForce)
    }

    private func canDraw(_ t: UITouch) -> Bool {
        !throughCamera && (t.type == .pencil || allowFingerDrawing)
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard drawingTouch == nil, let t = touches.first(where: canDraw), let doc = document else {
            super.touchesBegan(touches, with: event)
            return
        }
        drawingTouch = t
        doc.core.strokeBegin(canvasPoint(t.location(in: self)), pressure: pressure(t), time: t.timestamp)
        setNeedsDisplay()
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let t = drawingTouch, touches.contains(t), let doc = document else {
            super.touchesMoved(touches, with: event)
            return
        }
        // Coalesced touches give the full 240 Hz Pencil sample rate.
        for c in event?.coalescedTouches(for: t) ?? [t] {
            doc.core.strokeAdd(canvasPoint(c.location(in: self)), pressure: pressure(c), time: c.timestamp)
        }
        setNeedsDisplay()
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let t = drawingTouch, touches.contains(t), let doc = document else {
            super.touchesEnded(touches, with: event)
            return
        }
        drawingTouch = nil
        doc.edit(undoManagerProvider?(), doc.core.tool == .eraser ? "Erase" : "Brush Stroke") { $0.strokeEnd() }
        setNeedsDisplay()
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        if let t = drawingTouch, touches.contains(t) {
            drawingTouch = nil
            document?.core.strokeCancel()
            setNeedsDisplay()
        }
        super.touchesCancelled(touches, with: event)
    }

    // MARK: Gestures (fingers only)

    private func installGestures() {
        let finger = [NSNumber(value: UITouch.TouchType.direct.rawValue)]
        let pan = UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
        pan.allowedTouchTypes = finger
        pan.minimumNumberOfTouches = 1
        pan.maximumNumberOfTouches = 2
        panGesture = pan
        let pinch = UIPinchGestureRecognizer(target: self, action: #selector(handlePinch(_:)))
        pinch.allowedTouchTypes = finger
        let rotate = UIRotationGestureRecognizer(target: self, action: #selector(handleRotate(_:)))
        rotate.allowedTouchTypes = finger
        let undoTap = UITapGestureRecognizer(target: self, action: #selector(handleUndoTap))
        undoTap.numberOfTouchesRequired = 2
        undoTap.allowedTouchTypes = finger
        let redoTap = UITapGestureRecognizer(target: self, action: #selector(handleRedoTap))
        redoTap.numberOfTouchesRequired = 3
        redoTap.allowedTouchTypes = finger
        for g in [pan, pinch, rotate, undoTap, redoTap] as [UIGestureRecognizer] {
            g.delegate = gestureDelegate
            addGestureRecognizer(g)
        }
    }

    private lazy var gestureDelegate = SimultaneousGestures()

    @objc private func handlePan(_ g: UIPanGestureRecognizer) {
        let d = g.translation(in: self)
        viewport.offset.x += d.x
        viewport.offset.y += d.y
        g.setTranslation(.zero, in: self)
        setNeedsDisplay()
    }

    @objc private func handlePinch(_ g: UIPinchGestureRecognizer) {
        let anchor = g.location(in: self)
        let s = g.scale
        let newZoom = min(64, max(0.02, viewport.zoom * s))
        let applied = newZoom / viewport.zoom
        viewport.offset.x = anchor.x + (viewport.offset.x - anchor.x) * applied
        viewport.offset.y = anchor.y + (viewport.offset.y - anchor.y) * applied
        viewport.zoom = newZoom
        g.scale = 1
        setNeedsDisplay()
    }

    @objc private func handleRotate(_ g: UIRotationGestureRecognizer) {
        let anchor = g.location(in: self)
        let r = g.rotation
        let dx = viewport.offset.x - anchor.x, dy = viewport.offset.y - anchor.y
        viewport.offset = CGPoint(x: anchor.x + dx * cos(r) - dy * sin(r), y: anchor.y + dx * sin(r) + dy * cos(r))
        viewport.rotation += r
        g.rotation = 0
        setNeedsDisplay()
    }

    @objc private func handleUndoTap() { undoManagerProvider?()?.undo(); setNeedsDisplay() }
    @objc private func handleRedoTap() { undoManagerProvider?()?.redo(); setNeedsDisplay() }

    // MARK: Pencil double-tap / squeeze

    func pencilInteractionDidTap(_ interaction: UIPencilInteraction) {
        onToolToggle?()
    }
}

private final class SimultaneousGestures: NSObject, UIGestureRecognizerDelegate {
    func gestureRecognizer(_ g: UIGestureRecognizer, shouldRecognizeSimultaneouslyWith other: UIGestureRecognizer) -> Bool {
        !(g is UITapGestureRecognizer) && !(other is UITapGestureRecognizer)
    }
}

/// SwiftUI wrapper.
struct CanvasView: UIViewRepresentable {
    @ObservedObject var document: CelboxDocument
    var throughCamera: Bool
    var onionSkin: Bool
    var allowFingerDrawing: Bool
    var displayFrame: Int
    var fitRequest: Int
    var undoManager: UndoManager?
    var onToolToggle: () -> Void

    final class Coordinator {
        var lastFitRequest = 0
        var undoManager: UndoManager?
    }

    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeUIView(context: Context) -> CanvasMTKView {
        let v = CanvasMTKView()
        v.document = document
        let coordinator = context.coordinator
        v.undoManagerProvider = { [weak coordinator] in coordinator?.undoManager }
        return v
    }

    func updateUIView(_ v: CanvasMTKView, context: Context) {
        context.coordinator.undoManager = undoManager
        v.document = document
        v.throughCamera = throughCamera
        v.onionSkin = onionSkin
        v.allowFingerDrawing = allowFingerDrawing
        v.onToolToggle = onToolToggle
        let frame = displayFrame
        v.displayFrame = { frame }
        if context.coordinator.lastFitRequest != fitRequest {
            context.coordinator.lastFitRequest = fitRequest
            v.fitToScreen()
        }
        v.setNeedsDisplay()
    }
}
