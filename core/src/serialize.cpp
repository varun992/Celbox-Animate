#include "anim/serialize.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "anim/json.h"

namespace anim::io {
namespace {
using json::Array;
using json::Object;
using json::Value;

// ---- primitives ----
Value toJ(float v) { return Value(v); }
Value toJ(Vec2 v) { return Value(Array{v.x, v.y}); }
Value toJ(Color c) { return Value(Array{c.r, c.g, c.b, c.a}); }
void fromJ(const Value& j, float& v) { v = j.f(v); }
void fromJ(const Value& j, Vec2& v) { const auto& a = j.arr(); if (a.size() >= 2) v = {a[0].f(), a[1].f()}; }
void fromJ(const Value& j, Color& c) { const auto& a = j.arr(); if (a.size() >= 4) c = {a[0].f(), a[1].f(), a[2].f(), a[3].f()}; }

Value easeJ(const Easing& e) { return Value(Array{int(e.kind), e.x1, e.y1, e.x2, e.y2}); }
Easing easeFromJ(const Value& j) {
    const auto& a = j.arr();
    if (a.size() < 5) return {};
    return {EaseKind(a[0].i()), a[1].f(), a[2].f(), a[3].f(), a[4].f()};
}

template <class T>
Value trackJ(const AnimTrack<T>& t) {
    Array keys;
    for (const auto& k : t.keys()) keys.push_back(Array{k.frame, toJ(k.value), easeJ(k.ease)});
    return Object{{"def", toJ(t.defaultValue())}, {"keys", std::move(keys)}};
}
template <class T>
void trackFromJ(const Value& j, AnimTrack<T>& t) {
    if (j.isNull()) return;
    T def = t.defaultValue();
    fromJ(j["def"], def);
    t = AnimTrack<T>(def);
    for (const auto& k : j["keys"].arr()) {
        const auto& a = k.arr();
        if (a.size() < 3) continue;
        T v = def;
        fromJ(a[1], v);
        t.setKey(a[0].i(), v, easeFromJ(a[2]));
    }
}

template <class Tag> Value idJ(Id<Tag> id) { return Value(id.value); }
template <class Tag> Id<Tag> idFromJ(const Value& j) { return Id<Tag>{j.u64()}; }

// ---- model ----
Value transformJ(const TransformTracks& t) {
    return Object{{"position", trackJ(t.position)}, {"rotation", trackJ(t.rotationDeg)}, {"scale", trackJ(t.scale)},
                  {"skew", trackJ(t.skewDeg)},      {"anchor", trackJ(t.anchor)},        {"opacity", trackJ(t.opacity)}};
}
void transformFromJ(const Value& j, TransformTracks& t) {
    trackFromJ(j["position"], t.position); trackFromJ(j["rotation"], t.rotationDeg); trackFromJ(j["scale"], t.scale);
    trackFromJ(j["skew"], t.skewDeg);      trackFromJ(j["anchor"], t.anchor);        trackFromJ(j["opacity"], t.opacity);
}

Value effectJ(const Effect& e) {
    return Object{{"type", int(e.type)},          {"enabled", e.enabled},           {"radius", trackJ(e.radius)},
                  {"intensity", trackJ(e.intensity)}, {"color", trackJ(e.color)}, {"offset", trackJ(e.offset)}};
}
Effect effectFromJ(const Value& j) {
    Effect e;
    e.type = EffectType(j["type"].i());
    e.enabled = j["enabled"].b(true);
    trackFromJ(j["radius"], e.radius); trackFromJ(j["intensity"], e.intensity);
    trackFromJ(j["color"], e.color);   trackFromJ(j["offset"], e.offset);
    return e;
}

Value clipJ(const AudioClip& c) {
    return Object{{"id", idJ(c.id)}, {"asset", idJ(c.asset)}, {"role", int(c.role)}, {"start", c.startFrame},
                  {"in", c.sourceInSec}, {"dur", c.durationSec}, {"gain", trackJ(c.gainDb)}, {"pan", c.pan}, {"muted", c.muted}};
}
AudioClip clipFromJ(const Value& j) {
    AudioClip c;
    c.id = idFromJ<AudioClipTag>(j["id"]); c.asset = idFromJ<AudioAssetTag>(j["asset"]);
    c.role = AudioRole(j["role"].i()); c.startFrame = j["start"].i();
    c.sourceInSec = j["in"].num(); c.durationSec = j["dur"].num();
    trackFromJ(j["gain"], c.gainDb); c.pan = j["pan"].f(); c.muted = j["muted"].b();
    return c;
}

Value layerJ(const Layer& l) {
    Array keys;
    for (const auto& [f, k] : l.exposure.keys()) {
        Object o{{"f", f}};
        if (k.drawing) o["drawing"] = idJ(k.drawing);
        if (k.instance) o["inst"] = Object{{"symbol", idJ(k.instance->symbol)}, {"loop", int(k.instance->loop)}, {"first", k.instance->firstFrame}};
        if (k.cel) o["cel"] = idJ(k.cel);
        if (!k.label.empty()) o["label"] = k.label;
        keys.push_back(std::move(o));
    }
    Array children, effects, audio;
    for (const auto& c : l.children) children.push_back(layerJ(*c));
    for (const auto& e : l.effects) effects.push_back(effectJ(e));
    for (const auto& a : l.audioClips) audio.push_back(clipJ(a));
    Object o{{"id", idJ(l.id)},          {"name", l.name},
             {"kind", int(l.kind)},      {"visible", l.visible},
             {"locked", l.locked},       {"clip", l.clipToBelow},
             {"animFolder", l.animationFolder}, {"blend", int(l.blend)},
             {"tint", toJ(l.timelineTint)}, {"transform", transformJ(l.transform)},
             {"effects", std::move(effects)}, {"length", l.exposure.length()},
             {"keys", std::move(keys)},  {"children", std::move(children)},
             {"audio", std::move(audio)}};
    if (l.model) o["model"] = Object{{"uri", l.model->assetUri}, {"format", int(l.model->format)},
                                     {"clip", l.model->animationClip}, {"followCamera", l.model->followSceneCamera}};
    return o;
}

std::unique_ptr<Layer> layerFromJ(const Value& j) {
    auto l = std::make_unique<Layer>();
    l->id = idFromJ<LayerTag>(j["id"]);
    l->name = j["name"].str();
    l->kind = LayerKind(j["kind"].i());
    l->visible = j["visible"].b(true); l->locked = j["locked"].b();
    l->clipToBelow = j["clip"].b(); l->animationFolder = j["animFolder"].b();
    l->blend = BlendMode(j["blend"].i());
    fromJ(j["tint"], l->timelineTint);
    transformFromJ(j["transform"], l->transform);
    for (const auto& e : j["effects"].arr()) l->effects.push_back(effectFromJ(e));
    for (const auto& k : j["keys"].arr()) {
        Keyframe kf;
        kf.drawing = idFromJ<DrawingTag>(k["drawing"]);
        if (k["inst"].isObject())
            kf.instance = SymbolInstance{idFromJ<SymbolTag>(k["inst"]["symbol"]), LoopMode(k["inst"]["loop"].i()), k["inst"]["first"].i()};
        kf.cel = idFromJ<LayerTag>(k["cel"]);
        kf.label = k["label"].str();
        l->exposure.setKey(k["f"].i(), std::move(kf));
    }
    l->exposure.setLength(std::max(l->exposure.length(), j["length"].i()));
    for (const auto& c : j["children"].arr()) l->children.push_back(layerFromJ(c));
    for (const auto& a : j["audio"].arr()) l->audioClips.push_back(clipFromJ(a));
    if (j["model"].isObject()) {
        const auto& m = j["model"];
        l->model = Model3DRef{m["uri"].str(), Model3DRef::Format(m["format"].i()), m["clip"].str(), m["followCamera"].b(true)};
    }
    return l;
}

Value timelineJ(const Timeline& t) {
    Array layers;
    for (const auto& c : t.root().children) layers.push_back(layerJ(*c));
    return layers;
}
void timelineFromJ(const Value& j, Timeline& t) {
    t.root().children.clear();
    for (const auto& c : j.arr()) t.root().children.push_back(layerFromJ(c));
}

Value vectorJ(const VectorDrawing& d) {
    Array strokes, fills;
    for (const auto& s : d.strokes) {
        Array pts;
        pts.reserve(s.points.size() * 3);
        for (const auto& p : s.points) { pts.push_back(p.pos.x); pts.push_back(p.pos.y); pts.push_back(p.pressure); }
        strokes.push_back(Object{{"style", idJ(s.style)}, {"width", s.width}, {"object", idJ(s.object)},
                                 {"closed", s.closed}, {"pts", std::move(pts)}});
    }
    for (const auto& f : d.fills) {
        Array pts;
        for (const auto& p : f.contour) { pts.push_back(p.x); pts.push_back(p.y); }
        fills.push_back(Object{{"style", idJ(f.style)}, {"object", idJ(f.object)}, {"pts", std::move(pts)}});
    }
    return Object{{"strokes", std::move(strokes)}, {"fills", std::move(fills)}};
}
VectorDrawing vectorFromJ(const Value& j) {
    VectorDrawing d;
    for (const auto& s : j["strokes"].arr()) {
        VectorStroke st;
        st.style = idFromJ<StyleTag>(s["style"]); st.width = s["width"].f(4);
        st.object = idFromJ<ObjectTag>(s["object"]); st.closed = s["closed"].b();
        const auto& p = s["pts"].arr();
        for (size_t i = 0; i + 2 < p.size(); i += 3) st.points.push_back({{p[i].f(), p[i + 1].f()}, p[i + 2].f()});
        d.strokes.push_back(std::move(st));
    }
    for (const auto& f : j["fills"].arr()) {
        FillRegion r;
        r.style = idFromJ<StyleTag>(f["style"]); r.object = idFromJ<ObjectTag>(f["object"]);
        const auto& p = f["pts"].arr();
        for (size_t i = 0; i + 1 < p.size(); i += 2) r.contour.push_back({p[i].f(), p[i + 1].f()});
        d.fills.push_back(std::move(r));
    }
    return d;
}

// Raster: "CBXR" u32 version, i32 w, i32 h, u32 tileCount, then per tile i32 tx, i32 ty, rgba bytes.
void put32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); }
uint32_t get32(const Bytes& b, size_t& p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= uint32_t(b[p + i]) << (8 * i);
    p += 4;
    return v;
}
constexpr size_t kTileBytes = size_t(RasterTile::kSize) * RasterTile::kSize * 4;

Bytes rasterBytes(const RasterDrawing& r) {
    Bytes b{'C', 'B', 'X', 'R'};
    put32(b, 1); put32(b, uint32_t(r.width())); put32(b, uint32_t(r.height())); put32(b, uint32_t(r.tileCount()));
    r.forEachTile([&](int tx, int ty, const RasterTile& t) {
        put32(b, uint32_t(tx)); put32(b, uint32_t(ty));
        b.insert(b.end(), t.rgba.begin(), t.rgba.end());
    });
    return b;
}
bool rasterFromBytes(const Bytes& b, RasterDrawing& out) {
    if (b.size() < 20 || std::memcmp(b.data(), "CBXR", 4) != 0) return false;
    size_t p = 4;
    get32(b, p);
    const int w = int(get32(b, p)), h = int(get32(b, p));
    const uint32_t n = get32(b, p);
    if (b.size() < p + size_t(n) * (8 + kTileBytes)) return false;
    out = RasterDrawing(w, h);
    for (uint32_t i = 0; i < n; ++i) {
        const int tx = int(int32_t(get32(b, p))), ty = int(int32_t(get32(b, p)));
        auto t = std::make_shared<RasterTile>();
        std::memcpy(t->rgba.data(), b.data() + p, kTileBytes);
        p += kTileBytes;
        out.putTile(tx, ty, std::move(t));
    }
    return true;
}

Bytes toBytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

void collectDrawings(const Layer& l, std::vector<DrawingId>& out) {
    for (const auto& [_, k] : l.exposure.keys()) if (k.drawing) out.push_back(k.drawing);
    for (const auto& c : l.children) collectDrawings(*c, out);
}
}  // namespace

Package save(const Document& doc) {
    Package pkg;
    Array palette, symbols, cameras, cuts, assets, lipsync;
    for (const auto& s : doc.palette.styles()) {
        Object o{{"id", idJ(s.id)}, {"name", s.name}, {"color", toJ(s.color)}, {"locked", s.locked}};
        if (s.texture) o["texture"] = Object{{"uri", s.texture->assetUri}, {"mode", int(s.texture->mode)}, {"scale", s.texture->scale},
                                             {"rotation", s.texture->rotationDeg}, {"strength", s.texture->strength}};
        palette.push_back(std::move(o));
    }
    std::vector<DrawingId> used;
    collectDrawings(doc.timeline.root(), used);
    for (const auto& [id, s] : doc.symbols) {
        symbols.push_back(Object{{"id", idJ(id)}, {"name", s.name}, {"kind", int(s.kind)},
                                 {"registration", toJ(s.registration)}, {"layers", timelineJ(s.timeline)}});
        collectDrawings(s.timeline.root(), used);
    }
    for (const auto& c : doc.cameras)
        cameras.push_back(Object{{"id", idJ(c.id)}, {"name", c.name}, {"center", trackJ(c.center)},
                                 {"zoom", trackJ(c.zoom)}, {"rotation", trackJ(c.rotationDeg)}});
    for (const auto& [f, id] : doc.cameraCuts) cuts.push_back(Array{f, idJ(id)});
    for (const auto& a : doc.audioAssets)
        assets.push_back(Object{{"id", idJ(a.id)}, {"uri", a.uri}, {"rate", a.sampleRate}, {"channels", a.channels}, {"dur", a.durationSec}});
    for (const auto& t : doc.lipSync) {
        Array chart, keys;
        for (int v : t.chart.symbolFrame) chart.push_back(v);
        for (const auto& k : t.keys) keys.push_back(Array{k.frame, int(k.phoneme)});
        lipsync.push_back(Object{{"clip", idJ(t.clip)}, {"layer", idJ(t.targetLayer)}, {"chart", std::move(chart)}, {"keys", std::move(keys)}});
    }

    Object root{
        {"format", "celbox"},
        {"version", kFormatVersion},
        {"nextId", Value(doc.ids.peek())},
        {"canvas", Object{{"width", doc.canvas.width}, {"height", doc.canvas.height}, {"background", toJ(doc.canvas.background)}, {"dpi", doc.canvas.dpi}}},
        {"movie", Object{{"width", doc.movie.width}, {"height", doc.movie.height}, {"fps", doc.movie.fps},
                         {"frameCount", doc.movie.frameCount}, {"pixelAspect", doc.movie.pixelAspect}}},
        {"palette", std::move(palette)},
        {"layers", timelineJ(doc.timeline)},
        {"symbols", std::move(symbols)},
        {"cameras", std::move(cameras)},
        {"cameraCuts", std::move(cuts)},
        {"audioAssets", std::move(assets)},
        {"lipSync", std::move(lipsync)},
    };
    pkg["document.json"] = toBytes(json::write(root));

    std::sort(used.begin(), used.end());
    used.erase(std::unique(used.begin(), used.end()), used.end());
    for (DrawingId id : used) {
        const Drawing* d = doc.drawings.get(id);
        if (!d) continue;
        const std::string base = "drawings/" + std::to_string(id.value);
        if (auto* v = d->vector()) pkg[base + ".json"] = toBytes(json::write(vectorJ(*v)));
        else if (auto* r = std::get_if<RasterDrawing>(&d->content)) pkg[base + ".raster"] = rasterBytes(*r);
    }
    return pkg;
}

bool load(const Package& pkg, Document& doc, std::string* error) {
    auto fail = [&](std::string m) { if (error) *error = std::move(m); return false; };
    auto it = pkg.find("document.json");
    if (it == pkg.end()) return fail("missing document.json");
    std::string perr;
    auto parsed = json::parse(std::string_view(reinterpret_cast<const char*>(it->second.data()), it->second.size()), &perr);
    if (!parsed) return fail("document.json: " + perr);
    const Value& j = *parsed;
    if (j["format"].str() != "celbox") return fail("not a celbox document");
    if (j["version"].i() > kFormatVersion) return fail("document was made by a newer version of Celbox");

    doc = Document();
    doc.cameras.clear();
    doc.cameraCuts.clear();
    if (const uint64_t next = j["nextId"].u64(); next > 0) doc.ids.reserveAbove(next - 1);

    const auto& c = j["canvas"];
    doc.canvas.width = c["width"].i(doc.canvas.width); doc.canvas.height = c["height"].i(doc.canvas.height);
    fromJ(c["background"], doc.canvas.background); doc.canvas.dpi = c["dpi"].f(doc.canvas.dpi);
    const auto& m = j["movie"];
    doc.movie.width = m["width"].i(doc.movie.width); doc.movie.height = m["height"].i(doc.movie.height);
    doc.movie.fps = m["fps"].num(doc.movie.fps); doc.movie.frameCount = m["frameCount"].i();
    doc.movie.pixelAspect = m["pixelAspect"].f(1);

    for (const auto& s : j["palette"].arr()) {
        Style st{idFromJ<StyleTag>(s["id"]), s["name"].str(), {}, std::nullopt, s["locked"].b()};
        fromJ(s["color"], st.color);
        if (s["texture"].isObject()) {
            const auto& t = s["texture"];
            st.texture = TextureRef{t["uri"].str(), TextureRef::Mode(t["mode"].i()), t["scale"].f(1), t["rotation"].f(), t["strength"].f(1)};
        }
        doc.palette.restore(std::move(st));
    }
    timelineFromJ(j["layers"], doc.timeline);
    for (const auto& s : j["symbols"].arr()) {
        SymbolDef def;
        def.id = idFromJ<SymbolTag>(s["id"]);
        def.name = s["name"].str();
        def.kind = SymbolKind(s["kind"].i());
        fromJ(s["registration"], def.registration);
        timelineFromJ(s["layers"], def.timeline);
        doc.symbols.emplace(def.id, std::move(def));
    }
    for (const auto& cj : j["cameras"].arr()) {
        Camera cam;
        cam.id = idFromJ<CameraTag>(cj["id"]);
        cam.name = cj["name"].str();
        trackFromJ(cj["center"], cam.center); trackFromJ(cj["zoom"], cam.zoom); trackFromJ(cj["rotation"], cam.rotationDeg);
        doc.cameras.push_back(std::move(cam));
    }
    if (doc.cameras.empty()) {  // always keep a camera
        doc.ids.reserveAbove(doc.ids.peek());
        Camera& cam = doc.addCamera("Camera 1");
        doc.cameraCuts[0] = cam.id;
    }
    for (const auto& cut : j["cameraCuts"].arr()) {
        const auto& a = cut.arr();
        if (a.size() == 2) doc.cameraCuts[a[0].i()] = idFromJ<CameraTag>(a[1]);
    }
    for (const auto& a : j["audioAssets"].arr())
        doc.audioAssets.push_back({idFromJ<AudioAssetTag>(a["id"]), a["uri"].str(), a["rate"].i(48000), a["channels"].i(2), a["dur"].num(), {}});
    for (const auto& t : j["lipSync"].arr()) {
        LipSyncTrack ls;
        ls.clip = idFromJ<AudioClipTag>(t["clip"]);
        ls.targetLayer = idFromJ<LayerTag>(t["layer"]);
        const auto& ch = t["chart"].arr();
        for (size_t i = 0; i < ch.size() && i < kPhonemeCount; ++i) ls.chart.symbolFrame[i] = ch[i].i();
        for (const auto& k : t["keys"].arr()) ls.keys.push_back({k.arr().at(0).i(), Phoneme(k.arr().at(1).i())});
        doc.lipSync.push_back(std::move(ls));
    }

    for (const auto& [path, bytes] : pkg) {
        if (path.rfind("drawings/", 0) != 0) continue;
        const std::string name = path.substr(9);
        const size_t dot = name.find('.');
        if (dot == std::string::npos) continue;
        const DrawingId id{std::stoull(name.substr(0, dot))};
        const std::string ext = name.substr(dot + 1);
        if (ext == "json") {
            auto dj = json::parse(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()), &perr);
            if (!dj) return fail(path + ": " + perr);
            doc.drawings.put(Drawing{id, vectorFromJ(*dj)});
        } else if (ext == "raster") {
            RasterDrawing r;
            if (!rasterFromBytes(bytes, r)) return fail(path + ": corrupt raster");
            doc.drawings.put(Drawing{id, std::move(r)});
        }
    }
    return true;
}

bool writeDirectory(const Package& pkg, const std::string& dir, std::string* error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (const auto& [path, bytes] : pkg) {
        const fs::path p = fs::path(dir) / path;
        fs::create_directories(p.parent_path(), ec);
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        if (!f) { if (error) *error = "cannot write " + p.string(); return false; }
        f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    }
    return true;
}

bool readDirectory(const std::string& dir, Package& out, std::string* error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) { if (error) *error = "not a directory: " + dir; return false; }
    for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::ifstream f(e.path(), std::ios::binary);
        Bytes b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        out[fs::relative(e.path(), dir).generic_string()] = std::move(b);
    }
    return true;
}

}  // namespace anim::io
