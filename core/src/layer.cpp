#include "anim/layer.h"

#include <algorithm>
#include <functional>

namespace anim {

std::unique_ptr<Layer> Layer::clone() const {
    auto l = std::make_unique<Layer>();
    l->id = id; l->name = name; l->kind = kind; l->visible = visible; l->locked = locked;
    l->clipToBelow = clipToBelow; l->animationFolder = animationFolder; l->blend = blend;
    l->timelineTint = timelineTint; l->transform = transform; l->effects = effects;
    l->exposure = exposure; l->audioClips = audioClips; l->model = model;
    for (const auto& c : children) l->children.push_back(c->clone());
    return l;
}

int Layer::contentLength() const {
    int len = exposure.length();
    for (const auto& c : children) len = std::max(len, c->contentLength());
    return len;
}

Timeline::Timeline() : root_(std::make_unique<Layer>()) {
    root_->kind = LayerKind::Folder;
    root_->name = "Root";
}

namespace {
Layer* findIn(Layer& l, LayerId id) {
    if (l.id == id) return &l;
    for (auto& c : l.children) if (auto* r = findIn(*c, id)) return r;
    return nullptr;
}
Layer* findParentIn(Layer& l, LayerId id) {
    for (auto& c : l.children) {
        if (c->id == id) return &l;
        if (auto* r = findParentIn(*c, id)) return r;
    }
    return nullptr;
}
}  // namespace

Layer* Timeline::find(LayerId id) { return id ? findIn(*root_, id) : root_.get(); }
Layer* Timeline::parentOf(LayerId id) { return findParentIn(*root_, id); }

Layer* Timeline::addLayer(IdAllocator& ids, LayerKind kind, std::string name, LayerId parent, int index) {
    auto l = std::make_unique<Layer>();
    l->id = ids.make<LayerId>();
    l->kind = kind;
    l->name = std::move(name);
    Layer* raw = l.get();
    return attach(std::move(l), parent, index) ? raw : nullptr;
}

std::unique_ptr<Layer> Timeline::detach(LayerId id) {
    Layer* p = parentOf(id);
    if (!p) return nullptr;
    auto it = std::find_if(p->children.begin(), p->children.end(), [&](auto& c) { return c->id == id; });
    auto out = std::move(*it);
    p->children.erase(it);
    return out;
}

bool Timeline::attach(std::unique_ptr<Layer> layer, LayerId parent, int index) {
    Layer* p = find(parent);
    if (!p || !p->isFolder()) return false;
    if (index < 0 || index > int(p->children.size())) index = int(p->children.size());
    p->children.insert(p->children.begin() + index, std::move(layer));
    return true;
}

bool Timeline::move(LayerId id, LayerId newParent, int index) {
    if (id == newParent) return false;
    Layer* moving = find(id);
    if (!moving || (newParent && findIn(*moving, newParent))) return false;  // no cycles
    auto l = detach(id);
    if (!l) return false;
    return attach(std::move(l), newParent, index);
}

}  // namespace anim
