import XCTest
@testable import Celbox

/// Smoke tests for the Swift <-> C++ boundary. The core's own tests live in
/// tests/test_main.cpp and run on every platform with CMake.
final class CoreSessionTests: XCTestCase {
    func testDrawAndSaveRoundTrip() throws {
        let a = CoreSession()
        a.strokeBegin(CGPoint(x: 10, y: 10), pressure: 0.5, time: 0)
        a.strokeAdd(CGPoint(x: 80, y: 40), pressure: 0.8, time: 0.02)
        XCTAssertTrue(a.strokeEnd())
        a.currentFrame = 5
        XCTAssertTrue(a.insertFrame())
        XCTAssertEqual(a.frameCount, 6)

        let files = a.save()
        XCTAssertNotNil(files["document.json"])

        let b = CoreSession()
        try b.load(files)
        XCTAssertEqual(b.frameCount, 6)
        XCTAssertEqual(b.rowStates(layer: b.currentLayer, count: 2), [UInt8(CBX_CELL_KEY.rawValue), UInt8(CBX_CELL_HOLD.rawValue)])
    }

    func testTessellationProducesTriangles() {
        let s = CoreSession()
        let count = s.withTessellation(frame: 0, canvasToScreen: .identity, throughCamera: false, onion: true) { $0.count }
        XCTAssertGreaterThan(count, 0)
        XCTAssertEqual(count % 3, 0)
    }

    func testBadPackageThrows() {
        let s = CoreSession()
        XCTAssertThrowsError(try s.load([:]))
    }
}
