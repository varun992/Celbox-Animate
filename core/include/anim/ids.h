// Strongly typed 64-bit identifiers. Ids are stable across save/load and are
// the only way objects reference each other (no raw pointers across the model),
// which keeps undo, serialization and multi-threaded rendering simple.
#pragma once
#include <compare>
#include <cstdint>
#include <functional>

namespace anim {

template <class Tag>
struct Id {
    uint64_t value = 0;
    constexpr explicit operator bool() const { return value != 0; }
    constexpr auto operator<=>(const Id&) const = default;
};

using DrawingId    = Id<struct DrawingTag>;
using LayerId      = Id<struct LayerTag>;
using SymbolId     = Id<struct SymbolTag>;
using StyleId      = Id<struct StyleTag>;     // palette entry
using ObjectId     = Id<struct ObjectTag>;    // tracked object, stable across frames
using CameraId     = Id<struct CameraTag>;
using AudioAssetId = Id<struct AudioAssetTag>;
using AudioClipId  = Id<struct AudioClipTag>;

class IdAllocator {
public:
    template <class T>
    T make() { return T{next_++}; }
    uint64_t peek() const { return next_; }
    void reserveAbove(uint64_t v) { if (v >= next_) next_ = v + 1; }  // after load
private:
    uint64_t next_ = 1;
};

}  // namespace anim

template <class Tag>
struct std::hash<anim::Id<Tag>> {
    size_t operator()(const anim::Id<Tag>& id) const noexcept { return std::hash<uint64_t>{}(id.value); }
};
