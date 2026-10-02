#pragma once

// QUEMAO stage 6: the shared mod sequencer (ÁCIDO style).
// 16 tempo-synced steps that move any lane's knob. Every knob has its own relative lane:
// the centre line is the knob's own value, each step pushes it up or down (scaled by DEPTH).
// The editor shows the lane of the selected TARGET; all lanes that hold values play at once.

#include "fx.h"
#include <atomic>
#include <string>
#include <cstdio>

namespace quemao {

constexpr int kModTargetsPerLane = 16;
constexpr int kNumModTargets = 4 * kModTargetsPerLane;

inline const char* modTargetName (int t) {
    static const char* n[kModTargetsPerLane] = { "TUNE", "FINE", "P.ENV", "P.DEC", "ATTACK", "DECAY", "ARTIC", "TONE",
                                                 "DRIVE", "LEVEL", "CUTOFF", "RES", "PAN", "DLY SEND", "VERB SEND", "CHOR SEND" };
    return n[((t % kModTargetsPerLane) + kModTargetsPerLane) % kModTargetsPerLane];
}
inline int modTargetLane (int t) { return std::clamp (t / kModTargetsPerLane, 0, 3); }

// Targets that are read when a hit starts (pitch, envelope, level) step cleanly;
// the others move continuously and get a short glide to avoid clicks.
inline bool modTargetIsContinuous (int t) {
    const int k = t % kModTargetsPerLane;
    return k == 7 || k == 8 || k >= 10;
}

constexpr int kNumModRates = 5;
inline const char* modRateName (int i) { static const char* n[kNumModRates] = { "1/32", "1/16", "1/8", "1/4", "1/2" }; return n[std::clamp (i, 0, kNumModRates - 1)]; }
inline double modRateBeats (int i) { static const double b[kNumModRates] = { 0.125, 0.25, 0.5, 1.0, 2.0 }; return b[std::clamp (i, 0, kNumModRates - 1)]; }

inline int modStepAt (double ppq, int rate) {
    const int64_t i = (int64_t) std::floor (ppq / modRateBeats (rate));
    return (int) (((i % 16) + 16) % 16);
}

// Real-unit range of each target (for the relative offset)
inline void modTargetRange (int t, float& lo, float& hi) {
    switch (t % kModTargetsPerLane) {
        case 0:  lo = -24.0f; hi = 24.0f; break;   // tune (st)
        case 1:  lo = -50.0f; hi = 50.0f; break;   // fine (ct)
        case 12: lo = -1.0f;  hi = 1.0f;  break;   // pan
        default: lo = 0.0f;   hi = 1.0f;  break;
    }
}

inline float* modTargetField (int t, LaneSettings& s, LaneSends& snd) {
    switch (t % kModTargetsPerLane) {
        case 0:  return &s.voice.tune;     case 1:  return &s.voice.fine;
        case 2:  return &s.voice.pitchEnv; case 3:  return &s.voice.pitchDecay;
        case 4:  return &s.voice.attack;   case 5:  return &s.voice.decay;
        case 6:  return &s.voice.artic;    case 7:  return &s.shape.tone;
        case 8:  return &s.shape.drive;    case 9:  return &s.voice.level;
        case 10: return &s.shape.cutoff;   case 11: return &s.shape.res;
        case 12: return &s.shape.pan;      case 13: return &snd.delay;
        case 14: return &snd.reverb;       default: return &snd.chorus;
    }
}

// offset in -1..1 moves the knob by up to half its range at full depth. Tune lands on whole semitones.
inline void applyMod (int t, float offset, float depth, LaneSettings& s, LaneSends& snd) {
    float lo, hi; modTargetRange (t, lo, hi);
    float* f = modTargetField (t, s, snd);
    const float norm = std::clamp ((*f - lo) / (hi - lo) + offset * 0.5f * std::clamp (depth, 0.0f, 1.0f), 0.0f, 1.0f);
    *f = lo + norm * (hi - lo);
    if (t % kModTargetsPerLane == 0) *f = std::round (*f);
}

class ModSequencer {
public:
    std::atomic<float> values[kNumModTargets][16];
    std::atomic<int> uiStep { 0 };

    ModSequencer() { clearAll(); }
    void clearAll() { for (auto& t : values) for (auto& v : t) v.store (0.0f); }
    void clearTarget (int t) { for (auto& v : values[t]) v.store (0.0f); }
    bool targetActive (int t) const { for (auto& v : values[t]) if (std::abs (v.load()) > 1.0e-4f) return true; return false; }

    void prepare (float sampleRate) { sr = sampleRate; for (auto& s : smooth) s = 0.0f; freePpq = 0.0; }

    // once per block: which targets hold values
    void scan() { numActive = 0; for (int t = 0; t < kNumModTargets; ++t) if (targetActive (t)) active[numActive++] = t; }

    // once per sample: move the active knobs. When the transport is stopped the sequencer free-runs at host tempo.
    void apply (double ppq, bool playing, double ppqPerSample, int rate, float depth, LaneSettings* s, LaneSends* snd) {
        if (! playing) { freePpq += ppqPerSample; ppq = freePpq; } else freePpq = ppq;
        const int step = modStepAt (ppq, rate);
        uiStep.store (step);
        const float glide = 1.0f - std::exp (-1.0f / (0.003f * sr));
        for (int i = 0; i < numActive; ++i) {
            const int t = active[i];
            const float target = values[t][step].load();
            float v = target;
            if (modTargetIsContinuous (t)) { smooth[t] += glide * (target - smooth[t]); v = smooth[t]; }
            else smooth[t] = target;
            const int lane = modTargetLane (t);
            applyMod (t, v, depth, s[lane], snd[lane]);
        }
    }

    // "0.25,-0.5,..." for saving; empty when the lane is flat
    std::string serialise (int t) const {
        if (! targetActive (t)) return {};
        std::string out; char buf[16];
        for (int i = 0; i < 16; ++i) { std::snprintf (buf, sizeof buf, "%s%.3f", i ? "," : "", values[t][i].load()); out += buf; }
        return out;
    }
    void deserialise (int t, const std::string& text) {
        clearTarget (t);
        int i = 0; const char* p = text.c_str();
        while (*p != 0 && i < 16) {
            char* end = nullptr; const float v = std::strtof (p, &end);
            if (end == p) break;
            values[t][i++].store (std::clamp (v, -1.0f, 1.0f));
            p = (*end == ',') ? end + 1 : end;
        }
    }

    int activeCount() const { return numActive; }

private:
    float sr = 48000.0f;
    float smooth[kNumModTargets] {};
    int active[kNumModTargets] {};
    int numActive = 0;
    double freePpq = 0.0;
};

} // namespace quemao
