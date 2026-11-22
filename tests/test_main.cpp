// Dependency-free test runner for the core.
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "anim/ai.h"
#include "anim/assist.h"
#include "anim/brush.h"
#include "anim/edit.h"
#include "anim/evaluate.h"
#include "anim/ops.h"

using namespace anim;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++g_checks;                                                                       \
        if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)
#define NEAR(a, b) CHECK(std::abs(double(a) - double(b)) < 1e-3)

struct TestCase { const char* name; std::function<void()> fn; };
static std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
struct Reg { Reg(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()

static VectorDrawing line(Vec2 a, Vec2 b, StyleId style = {}, ObjectId obj = {}) {
    VectorDrawing d;
    d.strokes.push_back({{{a, 1}, {b, 1}}, style, 4, obj, false});
    return d;
}

TEST(easing_curves) {
    NEAR(Easing::linear().apply(0.25f), 0.25f);
    NEAR(Easing::hold().apply(0.9f), 0.0f);
    NEAR(Easing::easeInOut().apply(0.5f), 0.5f);
    CHECK(Easing::easeIn().apply(0.25f) < 0.25f);
    CHECK(Easing::easeOut().apply(0.25f) > 0.25f);
}

TEST(track_interpolation) {
    AnimTrack<float> t{5.0f};
    NEAR(t.valueAt(10), 5.0f);
    t.setKey(0, 0.0f);
    t.setKey(10, 100.0f);
    NEAR(t.valueAt(5), 50.0f);
    NEAR(t.valueAt(-3), 0.0f);
    NEAR(t.valueAt(20), 100.0f);
    t.setKey(0, 0.0f, Easing::hold());
    NEAR(t.valueAt(9), 0.0f);
    t.shiftKeys(5, 10);
    CHECK(t.hasKey(20) && !t.hasKey(10));
}

TEST(keyframes_frames_and_duplicates) {
    Document doc;
    Layer* l = doc.addLayer(LayerKind::Vector, "Ink");
    DrawingId d0 = ops::insertBlankKeyframe(doc, *l, 0);
    doc.drawings.get(d0)->vector()->strokes = line({0, 0}, {10, 0}).strokes;
    ops::insertFrames(*l, 0, 3);              // frames 0..3 hold d0
    CHECK(l->exposure.length() == 4);
    CHECK(l->exposure.keyAt(3)->drawing == d0);

    DrawingId d4 = ops::insertKeyframe(doc, *l, 4);  // F6 copies content
    CHECK(d4 && d4 != d0);
    CHECK(doc.drawings.get(d4)->vector()->strokes.size() == 1);

    DrawingId dup = ops::duplicateSpan(doc, *l, 1);  // duplicate span [0,4) after itself
    CHECK(l->exposure.isKey(4) && l->exposure.keyAt(4)->drawing == dup);
    CHECK(l->exposure.isKey(8) && l->exposure.keyAt(8)->drawing == d4);  // later key shifted by 4

    CHECK(ops::linkedDuplicate(*l, 0, 9));
    CHECK(l->exposure.keyAt(9)->drawing == d0);       // shares drawing
    DrawingId uniq = ops::makeUnique(doc, *l, 9);
    CHECK(uniq != d0);

    ops::removeFrames(*l, 4, 4);                       // delete the duplicated span
    CHECK(l->exposure.keyAt(4)->drawing == d4);
}

TEST(raster_duplicate_is_copy_on_write) {
    Document doc;
    Layer* l = doc.addLayer(LayerKind::Raster, "Paint");
    DrawingId a = ops::insertBlankKeyframe(doc, *l, 0);
    doc.drawings.get(a)->raster()->tileForWrite(0, 0).rgba[0] = 255;
    DrawingId b = ops::insertKeyframe(doc, *l, 1);
    auto* ra = doc.drawings.get(a)->raster();
    auto* rb = doc.drawings.get(b)->raster();
    CHECK(ra->sharesTileWith(*rb, 0, 0));
    rb->tileForWrite(0, 0).rgba[0] = 7;
    CHECK(!ra->sharesTileWith(*rb, 0, 0));
    CHECK(ra->tile(0, 0)->rgba[0] == 255);
}

TEST(palette_change_once_applies_everywhere) {
    Document doc;
    StyleId skin = doc.palette.add(doc.ids, "Skin", {1, 0.8f, 0.6f, 1});
    Layer* l = doc.addLayer(LayerKind::Vector, "Char");
    for (int f = 0; f < 3; ++f) {
        DrawingId d = ops::insertBlankKeyframe(doc, *l, f);
        *doc.drawings.get(d)->vector() = line({0, 0}, {1, 1}, skin);
    }
    doc.palette.setColor(skin, {0.5f, 0.3f, 0.2f, 1});
    for (int f = 0; f < 3; ++f) {
        const auto& s = doc.drawings.get(l->exposure.keyAt(f)->drawing)->vector()->strokes[0];
        CHECK(doc.palette.resolve(s.style) == (Color{0.5f, 0.3f, 0.2f, 1}));
    }
}

TEST(nested_folders_compose_transforms_and_effects) {
    Document doc;
    Layer* outer = doc.addLayer(LayerKind::Folder, "Character");
    Layer* inner = doc.addLayer(LayerKind::Folder, "Arm", outer->id);
    Layer* ink = doc.addLayer(LayerKind::Vector, "Hand", inner->id);
    ops::insertBlankKeyframe(doc, *ink, 0);
    ops::insertFrames(*ink, 0, 9);

    outer->transform.position.setKey(0, {100, 0});
    inner->transform.position.setKey(0, {0, 0});
    inner->transform.position.setKey(10, {0, 50});  // folder animated by itself
    Effect glow; glow.type = EffectType::Glow;
    glow.radius.setKey(0, 0.0f); glow.radius.setKey(10, 20.0f);
    inner->effects.push_back(glow);

    EvalOptions canvasSpace; canvasSpace.applyCamera = false;
    DisplayList dl = evaluateFrame(doc, 5, canvasSpace);
    CHECK(dl.drawCount() == 1);
    const RenderCmd* draw = nullptr; const RenderCmd* innerPush = nullptr;
    for (auto& c : dl.cmds) {
        if (c.kind == RenderCmd::Kind::DrawDrawing) draw = &c;
        if (c.kind == RenderCmd::Kind::PushGroup && c.layer == inner->id) innerPush = &c;
    }
    CHECK(draw && draw->transform.approxEqual(Affine::translate({100, 25})));
    CHECK(innerPush && innerPush->effects.size() == 1);
    NEAR(innerPush->effects[0].radius, 10.0f);
}

TEST(animation_folder_picks_cels) {
    Document doc;
    Layer* folder = doc.addLayer(LayerKind::Folder, "Walk");
    folder->animationFolder = true;
    Layer* c1 = doc.addLayer(LayerKind::Vector, "1", folder->id);
    Layer* c2 = doc.addLayer(LayerKind::Vector, "2", folder->id);
    ops::insertBlankKeyframe(doc, *c1, 0);
    ops::insertBlankKeyframe(doc, *c2, 0);
    Keyframe k1; k1.cel = c1->id; folder->exposure.setKey(0, k1);
    Keyframe k2; k2.cel = c2->id; folder->exposure.setKey(2, k2);
    folder->exposure.setLength(4);
    EvalOptions o; o.applyCamera = false;
    auto find = [&](int f) { for (auto& c : evaluateFrame(doc, f, o).cmds) if (c.kind == RenderCmd::Kind::DrawDrawing) return c.layer; return LayerId{}; };
    CHECK(find(1) == c1->id);
    CHECK(find(3) == c2->id);
}

TEST(motion_tween) {
    Document doc;
    Layer* l = doc.addLayer(LayerKind::Vector, "Ball");
    ops::insertBlankKeyframe(doc, *l, 0);
    ops::createMotionTween(*l, 0, 12, Easing::easeInOut());
    ops::Pose end = ops::poseAt(*l, 12);
    end.position = {240, 0}; end.rotationDeg = 90;
    ops::setPoseKey(*l, 12, end);
    NEAR(ops::poseAt(*l, 6).position.x, 120.0f);
    CHECK(ops::poseAt(*l, 3).position.x < 60.0f);  // eased
    CHECK(l->exposure.length() == 13);
}

TEST(graphic_vs_movieclip_symbols) {
    Document doc;
    SymbolDef& blink = doc.createSymbol("Blink", SymbolKind::Graphic);
    Layer* eye = blink.timeline.addLayer(doc.ids, LayerKind::Vector, "Eye");
    for (int f = 0; f < 4; ++f) ops::insertBlankKeyframe(doc, *eye, f);
    const SymbolId blinkId = blink.id;

    SymbolInstance loop{blinkId, LoopMode::Loop, 0};
    const SymbolDef& def = *doc.symbol(blinkId);
    CHECK(ops::symbolFrame(def, loop, 5, 5) == 1);
    CHECK(ops::symbolFrame(def, {blinkId, LoopMode::PlayOnce, 0}, 9, 9) == 3);
    CHECK(ops::symbolFrame(def, {blinkId, LoopMode::SingleFrame, 2}, 9, 9) == 2);

    // A graphic held inside a single-frame parent freezes; a movie clip keeps playing.
    SymbolDef& holder = doc.createSymbol("Holder", SymbolKind::Graphic);
    Layer* inner = holder.timeline.addLayer(doc.ids, LayerKind::Symbol, "inner");
    Keyframe ik; ik.instance = SymbolInstance{blinkId, LoopMode::Loop, 0};
    inner->exposure.setKey(0, ik);
    inner->exposure.setLength(1);
    Layer* top = doc.addLayer(LayerKind::Symbol, "holder");
    Keyframe hk; hk.instance = SymbolInstance{holder.id, LoopMode::SingleFrame, 0};
    top->exposure.setKey(0, hk);
    top->exposure.setLength(10);

    EvalOptions o; o.applyCamera = false;
    auto shown = [&](int f) { for (auto& c : evaluateFrame(doc, f, o).cmds) if (c.kind == RenderCmd::Kind::DrawDrawing) return c.drawing; return DrawingId{}; };
    CHECK(shown(0) == shown(2));                 // graphic: frozen
    doc.symbol(blinkId)->kind = SymbolKind::MovieClip;
    CHECK(shown(0) != shown(2));                 // movie clip: own clock
    CHECK(shown(1) == eye->exposure.keyAt(1)->drawing);
}

TEST(convert_to_symbol) {
    Document doc;
    Layer* l = doc.addLayer(LayerKind::Vector, "Prop");
    ops::insertBlankKeyframe(doc, *l, 0);
    ops::insertFrames(*l, 0, 5);
    const LayerId oldId = l->id;
    SymbolId s = doc.convertToSymbol(oldId, "PropSym", SymbolKind::Graphic);
    CHECK(s);
    CHECK(doc.timeline.find(oldId) == nullptr);
    CHECK(doc.symbol(s)->timeline.find(oldId) != nullptr);
    CHECK(doc.timeline.root().children.size() == 1);
    CHECK(doc.timeline.root().children[0]->kind == LayerKind::Symbol);
    EvalOptions o; o.applyCamera = false;
    CHECK(evaluateFrame(doc, 3, o).drawCount() == 1);
}

TEST(camera_on_big_canvas) {
    Document doc;  // canvas 3840x2160, movie 1920x1080
    Camera& cam = doc.cameras.front();
    cam.center.setKey(0, {960, 1080});
    cam.center.setKey(24, {2880, 1080});  // pan across the background
    Vec2 c0 = cam.viewMatrix(0, doc.movie).apply({960, 1080});
    NEAR(c0.x, 960.0f); NEAR(c0.y, 540.0f);
    auto corners = cam.frameCorners(12, doc.movie);
    NEAR(corners[0].x, 1920.0f - 960.0f);
    cam.zoom.setKey(0, 2.0f);
    auto z = cam.frameCorners(0, doc.movie);
    NEAR(z[1].x - z[0].x, 960.0f);  // zoomed in: sees half the width

    Camera& cam2 = doc.addCamera("Close-up");
    doc.cameraCuts[10] = cam2.id;
    CHECK(doc.activeCamera(5)->id == cam.id);
    CHECK(doc.activeCamera(10)->id == cam2.id);
}

TEST(undo_redo_transaction) {
    Document doc;
    edit::UndoStack undo;
    Layer* l = doc.addLayer(LayerKind::Vector, "Ink");
    const size_t before = doc.drawings.size();
    {
        edit::Transaction tx(doc, "Insert keyframe");
        tx.touchLayer(l->id);
        ops::insertBlankKeyframe(doc, *l, 0);
        ops::insertFrames(*l, 0, 2);
        tx.commit(undo);
    }
    CHECK(l->exposure.length() == 3 && doc.drawings.size() == before + 1);
    CHECK(undo.undo(doc));
    CHECK(l->exposure.length() == 0 && doc.drawings.size() == before);
    CHECK(undo.redo(doc));
    CHECK(l->exposure.length() == 3 && doc.drawings.size() == before + 1);
}

TEST(multi_frame_transform_and_lasso) {
    Document doc;
    edit::UndoStack undo;
    Layer* l = doc.addLayer(LayerKind::Vector, "Ink");
    for (int f = 0; f < 3; ++f) {
        DrawingId d = ops::insertBlankKeyframe(doc, *l, f);
        auto& v = *doc.drawings.get(d)->vector();
        v = line({0, 0}, {10, 0});
        v.strokes.push_back(line({100, 100}, {110, 100}).strokes[0]);
    }
    ops::linkedDuplicate(*l, 0, 3);  // linked frame must move only once
    edit::FrameSelection sel{{l->id}, 0, 3};
    CHECK(edit::drawingsIn(doc, sel).size() == 3);

    edit::transformFrames(doc, sel, Affine::translate({5, 0}), &undo);
    for (int f = 0; f < 4; ++f) NEAR(doc.drawings.get(l->exposure.keyAt(f)->drawing)->vector()->strokes[0].points[0].pos.x, 5.0f);

    std::vector<Vec2> lasso{{90, 90}, {120, 90}, {120, 110}, {90, 110}};
    edit::lassoTransformFrames(doc, sel, lasso, Affine::translate({0, 10}), edit::LassoMode::AllPointsInside, &undo);
    for (int f = 0; f < 3; ++f) {
        auto& v = *doc.drawings.get(l->exposure.keyAt(f)->drawing)->vector();
        NEAR(v.strokes[1].points[0].pos.y, 110.0f);
        NEAR(v.strokes[0].points[0].pos.y, 0.0f);
    }
    undo.undo(doc);
    undo.undo(doc);
    NEAR(doc.drawings.get(l->exposure.keyAt(0)->drawing)->vector()->strokes[0].points[0].pos.x, 0.0f);
}

TEST(tracked_object_restyle) {
    Document doc;
    StyleId red = doc.palette.add(doc.ids, "Red", {1, 0, 0, 1});
    StyleId blue = doc.palette.add(doc.ids, "Blue", {0, 0, 1, 1});
    const ObjectId scarf = doc.ids.make<ObjectId>();
    Layer* l = doc.addLayer(LayerKind::Vector, "Char");
    for (int f = 0; f < 4; ++f) {
        DrawingId d = ops::insertBlankKeyframe(doc, *l, f);
        auto& v = *doc.drawings.get(d)->vector();
        v = line({0, 0}, {1, 0}, red, scarf);
        v.strokes.push_back(line({0, 0}, {1, 0}, red).strokes[0]);
    }
    CHECK(edit::restyleObject(doc, {{l->id}, 0, 3}, scarf, blue) == 4);
    auto& v = *doc.drawings.get(l->exposure.keyAt(2)->drawing)->vector();
    CHECK(v.strokes[0].style == blue && v.strokes[1].style == red);
}

TEST(stroke_stabilizer_and_resample) {
    brush::StabilizerSettings s; s.lazyRadius = 0; s.smoothingWindow = 4; s.spacing = 2;
    brush::StrokeBuilder b(s);
    b.begin({{0, 0}, 0.5f});
    for (int i = 1; i <= 50; ++i) b.add({{float(i), (i % 2) ? 3.0f : -3.0f}, 0.5f});  // jittery input
    b.end();
    const auto& pts = b.points();
    CHECK(pts.size() > 10);
    float maxDev = 0;
    for (size_t i = 1; i + 1 < pts.size(); ++i) maxDev = std::max(maxDev, std::abs(pts[i].pos.y));
    CHECK(maxDev < 3.0f);  // jitter reduced
    NEAR(pts.back().pos.x, 50.0f);

    auto r = brush::resample(pts, 11);
    CHECK(r.size() == 11);
    NEAR(r.front().pos.x, pts.front().pos.x);
    CHECK(!brush::dabs(pts, 10).empty());
}

TEST(drawing_assists) {
    assist::Grid g{10, {0, 0}};
    CHECK(g.snap({14, 26}) == (Vec2{10, 30}));
    assist::LineRuler r{{0, 0}, {10, 0}};
    NEAR(r.project({5, 7}).y, 0.0f);
    assist::Symmetry sym{{50, 0}, 2, true, 90};
    auto t = sym.transforms();
    CHECK(t.size() == 2);
    Vec2 m = t[1].apply({40, 5});
    NEAR(m.x, 60.0f); NEAR(m.y, 5.0f);
    assist::EllipseRuler e{{0, 0}, 10, 5, 0};
    Vec2 p = e.project({20, 0});
    NEAR(p.x, 10.0f); NEAR(p.y, 0.0f);
}

TEST(lip_sync_drives_mouth_symbol) {
    Document doc;
    SymbolDef& mouth = doc.createSymbol("Mouth", SymbolKind::Graphic);
    Layer* m = mouth.timeline.addLayer(doc.ids, LayerKind::Vector, "shapes");
    for (int f = 0; f < int(kPhonemeCount); ++f) ops::insertBlankKeyframe(doc, *m, f);
    Layer* ml = doc.addLayer(LayerKind::Symbol, "Mouth");
    Keyframe k; k.instance = SymbolInstance{mouth.id, LoopMode::SingleFrame, 0};
    ml->exposure.setKey(0, k);

    LipSyncTrack ls;
    ls.targetLayer = ml->id;
    for (size_t i = 0; i < kPhonemeCount; ++i) ls.chart.symbolFrame[i] = int(i);
    ls.keys = {{0, Phoneme::Rest}, {3, Phoneme::AI}, {6, Phoneme::MBP}};
    CHECK(ops::applyLipSync(doc, ls));
    ml->exposure.setLength(9);
    EvalOptions o; o.applyCamera = false;
    auto shown = [&](int f) { for (auto& c : evaluateFrame(doc, f, o).cmds) if (c.kind == RenderCmd::Kind::DrawDrawing) return c.drawing; return DrawingId{}; };
    CHECK(shown(4) == m->exposure.keyAt(int(Phoneme::AI))->drawing);
    CHECK(shown(7) == m->exposure.keyAt(int(Phoneme::MBP))->drawing);

    AudioClip clip; clip.startFrame = 10; clip.durationSec = 1.0;
    CHECK(clip.endFrame(24) == 34);
    NEAR(clip.sourceTimeAt(22, 24), 0.5);
}

struct FakeEntitlements : ai::IEntitlements {
    bool pro = false;
    bool has(const std::string& f) const override { return pro && f == "ai_inbetween"; }
};
struct PaidProvider : ai::IInbetweenProvider {
    std::string id() const override { return "cloud"; }
    std::string requiredEntitlement() const override { return "ai_inbetween"; }
    ai::InbetweenResult generate(const ai::InbetweenPayload&) override { return {}; }
};

TEST(ai_inbetween_hook) {
    Document doc;
    edit::UndoStack undo;
    Layer* l = doc.addLayer(LayerKind::Vector, "Arm");
    const ObjectId arm = doc.ids.make<ObjectId>();
    DrawingId a = ops::insertBlankKeyframe(doc, *l, 0);
    DrawingId b = ops::insertBlankKeyframe(doc, *l, 8);
    *doc.drawings.get(a)->vector() = line({0, 0}, {10, 0}, {}, arm);
    *doc.drawings.get(b)->vector() = line({0, 80}, {10, 80}, {}, arm);

    FakeEntitlements ent;
    ai::InbetweenService svc(&ent);
    svc.registerProvider(std::make_unique<ai::GeometricInbetweenProvider>());
    svc.registerProvider(std::make_unique<PaidProvider>());

    ai::InbetweenRequest req{l->id, 0, 8};
    req.step = 2;
    CHECK(!svc.run(doc, req, "cloud", &undo).ok());  // paywalled
    auto res = svc.run(doc, req, "geometric", &undo);
    CHECK(res.ok() && res.frames.size() == 3);       // frames 2,4,6 (on twos)
    auto& mid = *doc.drawings.get(l->exposure.keyAt(4)->drawing)->vector();
    NEAR(mid.strokes[0].points[0].pos.y, 40.0f);
    CHECK(mid.strokes[0].object == arm);
    undo.undo(doc);
    CHECK(!l->exposure.isKey(4));
}

TEST(layer_reparent_into_folder) {
    Document doc;
    Layer* f = doc.addLayer(LayerKind::Folder, "Head");
    Layer* l = doc.addLayer(LayerKind::Vector, "Eyes");
    CHECK(doc.timeline.move(l->id, f->id));
    CHECK(doc.timeline.parentOf(l->id) == f);
    CHECK(!doc.timeline.move(f->id, l->id));  // cannot move into non-folder / cycle
}

int main() {
    for (auto& t : registry()) {
        const int before = g_fail;
        t.fn();
        std::printf("%s %s\n", g_fail == before ? "PASS" : "FAIL", t.name);
    }
    std::printf("\n%d checks, %d failed, %zu tests\n", g_checks, g_fail, registry().size());
    return g_fail == 0 ? 0 : 1;
}
