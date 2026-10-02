#pragma once

// QUEMAO stage 5: send effects and output stage (pure C++, no JUCE).
//   Each lane has three sends: CHORUS, DELAY, REVERB. Returns are ADDED to the dry signal,
//   so the original hit is never dampened.
//   DELAY: one tempo-synced ping-pong line per lane (its own time, or the global TIME),
//          sharing Feedback, Tone and Mix so the four echoes sound like one space.
//   REVERB: ROOM / PLATE / HALL, Size, Damp, Mix.
//   OUTPUT: COMP (punchy bus compressor), CRUSH (bits + sample rate), GAIN.

#include "lane.h"
#include <vector>

namespace quemao {

// ---- tempo divisions for the delay ----
constexpr int kNumDivisions = 10;
inline const char* divisionName (int i) {
    static const char* n[kNumDivisions] = { "1/32", "1/16", "1/16D", "1/8T", "1/8", "3/16", "1/4T", "1/4", "3/8", "1/2" };
    return n[std::clamp (i, 0, kNumDivisions - 1)];
}
inline double divisionBeats (int i) {
    static const double b[kNumDivisions] = { 0.125, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
    return b[std::clamp (i, 0, kNumDivisions - 1)];
}
constexpr int kDefaultDivision = 5;   // 3/16

enum class ReverbType : int { room = 0, plate = 1, hall = 2 };
inline const char* reverbTypeName (int t) { static const char* n[3] = { "ROOM", "PLATE", "HALL" }; return n[std::clamp (t, 0, 2)]; }

struct LaneSends {
    float chorus = 0.0f, delay = 0.0f, reverb = 0.0f;   // 0..1
    int delayTime = 0;                                  // 0 = GLOBAL, 1.. = division index + 1
};

struct FxSettings {
    float chorusTone = 0.6f, chorusRate = 0.3f, chorusMix = 0.5f;
    int delayTime = kDefaultDivision;
    float delayFeedback = 0.45f, delayTone = 0.6f, delayMix = 0.5f;
    int reverbType = (int) ReverbType::plate;
    float reverbSize = 0.55f, reverbDamp = 0.4f, reverbMix = 0.5f;
    float comp = 0.0f, crush = 0.0f, gain = 0.8f;
};

// ---- small helpers ----
struct OnePole {
    float z = 0.0f, c = 1.0f;
    void setLowpass (float sr, float hz) { c = 1.0f - std::exp (-kTwoPi * std::min (hz, sr * 0.45f) / sr); }
    float lp (float x) { z += c * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
};

class DelayBuffer {
public:
    void resize (int n) { buf.assign ((size_t) std::max (n, 4), 0.0f); pos = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) pos] = x; if (++pos >= (int) buf.size()) pos = 0; }
    // read 'delay' samples behind the newest sample (fractional, linear interpolation)
    float read (float delay) const {
        const int n = (int) buf.size();
        delay = std::clamp (delay, 1.0f, (float) (n - 2));
        float rp = (float) pos - 1.0f - delay;
        while (rp < 0.0f) rp += (float) n;
        const int i0 = (int) rp, i1 = (i0 + 1) % n;
        const float f = rp - (float) i0;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
    }
private:
    std::vector<float> buf;
    int pos = 0;
};

// ---- chorus: two modulated lines, stereo ----
class Chorus {
public:
    void prepare (float sampleRate) { sr = sampleRate; l.resize ((int) (0.05f * sr)); r.resize ((int) (0.05f * sr)); phase = 0; tl.z = tr.z = 0; }
    void process (float inL, float inR, const FxSettings& s, float& outL, float& outR) {
        const float rate = 0.1f + s.chorusRate * s.chorusRate * 4.9f;     // 0.1 .. 5 Hz
        phase += kTwoPi * rate / sr; if (phase > kTwoPi) phase -= kTwoPi;
        const float base = 0.012f * sr, depth = 0.004f * sr;
        l.push (inL); r.push (inR);
        float wl = l.read (base + depth * std::sin (phase));
        float wr = r.read (base + depth * std::sin (phase + 1.5708f));
        const float hz = 1500.0f + s.chorusTone * s.chorusTone * 14000.0f;
        tl.setLowpass (sr, hz); tr.setLowpass (sr, hz);
        outL = tl.lp (wl); outR = tr.lp (wr);
    }
private:
    float sr = 48000.0f, phase = 0.0f;
    DelayBuffer l, r;
    OnePole tl, tr;
};

// ---- one lane's ping-pong delay ----
class LaneDelay {
public:
    void prepare (float sampleRate) {
        sr = sampleRate;
        const int n = (int) (2.2f * sr);   // 1/2 note down to ~55 BPM
        l.resize (n); r.resize (n); cur = -1.0f;
        lpL.z = lpR.z = hpL.z = hpR.z = 0.0f;
    }
    void process (float in, double delaySamples, const FxSettings& s, float& outL, float& outR) {
        const float target = (float) std::min (delaySamples, (double) sr * 2.1);
        if (cur < 0.0f) cur = target;
        cur += 0.0008f * (target - cur);                         // glide instead of clicking on changes
        const float hz = 1200.0f + s.delayTone * s.delayTone * 12000.0f;
        lpL.setLowpass (sr, hz); lpR.setLowpass (sr, hz);
        hpL.setLowpass (sr, 140.0f); hpR.setLowpass (sr, 140.0f);
        const float fb = std::clamp (s.delayFeedback, 0.0f, 1.0f) * 0.92f;
        const float yl = l.read (cur), yr = r.read (cur);
        // ping-pong: the input goes left, each repeat crosses sides
        l.push (in + hpL.hp (lpL.lp (yr)) * fb);
        r.push (hpR.hp (lpR.lp (yl)) * fb);
        outL = yl; outR = yr;
    }
    float currentDelay() const { return cur; }
private:
    float sr = 48000.0f, cur = -1.0f;
    DelayBuffer l, r;
    OnePole lpL, lpR, hpL, hpR;
};

// ---- reverb: Freeverb-style network with three characters ----
class Reverb {
public:
    void prepare (float sampleRate) {
        sr = sampleRate;
        builtType = -1;
        pre.resize ((int) (0.06f * sr));
    }
    void process (float inL, float inR, const FxSettings& s, float& outL, float& outR) {
        if (s.reverbType != builtType) build (s.reverbType);
        const int t = builtType;
        // size / damp mapped per type
        const float sizeScale = t == 2 ? 1.0f : t == 1 ? 0.95f : 0.85f;
        const float fb = std::clamp (0.70f + std::clamp (s.reverbSize, 0.0f, 1.0f) * 0.27f * sizeScale, 0.0f, 0.985f);
        const float damp = std::clamp (s.reverbDamp, 0.0f, 1.0f) * (t == 1 ? 0.55f : 0.85f);
        const float preDelay = (t == 2 ? 0.025f : t == 0 ? 0.006f : 0.0f) * sr;

        const float mono = (inL + inR) * 0.5f;
        pre.push (mono);
        const float x = (preDelay > 1.0f ? pre.read (preDelay) : mono) * 0.06f;

        float sl = 0.0f, sr2 = 0.0f;
        for (int c = 0; c < kCombs; ++c) { sl += combL[c].process (x, fb, damp); sr2 += combR[c].process (x, fb, damp); }
        for (int a = 0; a < numAllpass; ++a) { sl = apL[a].process (sl); sr2 = apR[a].process (sr2); }
        outL = sl; outR = sr2;
    }
private:
    struct Comb {
        std::vector<float> b; int p = 0; float store = 0.0f;
        void init (int n) { b.assign ((size_t) std::max (n, 2), 0.0f); p = 0; store = 0.0f; }
        float process (float in, float fb, float damp) {
            const float y = b[(size_t) p];
            store = y * (1.0f - damp) + store * damp;
            b[(size_t) p] = in + store * fb;
            if (++p >= (int) b.size()) p = 0;
            return y;
        }
    };
    struct Allpass {
        std::vector<float> b; int p = 0;
        void init (int n) { b.assign ((size_t) std::max (n, 2), 0.0f); p = 0; }
        float process (float in) {
            const float y = b[(size_t) p];
            b[(size_t) p] = in + y * 0.5f;
            if (++p >= (int) b.size()) p = 0;
            return y - in;
        }
    };
    void build (int t) {
        builtType = std::clamp (t, 0, 2);
        static const int combT[kCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apT[6] = { 556, 441, 341, 225, 180, 127 };
        const float scale = (sr / 44100.0f) * (builtType == 2 ? 1.45f : builtType == 0 ? 0.7f : 1.0f);
        for (int c = 0; c < kCombs; ++c) { combL[c].init ((int) (combT[c] * scale)); combR[c].init ((int) ((combT[c] + 23) * scale)); }
        numAllpass = builtType == 1 ? 6 : 4;                      // the plate is denser
        for (int a = 0; a < numAllpass; ++a) { apL[a].init ((int) (apT[a] * scale)); apR[a].init ((int) ((apT[a] + 23) * scale)); }
    }
    static constexpr int kCombs = 8;
    float sr = 48000.0f;
    int builtType = -1, numAllpass = 4;
    Comb combL[kCombs], combR[kCombs];
    Allpass apL[6], apR[6];
    DelayBuffer pre;
};

// ---- output stage ----
class OutputStage {
public:
    void prepare (float sampleRate) { sr = sampleRate; env = 0.0f; holdL = holdR = 0.0f; holdCount = 0; }
    void process (float& l, float& r, const FxSettings& s) {
        if (s.comp > 0.001f) {
            const float c = std::clamp (s.comp, 0.0f, 1.0f);
            const float level = std::max (std::abs (l), std::abs (r));
            const float att = std::exp (-1.0f / (0.0015f * sr)), rel = std::exp (-1.0f / (0.09f * sr));
            env = level > env ? att * env + (1.0f - att) * level : rel * env + (1.0f - rel) * level;
            const float thrDb = -6.0f - 24.0f * c, ratio = 1.0f + 7.0f * c;
            const float envDb = 20.0f * std::log10 (std::max (env, 1.0e-6f));
            const float overDb = std::max (0.0f, envDb - thrDb);
            const float grDb = overDb * (1.0f - 1.0f / ratio);
            const float makeupDb = -thrDb * (1.0f - 1.0f / ratio) * 0.8f;
            const float g = std::pow (10.0f, (makeupDb - grDb) / 20.0f);
            l *= g; r *= g;
        }
        if (s.crush > 0.001f) {
            const float c = std::clamp (s.crush, 0.0f, 1.0f);
            const int hold = 1 + (int) (c * c * 15.0f);
            if (++holdCount >= hold) { holdCount = 0; holdL = l; holdR = r; }
            const float steps = std::pow (2.0f, 16.0f - 12.0f * c) * 0.5f;
            const float ql = std::round (holdL * steps) / steps, qr = std::round (holdR * steps) / steps;
            const float wet = std::min (1.0f, c * 3.0f);
            l = l + (ql - l) * wet; r = r + (qr - r) * wet;
        }
        l = std::tanh (l * s.gain); r = std::tanh (r * s.gain);
    }
    float envelope() const { return env; }
private:
    float sr = 48000.0f, env = 0.0f, holdL = 0, holdR = 0;
    int holdCount = 0;
};

// ---- the mixer: dry lanes + sends + returns + output ----
class Mixer {
public:
    void prepare (float sampleRate) {
        sr = sampleRate;
        chorus.prepare (sr); reverb.prepare (sr); out.prepare (sr);
        for (auto& d : delays) d.prepare (sr);
    }
    // x = each lane's mono output for this sample
    void process (const float* x, const LaneSettings* s, const LaneSends* sends, const FxSettings& fx, double bpm,
                  float& outL, float& outR) {
        float dl = 0, dr = 0, cl = 0, cr = 0, rl = 0, rr = 0, el = 0, er = 0;   // dry, chorus, reverb, echoes
        const double beat = sr * 60.0 / std::max (bpm, 20.0);
        for (int i = 0; i < 4; ++i) {
            float gl, gr; panGains (s[i].shape.pan, gl, gr);
            const float pl = x[i] * gl, pr = x[i] * gr;
            dl += pl; dr += pr;
            cl += pl * sends[i].chorus; cr += pr * sends[i].chorus;
            rl += pl * sends[i].reverb; rr += pr * sends[i].reverb;
            const int div = sends[i].delayTime == 0 ? fx.delayTime : sends[i].delayTime - 1;
            float yl, yr;
            delays[i].process (x[i] * sends[i].delay, divisionBeats (div) * beat, fx, yl, yr);
            el += yl; er += yr;     // ping-pong returns, mixed wide
        }

        float wcl, wcr, wrl, wrr;
        chorus.process (cl, cr, fx, wcl, wcr);
        reverb.process (rl + el * 0.3f * fx.delayMix, rr + er * 0.3f * fx.delayMix, fx, wrl, wrr);

        float l = dl + wcl * fx.chorusMix * 1.2f + el * fx.delayMix * 1.2f + wrl * fx.reverbMix * 1.6f;
        float r = dr + wcr * fx.chorusMix * 1.2f + er * fx.delayMix * 1.2f + wrr * fx.reverbMix * 1.6f;
        l *= 0.45f; r *= 0.45f;
        out.process (l, r, fx);
        outL = l; outR = r;
    }
    const LaneDelay& delay (int i) const { return delays[i]; }
private:
    float sr = 48000.0f;
    Chorus chorus;
    LaneDelay delays[4];
    Reverb reverb;
    OutputStage out;
};

} // namespace quemao
