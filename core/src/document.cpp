#include "anim/document.h"

namespace anim {

Document::Document() {
    Camera& cam = addCamera("Camera 1");
    cam.center.setDefault({canvas.width * 0.5f, canvas.height * 0.5f});
    cameraCuts[0] = cam.id;
}

Camera& Document::addCamera(std::string name) {
    Camera c;
    c.id = ids.make<CameraId>();
    c.name = std::move(name);
    c.center.setDefault({canvas.width * 0.5f, canvas.height * 0.5f});
    cameras.push_back(std::move(c));
    return cameras.back();
}

const Camera* Document::activeCamera(int frame) const {
    CameraId id;
    auto it = cameraCuts.upper_bound(frame);
    if (it != cameraCuts.begin()) id = std::prev(it)->second;
    for (const auto& c : cameras) if (c.id == id) return &c;
    return cameras.empty() ? nullptr : &cameras.front();
}

SymbolDef& Document::createSymbol(std::string name, SymbolKind kind) {
    SymbolDef def;
    def.id = ids.make<SymbolId>();
    def.name = std::move(name);
    def.kind = kind;
    auto [it, _] = symbols.emplace(def.id, std::move(def));
    return it->second;
}

Timeline* Document::timelineOwning(LayerId id) {
    if (timeline.find(id)) return &timeline;
    for (auto& [_, s] : symbols) if (s.timeline.find(id)) return &s.timeline;
    return nullptr;
}

Layer* Document::findLayer(LayerId id) {
    Timeline* t = timelineOwning(id);
    return t ? t->find(id) : nullptr;
}

SymbolId Document::convertToSymbol(LayerId layerId, std::string name, SymbolKind kind) {
    Timeline* owner = timelineOwning(layerId);
    if (!owner) return {};
    Layer* parent = owner->parentOf(layerId);
    if (!parent) return {};
    int index = 0;
    for (; index < int(parent->children.size()); ++index) if (parent->children[index]->id == layerId) break;
    const LayerId parentId = parent == &owner->root() ? LayerId{} : parent->id;

    auto moved = owner->detach(layerId);
    const int len = std::max(1, moved->contentLength());
    SymbolDef& sym = createSymbol(std::move(name), kind);
    sym.timeline.attach(std::move(moved));

    Layer* inst = owner->addLayer(ids, LayerKind::Symbol, sym.name, parentId, index);
    Keyframe k;
    k.instance = SymbolInstance{sym.id, LoopMode::Loop, 0};
    inst->exposure.setKey(0, k);
    inst->exposure.setLength(len);
    return sym.id;
}

}  // namespace anim
