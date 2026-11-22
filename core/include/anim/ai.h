// AI in-betweening hook (future paid feature).
//
// The core defines the request/response contract and inserts results through
// the normal undo path; providers are plug-ins: a cloud service behind the
// paywall, an on-device Core ML / DirectML model, or the free geometric
// fallback below. Entitlements are checked here, but the server must also
// verify the receipt; the client check is only for UX.
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "anim/document.h"
#include "anim/edit.h"

namespace anim::ai {

struct InbetweenRequest {
    LayerId layer;
    int startFrame = 0;             // existing key
    int endFrame = 0;               // existing key
    std::vector<int> guideFrames;   // optional user-drawn in-betweens between start and end
    int step = 1;                   // 1 = on ones, 2 = on twos
    Easing timing = Easing::linear();  // spacing chart
    bool sendLayerContext = true;   // include sibling layers (character structure)
    std::string styleHint;
};

struct GeneratedFrame {
    int frame;
    VectorDrawing drawing;  // raster providers would return RasterDrawing (later)
};

struct InbetweenResult {
    std::vector<GeneratedFrame> frames;
    std::string error;
    bool ok() const { return error.empty(); }
};

// Packed input sent to a provider: key drawings with tracked-object ids, so a
// model can match strokes across frames.
struct InbetweenPayload {
    InbetweenRequest request;
    std::vector<std::pair<int, const VectorDrawing*>> keys;  // start, guides..., end (sorted)
    std::vector<int> targetFrames;
};

class IInbetweenProvider {
public:
    virtual ~IInbetweenProvider() = default;
    virtual std::string id() const = 0;
    virtual std::string requiredEntitlement() const { return {}; }  // empty = free
    virtual InbetweenResult generate(const InbetweenPayload& payload) = 0;
};

class IEntitlements {
public:
    virtual ~IEntitlements() = default;
    virtual bool has(const std::string& feature) const = 0;
};

// Free baseline: matches strokes by ObjectId (falling back to index), resamples
// each pair to the same point count and interpolates with the timing curve.
class GeometricInbetweenProvider final : public IInbetweenProvider {
public:
    std::string id() const override { return "geometric"; }
    InbetweenResult generate(const InbetweenPayload& payload) override;
};

class InbetweenService {
public:
    explicit InbetweenService(const IEntitlements* ent = nullptr) : entitlements_(ent) {}
    void registerProvider(std::unique_ptr<IInbetweenProvider> p) { auto key = p->id(); providers_[key] = std::move(p); }

    // Validates, builds the payload, runs the provider and inserts the frames
    // as keyframes in one undoable transaction.
    InbetweenResult run(Document& doc, const InbetweenRequest& req, const std::string& providerId,
                        edit::UndoStack* undo = nullptr);

private:
    const IEntitlements* entitlements_;
    std::unordered_map<std::string, std::unique_ptr<IInbetweenProvider>> providers_;
};

}  // namespace anim::ai
