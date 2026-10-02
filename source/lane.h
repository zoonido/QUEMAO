#pragma once

// QUEMAO stage 1: one lane = 16-step pattern + clock + a small pool of drum voices.
// MIDI is switchable per lane:
//   LAUNCH  - note-on starts the pattern from step 1 instantly (or restarts it), note-off stops it.
//             LATCH makes the lane follow the host transport, locked to the grid, no notes needed.
//   TRIGGER - the lane's note plays the drum directly, with velocity (pad mode). The pattern rests.

#include "engine.h"
#include <atomic>
#include <climits>
#include <cstring>

namespace quemao {

enum StepState : uint8_t { stepOff = 0, stepHit = 1, stepGhost = 2, stepRatchet = 3 };

inline float stepVelocity (uint8_t s) {
    switch (s) {
        case stepHit:     return 1.0f;
        case stepGhost:   return 0.4f;
        case stepRatchet: return 0.88f;
        default:          return 0.0f;
    }
}

inline int64_t floorDiv (int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// The clock ticks in half-steps (1/32 notes). Every step fires on its first half;
// a ratchet also fires on its second half.
inline int stepOfHalf (int64_t half) {
    const int64_t q = floorDiv (half, 2);
    return (int) (((q % 16) + 16) % 16);
}
inline bool halfFires (int64_t half, uint8_t state) {
    const bool second = (half - floorDiv (half, 2) * 2) == 1;
    return second ? state == stepRatchet : state != stepOff;
}

// ---- stage 3: per-lane shaping after the voices: drive -> tone -> filter (pan is applied by the mixer)

struct LaneShape {
    float drive = 0.0f;      // 0..1
    float tone = 0.5f;       // 0..1, 0.5 = flat, lower = darker, higher = brighter
    float cutoff = 1.0f;     // 0..1, fully right = open in both LP and HP
    float res = 0.1f;        // 0..1
    bool highpass = false;
    float pan = 0.0f;        // -1..1
    int choke = 0;           // 0 = none, 1 = A, 2 = B
};

inline float cutoffHz (float knob, bool highpass) {
    const float k = std::clamp (highpass ? 1.0f - knob : knob, 0.0f, 1.0f);
    return 20.0f * std::pow (1000.0f, k);            // 20 Hz .. 20 kHz
}
inline float resonanceQ (float r) { r = std::clamp (r, 0.0f, 1.0f); return 0.5f + r * r * 11.5f; }
// equal-power pan, unity at centre
inline void panGains (float pan, float& l, float& r) {
    const float a = (std::clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.785398163f;   // 0 .. pi/2
    l = std::cos (a) * 1.41421356f;
    r = std::sin (a) * 1.41421356f;
}

class LaneFx {
public:
    void prepare (float sampleRate) { sr = sampleRate; tilt = 0; ic1 = ic2 = 0; curHz = -1.0f; smoothHz = -1.0f; }

    float process (float x, const LaneShape& s) {
        // drive
        if (s.drive > 0.001f) {
            const float dk = 1.0f + 9.0f * s.drive;
            x = std::tanh (x * dk) / std::sqrt (dk);
        }
        // tone: tilt around ~800 Hz
        const float c = 1.0f - std::exp (-kTwoPi * 800.0f / sr);
        tilt += c * (x - tilt);
        const float t = std::clamp (s.tone, 0.0f, 1.0f) - 0.5f;
        x = tilt * (1.0f - t * 1.2f) + (x - tilt) * std::max (0.0f, 1.0f + t * 2.4f);
        // filter: TPT state-variable, smoothed cutoff
        const float target = cutoffHz (s.cutoff, s.highpass);
        if (smoothHz < 0.0f) smoothHz = target;
        smoothHz += 0.002f * (target - smoothHz);
        const float q = resonanceQ (s.res);
        if (std::abs (smoothHz - curHz) > 0.001f * curHz || std::abs (q - curQ) > 1.0e-6f || s.highpass != curHp) {
            curHz = smoothHz; curQ = q; curHp = s.highpass;
            const float g = std::tan (3.14159265f * std::min (curHz, sr * 0.45f) / sr);
            k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        return s.highpass ? x - k * v1 - v2 : v2;
    }

private:
    float sr = 48000.0f, tilt = 0, ic1 = 0, ic2 = 0;
    float curHz = -1.0f, smoothHz = -1.0f, curQ = 0.0f, k = 1, a1 = 0, a2 = 0, a3 = 0;
    bool curHp = false;
};

struct LaneSettings {
    VoiceParams voice;
    LaneShape shape;
    bool trigger = false;
    bool latch = false;
    // stage 4: playback feel
    float prob = 1.0f;    // 0..1 chance that each pattern hit plays
    float human = 0.0f;   // 0..1 timing (0..12 ms, laid back) and velocity (+-20%) drift
    float swing = 0.0f;   // 0..1 delays the off 16ths by up to half a step (50% .. 75% swing)
};

inline double swingDelaySamples (float swing, double stepSamples) { return std::clamp (swing, 0.0f, 1.0f) * 0.5 * stepSamples; }

enum LaneStatus : int { statusIdle = 0, statusHeld = 1, statusLatched = 2, statusTrigger = 3 };

class Lane {
public:
    static constexpr int kVoices = 4;
    std::atomic<uint8_t> steps[16];
    std::atomic<bool> locks[16];
    std::atomic<int> uiStep { -1 };
    std::atomic<int> uiStatus { statusIdle };
    std::atomic<int> uiFlash { 0 };   // counts hits, the editor flashes the LED on change

    Lane() { for (auto& s : steps) s.store (stepOff); for (auto& l : locks) l.store (false); }

    void setLocks (const char* p) {
        const int len = p != nullptr ? (int) std::strlen (p) : 0;
        for (int i = 0; i < 16; ++i) locks[i].store (i < len && p[i] == '1');
    }
    void lockString (char* out) const { for (int i = 0; i < 16; ++i) out[i] = locks[i].load() ? '1' : '0'; out[16] = 0; }

    void prepare (float sampleRate) {
        sr = sampleRate; held = false; wasPlaying = false; lastHalf = INT64_MIN; counter = 0.0;
        fx.prepare (sampleRate);
        numPending = 0;
    }

    // true once after this lane struck a hit (used for choke groups)
    bool consumeFired() { const bool f = fired; fired = false; return f; }
    void chokeAll() { for (auto& v : voices) v.choke(); }

    void setPattern (const char* p) {
        const int len = p != nullptr ? (int) std::strlen (p) : 0;
        for (int i = 0; i < 16; ++i) {
            const char c = i < len ? p[i] : '.';
            steps[i].store (c == 'x' ? stepHit : c == 'g' ? stepGhost : c == 'r' ? stepRatchet : stepOff);
        }
    }

    void patternString (char* out) const {   // out must hold 17 chars
        for (int i = 0; i < 16; ++i) {
            const auto s = steps[i].load();
            out[i] = s == stepHit ? 'x' : s == stepGhost ? 'g' : s == stepRatchet ? 'r' : '.';
        }
        out[16] = 0;
    }

    void noteOn (float velocity, const LaneSettings& s) {
        if (s.trigger) { fire (s.voice, velocity); return; }
        held = true; counter = 0.0; lastHalf = INT64_MIN;
    }
    void noteOff() { held = false; }

    // Called once per sample. ppq = host position at this sample (quarter notes).
    float process (const LaneSettings& s, double bpm, bool playing, double ppq) {
        if (s.trigger) {
            held = false;
            uiStatus.store (statusTrigger); uiStep.store (-1);
        } else if (s.latch) {
            if (playing) {
                const int64_t h = (int64_t) std::floor (ppq * 8.0);
                if (! wasPlaying) {
                    const double frac = ppq * 8.0 - (double) h;
                    lastHalf = frac < 1.0e-3 ? h - 1 : h;   // starting on a boundary fires it, otherwise wait for the next
                }
                if (h != lastHalf) { lastHalf = h; onHalf (h, s, (double) sr * 60.0 / std::max (bpm, 20.0) / 4.0); }
                uiStatus.store (statusLatched);
            } else {
                uiStatus.store (statusIdle); uiStep.store (-1);
            }
        } else if (held) {
            const double halfLen = (double) sr * 60.0 / std::max (bpm, 20.0) / 8.0;
            const int64_t h = (int64_t) std::floor (counter / halfLen);
            if (h != lastHalf) { lastHalf = h; onHalf (h, s, halfLen * 2.0); }
            counter += 1.0;
            uiStatus.store (statusHeld);
        } else {
            uiStatus.store (statusIdle); uiStep.store (-1);
        }
        wasPlaying = playing;

        // hits delayed by swing / humanize
        for (int i = 0; i < numPending; ) {
            if (--pending[i].delay <= 0) { fire (s.voice, pending[i].vel); pending[i] = pending[--numPending]; }
            else ++i;
        }

        float out = 0.0f;
        for (auto& v : voices) out += v.process();
        return fx.process (out, s.shape);
    }

    void fire (const VoiceParams& p, float velocity) {
        DrumVoice* target = nullptr;
        for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
        if (target == nullptr) {
            target = &voices[0];
            for (auto& v : voices) if (v.energy() < target->energy()) target = &v;
        }
        target->trigger (p, velocity, sr);
        uiFlash.fetch_add (1);
        fired = true;
    }

private:
    void onHalf (int64_t h, const LaneSettings& s, double stepSamples) {
        const int step = stepOfHalf (h);
        uiStep.store (step);
        const uint8_t st = steps[step].load();
        if (! halfFires (h, st)) return;

        const bool firstHalf = (h - floorDiv (h, 2) * 2) == 0;
        if (firstHalf) hitPlays = rand01() < s.prob;        // a ratchet's second hit follows its first
        if (! hitPlays) return;

        double delay = (step % 2 == 1) ? swingDelaySamples (s.swing, stepSamples) : 0.0;
        float vel = stepVelocity (st);
        if (s.human > 0.0f) {
            delay += rand01() * s.human * 0.012 * sr;
            vel = std::clamp (vel * (1.0f + (rand01() - 0.5f) * 0.4f * s.human), 0.05f, 1.0f);
        }
        const int d = (int) std::lround (delay);
        if (d < 1 || numPending >= kMaxPending) fire (s.voice, vel);
        else pending[numPending++] = { d, vel };
    }

    float rand01() { return rng.next() * 0.5f + 0.5f; }

    struct Pending { int delay; float vel; };
    static constexpr int kMaxPending = 16;
    Pending pending[kMaxPending] {};
    int numPending = 0;
    bool hitPlays = true;
    Rng rng;

    DrumVoice voices[kVoices];
    LaneFx fx;
    bool fired = false;
    float sr = 48000.0f;
    bool held = false, wasPlaying = false;
    double counter = 0.0;
    int64_t lastHalf = INT64_MIN;
};

// Factory defaults for stage 1 (the tribal-techno take: RITUAL 132)
struct LaneDefault { const char* name; Engine engine; bool trigger; float tune; float decay; float artic; const char* pattern; };
inline const LaneDefault kLaneDefaults[4] = {
    { "TOM GRAVE", Engine::membrana, false, -7.0f, 0.55f, 0.30f, "x..x..x...x.x..g" },
    { "CONGA",     Engine::mano,     false,  0.0f, 0.50f, 0.50f, ".gx.g.xg.gx.gxrg" },
    { "TOM ALTO",  Engine::membrana, false,  5.0f, 0.45f, 0.55f, "......x.....x.r." },
    { "BONGO",     Engine::mano,     true,   7.0f, 0.35f, 0.65f, "..g.x..g..g.x..." },
};

// Default pans: a slight spread
inline const float kLanePans[4] = { 0.0f, -0.25f, 0.2f, 0.3f };

// Lane notes: C1, D1, E1, F1 (Ableton naming, C3 = 60)
inline const int kLaneNotes[4] = { 36, 38, 40, 41 };

} // namespace quemao
