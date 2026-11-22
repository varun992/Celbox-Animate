// Audio clips, SFX and lip sync. Decoding/playback lives in the platform layer
// (AVAudioEngine on Apple, XAudio2/WASAPI on Windows); the core owns timing,
// gain envelopes and the phoneme track that drives mouth symbols.
#pragma once
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "anim/ids.h"
#include "anim/track.h"

namespace anim {

enum class AudioRole { Dialogue, SFX, Music };

struct AudioAsset {
    AudioAssetId id;
    std::string uri;
    int sampleRate = 48000;
    int channels = 2;
    double durationSec = 0;
    std::vector<float> peaks;  // min/max pairs for the waveform overview in the timeline
};

struct AudioClip {
    AudioClipId id;
    AudioAssetId asset;
    AudioRole role = AudioRole::SFX;
    int startFrame = 0;
    double sourceInSec = 0;      // trim in
    double durationSec = 0;      // trimmed length
    AnimTrack<float> gainDb{0};  // animatable volume envelope
    float pan = 0;
    bool muted = false;

    int endFrame(double fps) const { return startFrame + int(std::ceil(durationSec * fps)); }
    bool activeAt(int frame, double fps) const { return frame >= startFrame && frame < endFrame(fps); }
    double sourceTimeAt(double frame, double fps) const { return sourceInSec + (frame - startFrame) / fps; }
};

// Mouth shapes (a common animation phoneme set).
enum class Phoneme : uint8_t { Rest, MBP, AI, E, O, U, FV, L, WQ, Etc, Count };
constexpr size_t kPhonemeCount = size_t(Phoneme::Count);

struct PhonemeKey {
    int frame;
    Phoneme phoneme;
};

// Maps each phoneme to a frame inside the mouth symbol's own timeline.
struct MouthChart {
    std::array<int, kPhonemeCount> symbolFrame{};
};

struct LipSyncTrack {
    AudioClipId clip;
    LayerId targetLayer;  // a Symbol layer whose symbol contains the mouth drawings
    MouthChart chart;
    std::vector<PhonemeKey> keys;  // hand-edited or produced by an analyzer
};

// Pluggable analyzer: an on-device model, a Rhubarb-style tool, or a cloud
// service. Async in the app; synchronous here for clarity.
class ILipSyncAnalyzer {
public:
    virtual ~ILipSyncAnalyzer() = default;
    virtual std::vector<PhonemeKey> analyze(const AudioAsset& asset, const AudioClip& clip, double fps,
                                            const std::string* transcript) = 0;
};

}  // namespace anim
