// Document palette. Strokes and fills reference a StyleId rather than storing
// a color, so recoloring a style once recolors that object on every frame
// (the OpenToonz "studio palette" idea, and requirement #1's "change color once").
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "anim/ids.h"
#include "anim/math.h"

namespace anim {

struct TextureRef {
    std::string assetUri;  // image in the document bundle
    enum class Mode { Tile, Stretch, ScreenSpace } mode = Mode::Tile;
    float scale = 1.0f, rotationDeg = 0.0f, strength = 1.0f;
};

struct Style {
    StyleId id;
    std::string name;
    Color color;
    std::optional<TextureRef> texture;
    bool locked = false;
};

class Palette {
public:
    StyleId add(IdAllocator& ids, std::string name, Color color) {
        Style s{ids.make<StyleId>(), std::move(name), color, std::nullopt, false};
        styles_.push_back(s);
        return s.id;
    }
    Style* find(StyleId id) {
        for (auto& s : styles_) if (s.id == id) return &s;
        return nullptr;
    }
    const Style* find(StyleId id) const { return const_cast<Palette*>(this)->find(id); }
    bool setColor(StyleId id, Color c) {
        auto* s = find(id);
        if (!s || s->locked) return false;
        s->color = c;
        return true;
    }
    Color resolve(StyleId id) const { auto* s = find(id); return s ? s->color : Color::black(); }
    const std::vector<Style>& styles() const { return styles_; }

private:
    std::vector<Style> styles_;
};

}  // namespace anim
