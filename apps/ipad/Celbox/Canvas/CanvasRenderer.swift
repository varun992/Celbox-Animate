import MetalKit

/// Draws the triangles produced by the core's tessellator. All geometry
/// decisions live in the core so Mac/Windows renderers match exactly.
final class CanvasRenderer: NSObject, MTKViewDelegate {
    let device: MTLDevice
    private let queue: MTLCommandQueue
    private let pipeline: MTLRenderPipelineState

    /// Supplies the frame to draw: called on the main thread for each redraw.
    var drawFrame: ((_ body: (UnsafeBufferPointer<CbxVertex>) -> Void, _ pixelSize: CGSize) -> Void)?
    static let sampleCount = 4

    init?(view: MTKView) {
        guard let device = view.device ?? MTLCreateSystemDefaultDevice(),
              let queue = device.makeCommandQueue(),
              let library = device.makeDefaultLibrary() else { return nil }
        self.device = device
        self.queue = queue

        let desc = MTLRenderPipelineDescriptor()
        desc.vertexFunction = library.makeFunction(name: "celbox_vertex")
        desc.fragmentFunction = library.makeFunction(name: "celbox_fragment")
        desc.rasterSampleCount = Self.sampleCount
        let color = desc.colorAttachments[0]!
        color.pixelFormat = view.colorPixelFormat
        color.isBlendingEnabled = true
        color.rgbBlendOperation = .add
        color.alphaBlendOperation = .add
        color.sourceRGBBlendFactor = .one
        color.sourceAlphaBlendFactor = .one
        color.destinationRGBBlendFactor = .oneMinusSourceAlpha
        color.destinationAlphaBlendFactor = .oneMinusSourceAlpha
        guard let pipeline = try? device.makeRenderPipelineState(descriptor: desc) else { return nil }
        self.pipeline = pipeline
        super.init()
    }

    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

    func draw(in view: MTKView) {
        guard let pass = view.currentRenderPassDescriptor,
              let drawable = view.currentDrawable,
              let cmd = queue.makeCommandBuffer(),
              let enc = cmd.makeRenderCommandEncoder(descriptor: pass) else { return }
        enc.setRenderPipelineState(pipeline)
        var viewport = SIMD2<Float>(Float(view.drawableSize.width), Float(view.drawableSize.height))

        drawFrame?({ vertices in
            guard vertices.count > 0, let base = vertices.baseAddress else { return }
            // A fresh buffer per frame keeps the CPU from overwriting geometry the
            // GPU is still reading. (A ring of buffers is the M2 optimization.)
            let bytes = vertices.count * MemoryLayout<CbxVertex>.stride
            guard let buf = device.makeBuffer(bytes: base, length: bytes, options: .storageModeShared) else { return }
            enc.setVertexBuffer(buf, offset: 0, index: 0)
            enc.setVertexBytes(&viewport, length: MemoryLayout<SIMD2<Float>>.stride, index: 1)
            enc.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: vertices.count)
        }, view.drawableSize)

        enc.endEncoding()
        cmd.present(drawable)
        cmd.commit()
    }
}
