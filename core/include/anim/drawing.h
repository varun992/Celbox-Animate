// Drawing content. A Drawing is the artwork of one cel; keyframes point at
// drawings by id so a drawing can be held, reused (linked duplicate) or
// cloned (independent duplicate).
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

#include "anim/ids.h"
#include "anim/math.h"

namespace anim {

struct StrokePoint {
    Vec2 pos;
    float pressure = 1.0f;
    bool operator==(const StrokePoint&) const = default;
};

struct VectorStroke {
    std::vector<StrokePoint> points;  // centerline after smoothing
    StyleId style;
    float width = 4.0f;               // max width; pressure modulates
    ObjectId object;                  // tracked object this stroke belongs to (optional)
    bool closed = false;
};

// Result of a paint-bucket fill on vector art: a closed region with a style.
struct FillRegion {
    std::vector<Vec2> contour;
    StyleId style;
    ObjectId object;
};

struct VectorDrawing {
    std::vector<VectorStroke> strokes;
    std::vector<FillRegion> fills;

    void transform(const Affine& m) {
        for (auto& s : strokes) for (auto& p : s.points) p.pos = m.apply(p.pos);
        for (auto& f : fills) for (auto& p : f.contour) p = m.apply(p);
    }
    Rect bounds() const;
};

// Sparse, tiled raster. Tiles are shared between duplicated drawings and copied
// on first write, so duplicating a 4K frame costs almost nothing.
struct RasterTile {
    static constexpr int kSize = 256;
    std::vector<uint8_t> rgba = std::vector<uint8_t>(kSize * kSize * 4, 0);
};

class RasterDrawing {
public:
    RasterDrawing(int w = 0, int h = 0) : width_(w), height_(h) {}
    const RasterTile* tile(int tx, int ty) const {
        auto it = tiles_.find(key(tx, ty));
        return it == tiles_.end() ? nullptr : it->second.get();
    }
    RasterTile& tileForWrite(int tx, int ty) {
        auto& slot = tiles_[key(tx, ty)];
        if (!slot) slot = std::make_shared<RasterTile>();
        else if (slot.use_count() > 1) slot = std::make_shared<RasterTile>(*slot);  // copy-on-write
        return *slot;
    }
    size_t tileCount() const { return tiles_.size(); }
    template <class Fn>  // fn(tx, ty, const RasterTile&)
    void forEachTile(Fn fn) const {
        for (const auto& [k, t] : tiles_) fn(int(int32_t(k >> 32)), int(int32_t(k & 0xffffffffu)), *t);
    }
    void putTile(int tx, int ty, std::shared_ptr<RasterTile> t) { tiles_[key(tx, ty)] = std::move(t); }
    bool sharesTileWith(const RasterDrawing& o, int tx, int ty) const {
        auto a = tiles_.find(key(tx, ty)); auto b = o.tiles_.find(key(tx, ty));
        return a != tiles_.end() && b != o.tiles_.end() && a->second == b->second;
    }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    static uint64_t key(int tx, int ty) { return (uint64_t(uint32_t(tx)) << 32) | uint32_t(ty); }
    int width_, height_;
    std::unordered_map<uint64_t, std::shared_ptr<RasterTile>> tiles_;
};

struct Drawing {
    DrawingId id;
    std::variant<VectorDrawing, RasterDrawing> content;

    VectorDrawing* vector() { return std::get_if<VectorDrawing>(&content); }
    const VectorDrawing* vector() const { return std::get_if<VectorDrawing>(&content); }
    RasterDrawing* raster() { return std::get_if<RasterDrawing>(&content); }
};

class DrawingStore {
public:
    DrawingId create(IdAllocator& ids, std::variant<VectorDrawing, RasterDrawing> content) {
        const DrawingId id = ids.make<DrawingId>();
        drawings_.emplace(id, Drawing{id, std::move(content)});
        return id;
    }
    DrawingId clone(IdAllocator& ids, DrawingId src) {
        auto* d = get(src);
        if (!d) return {};
        return create(ids, d->content);  // raster tiles are shared (COW)
    }
    Drawing* get(DrawingId id) { auto it = drawings_.find(id); return it == drawings_.end() ? nullptr : &it->second; }
    const Drawing* get(DrawingId id) const { return const_cast<DrawingStore*>(this)->get(id); }
    void put(Drawing d) { drawings_.insert_or_assign(d.id, std::move(d)); }
    void erase(DrawingId id) { drawings_.erase(id); }
    size_t size() const { return drawings_.size(); }

private:
    std::unordered_map<DrawingId, Drawing> drawings_;
};

}  // namespace anim
