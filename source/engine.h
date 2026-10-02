#pragma once

// QUEMAO stage 2: the drum voice with all six engines.
//   MEMBRANA  drumhead model           Artic = strike position, center -> edge
//   MANO      hand drum                Artic = muted -> open -> slap
//   CAJA      snare: body + wires      Artic = cross-stick -> normal -> rimshot
//   ANALOGO   drum-machine circuits    Artic = TOM | SNARE | RIM | CLAP (four zones)
//   MADERA    wood: shell + plucked knock   Artic = shell -> rim
//   FM        two-operator FM          Artic = FM ratio and brightness, round -> metallic
// Pure C++ (no JUCE) so the engine checks and the preview tool can use it.

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace quemao {

constexpr float kTwoPi = 6.28318530718f;

struct Rng {
    uint32_t s = 0x9E3779B9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) s / 2147483648.0f - 1.0f; }
};

// RBJ biquad
struct Biquad {
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setBandpass (float sr, float f, float q) {
        f = std::min (f, sr * 0.45f);
        const float w = kTwoPi * f / sr, al = std::sin (w) / (2.0f * q), a0 = 1.0f + al;
        b0 = al / a0; b1 = 0.0f; b2 = -al / a0; a1 = -2.0f * std::cos (w) / a0; a2 = (1.0f - al) / a0;
    }
    void setLowpass (float sr, float f, float q) {
        f = std::min (f, sr * 0.45f);
        const float w = kTwoPi * f / sr, al = std::sin (w) / (2.0f * q), c = std::cos (w), a0 = 1.0f + al;
        b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    void setHighpass (float sr, float f, float q) {
        f = std::min (f, sr * 0.45f);
        const float w = kTwoPi * f / sr, al = std::sin (w) / (2.0f * q), c = std::cos (w), a0 = 1.0f + al;
        b0 = (1.0f + c) * 0.5f / a0; b1 = -(1.0f + c) / a0; b2 = b0; a1 = -2.0f * c / a0; a2 = (1.0f - al) / a0;
    }
    void reset() { z1 = z2 = 0.0f; }
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

enum class Engine : int { membrana = 0, mano = 1, caja = 2, analogo = 3, madera = 4, fm = 5 };
constexpr int kNumEngines = 6;
inline const char* engineName (Engine e) {
    static const char* names[kNumEngines] = { "MEMBRANA", "MANO", "CAJA", "ANALOGO", "MADERA", "FM" };
    return names[(int) e];
}
inline const char* articLabel (Engine e) {
    static const char* names[kNumEngines] = { "EDGE", "ARTIC", "STROKE", "TYPE", "RIM", "METAL" };
    return names[(int) e];
}
// ANALOGO: which circuit the Artic knob selects
inline int analogType (float artic) { return std::clamp ((int) (artic * 4.0f), 0, 3); }
inline const char* analogTypeName (int t) {
    static const char* names[4] = { "TOM", "SNARE", "RIM", "CLAP" };
    return names[std::clamp (t, 0, 3)];
}

struct VoiceParams {
    Engine engine = Engine::membrana;
    float tune = 0.0f;   // semitones, -24..+24
    float fine = 0.0f;   // cents, -50..+50
    float decay = 0.5f;  // 0..1
    float artic = 0.5f;  // 0..1, meaning depends on the engine (see top)
    float level = 0.8f;  // 0..1
    float pitchEnv = 0.5f;    // 0..1, 0.5 = the engine's own sweep, 0 = none, 1 = 4x deeper
    float pitchDecay = 0.5f;  // 0..1, 0.5 = the engine's own sweep time, 0 = 4x faster, 1 = 4x slower
    float attack = 0.0f;      // 0..1 -> 0..30 ms fade-in that softens the strike
};

inline float pitchEnvFactor (float k) { k = std::clamp (k, 0.0f, 1.0f); return k < 0.5f ? k * 2.0f : 1.0f + (k - 0.5f) * 6.0f; }
inline float pitchDecayFactor (float k) { return std::pow (2.0f, (std::clamp (k, 0.0f, 1.0f) - 0.5f) * 4.0f); }
inline float attackSeconds (float k) { return std::clamp (k, 0.0f, 1.0f) * 0.03f; }

// Pitch at Tune 0 for each engine
inline float baseFrequency (Engine e) {
    switch (e) {
        case Engine::membrana: return 110.0f;   // A2
        case Engine::mano:     return 220.0f;   // A3
        case Engine::caja:     return 185.0f;   // F#3
        case Engine::analogo:  return 110.0f;   // A2
        case Engine::madera:   return 220.0f;   // A3
        case Engine::fm:       return 110.0f;   // A2
    }
    return 110.0f;
}

inline float voiceFrequency (const VoiceParams& p) {
    return baseFrequency (p.engine) * std::pow (2.0f, (p.tune + p.fine * 0.01f) / 12.0f);
}

// A filtered, enveloped noise source. Burst mode retriggers the envelope (hand clap).
struct NoiseLayer {
    enum Type { bandpass, highpass, lowpass };
    float amp = 0, envv = 0, mul = 0, tailMul = 0;
    int bursts = 0, burstGap = 0, burstCounter = 0;
    Biquad f;
    void set (float sr, Type t, float freq, float q, float a, float tau, int nBursts = 0, float gapSec = 0.0f, float tailTau = 0.0f) {
        amp = a; envv = a > 0.0f ? 1.0f : 0.0f;
        mul = std::exp (-1.0f / (std::max (tau, 0.0005f) * sr));
        if (t == bandpass) f.setBandpass (sr, freq, q); else if (t == highpass) f.setHighpass (sr, freq, q); else f.setLowpass (sr, freq, q);
        f.reset();
        bursts = nBursts; burstGap = (int) (gapSec * sr); burstCounter = 0;
        tailMul = std::exp (-1.0f / (std::max (tailTau, 0.0005f) * sr));
    }
    float process (float n) {
        if (amp <= 0.0f) return 0.0f;
        if (bursts > 0 && ++burstCounter >= burstGap) { burstCounter = 0; --bursts; envv = 1.0f; if (bursts == 0) mul = tailMul; }
        const float y = f.process (n) * envv * amp;
        envv *= mul;
        return y;
    }
    bool silent() const { return amp <= 0.0f || (envv < 1.0e-4f && bursts == 0); }
};

class DrumVoice {
public:
    static constexpr int kModes = 8;

    bool isActive() const { return active; }
    float energy() const { return active ? std::max ({ env[0], fmEnv, ksEnergy, noiseA.envv }) : 0.0f; }

    void trigger (const VoiceParams& p, float velocity, float sampleRate) {
        sr = sampleRate;
        const float f0 = voiceFrequency (p);
        const float d = std::clamp (p.decay, 0.0f, 1.0f);
        const float a = std::clamp (p.artic, 0.0f, 1.0f);
        gain = std::clamp (p.level, 0.0f, 1.0f) * std::clamp (velocity, 0.0f, 1.0f);

        // reset every layer, then each engine switches on what it uses
        nModes = 0; pitchAmt = 0.0f; pitchTau = 0.02f; muteAmt = 0.0f; drive = 1.1f;
        noiseA.set (sr, NoiseLayer::bandpass, 1000.0f, 1.0f, 0.0f, 0.01f);
        noiseB.set (sr, NoiseLayer::bandpass, 1000.0f, 1.0f, 0.0f, 0.01f);
        fmOn = false; fmEnv = 0.0f; ksOn = false; ksEnergy = 0.0f;

        switch (p.engine) {
            case Engine::membrana: {
                // circular membrane modes (0,1) (1,1) (2,1) (0,2) (3,1)
                const float r[]  = { 1.0f, 1.593f, 2.135f, 2.295f, 2.653f };
                const float am[] = { 1.0f, 0.45f, 0.26f, 0.16f, 0.10f };
                const float tau = 0.06f + std::pow (d, 1.5f) * 1.1f;
                const float edge = 0.35f + 1.5f * a;
                for (int k = 0; k < 5; ++k)
                    addMode (f0 * r[k], k == 0 ? am[k] * (1.25f - 0.6f * a) : am[k] * edge, tau / (1.0f + 0.9f * (float) k));
                pitchAmt = 0.55f; pitchTau = 0.032f;
                noiseA.set (sr, NoiseLayer::bandpass, 900.0f + 2200.0f * a, 0.9f, (0.18f + 0.45f * a) * 2.0f, 0.007f);
                break;
            }
            case Engine::mano: {
                const float r[]  = { 1.0f, 1.52f, 1.98f, 2.44f, 2.90f };
                const float am[] = { 1.0f, 0.50f, 0.33f, 0.20f, 0.12f };
                const float open = 1.0f - std::abs (a - 0.5f) * 2.0f;
                const float slap = std::max (0.0f, a - 0.55f) / 0.45f;
                const float base = 0.035f + d * 0.38f;
                const float tau = a < 0.5f ? base * (0.18f + 0.82f * open) : base * (0.45f + 0.55f * open);
                for (int k = 0; k < 5; ++k)
                    addMode (f0 * r[k], am[k] * (k == 0 ? 1.0f : (0.6f + 0.8f * slap)),
                             tau / (1.0f + (0.7f + 1.2f * (1.0f - open)) * (float) k));
                pitchAmt = 0.12f; pitchTau = 0.012f;
                noiseA.set (sr, NoiseLayer::bandpass, 1200.0f, 0.8f, 1.1f * (1.0f - 0.4f * a), 0.006f);
                noiseB.set (sr, NoiseLayer::bandpass, 3600.0f, 0.9f, slap * 2.6f, 0.018f);
                muteAmt = std::max (0.0f, 0.35f - a) * 2.4f;
                break;
            }
            case Engine::caja: {
                // 0 = cross-stick, 0.5 = normal hit, 1 = rimshot
                const float cross = std::max (0.0f, 0.35f - a) / 0.35f;
                const float rimshot = std::max (0.0f, a - 0.65f) / 0.35f;
                const float r[]  = { 1.0f, 1.62f, 2.15f, 2.61f };
                const float am[] = { 1.0f, 0.55f, 0.30f, 0.20f };
                const float tau = 0.05f + 0.13f * d;
                for (int k = 0; k < 4; ++k)
                    addMode (f0 * r[k], am[k] * (1.0f - 0.85f * cross) * (k == 0 ? 1.0f : 1.0f + 1.2f * rimshot),
                             tau / (1.0f + 0.6f * (float) k));
                pitchAmt = 0.3f + 0.3f * rimshot; pitchTau = 0.01f;
                // the wires
                noiseB.set (sr, NoiseLayer::highpass, 1900.0f, 0.7f, (1.1f + 0.4f * rimshot) * (1.0f - 0.8f * cross), 0.07f + 0.3f * d);
                // stick: woody click for cross-stick, crack for rimshot
                if (cross > 0.0f) noiseA.set (sr, NoiseLayer::bandpass, 1700.0f, 4.0f, 6.0f * cross, 0.025f);
                else              noiseA.set (sr, NoiseLayer::bandpass, 3500.0f, 1.5f, 0.5f + 2.2f * rimshot, 0.005f);
                break;
            }
            case Engine::analogo: {
                const int type = analogType (a);
                const float t = a * 4.0f - (float) type;   // position inside the zone tweaks the circuit
                if (type == 0) {        // TOM
                    addMode (f0, 1.0f, 0.08f + 0.6f * d);
                    addMode (f0 * 1.5f, 0.12f * t, 0.05f + 0.2f * d);
                    pitchAmt = 0.35f + 0.4f * t; pitchTau = 0.06f;
                    noiseA.set (sr, NoiseLayer::bandpass, 800.0f, 0.8f, 0.3f, 0.01f);
                } else if (type == 1) { // SNARE
                    addMode (f0 * 1.64f, 1.0f, 0.05f + 0.15f * d);
                    addMode (f0 * 3.0f, 0.6f, 0.04f + 0.08f * d);
                    pitchAmt = 0.5f; pitchTau = 0.008f;
                    noiseB.set (sr, NoiseLayer::bandpass, 5000.0f + 3000.0f * t, 0.45f, 1.6f, 0.06f + 0.25f * d);
                } else if (type == 2) { // RIM
                    addMode (f0 * 15.5f, 1.0f, 0.012f + 0.02f * d);
                    addMode (f0 * 4.1f, 0.6f, 0.02f + 0.03f * d);
                    noiseA.set (sr, NoiseLayer::bandpass, 2500.0f, 1.0f, 1.6f, 0.003f);
                    drive = 2.0f;
                } else {                // CLAP: four quick bursts, then a tail
                    noiseA.set (sr, NoiseLayer::bandpass, 1000.0f + 500.0f * t, 1.2f, 2.4f, 0.007f, 3, 0.0095f, 0.05f + 0.35f * d);
                }
                break;
            }
            case Engine::madera: {
                // shell (low, round) blended with rim (bright inharmonic block)
                const float r[]  = { 1.0f, 1.47f, 2.09f, 2.56f };
                const float am[] = { 1.0f, 0.40f, 0.22f, 0.12f };
                const float shellTau = 0.04f + 0.25f * d;
                for (int k = 0; k < 4; ++k)
                    addMode (f0 * r[k], am[k] * (1.0f - 0.85f * a), shellTau / (1.0f + 0.8f * (float) k));
                const float br[] = { 1.0f, 2.76f, 5.40f };       // free bar / wood block
                const float ba[] = { 1.0f, 0.40f, 0.15f };
                const float bt[] = { 0.025f + 0.06f * d, 0.010f, 0.005f };
                for (int k = 0; k < 3; ++k) addMode (f0 * 3.2f * br[k], ba[k] * (0.1f + 1.4f * a), bt[k]);
                pitchAmt = 0.15f; pitchTau = 0.01f;
                noiseA.set (sr, NoiseLayer::bandpass, 3000.0f, 0.8f, 0.3f + 0.9f * a, 0.003f);
                // plucked knock (Karplus-Strong) at the fundamental
                startKs (f0, 0.86f + 0.12f * d, 0.55f * (1.0f - a) + 0.2f);
                break;
            }
            case Engine::fm: {
                fmOn = true;
                fmFreq = f0; fmRatio = 1.0f + 2.53f * a;
                fmIndex = 1.0f + 6.0f * a;
                fmEnv = 1.0f; fmMul = std::exp (-1.0f / ((0.08f + 0.6f * d) * sr));
                fmIdxEnv = 1.0f; fmIdxMul = std::exp (-1.0f / ((0.03f + 0.1f * d) * sr));
                fmCar = fmMod = 0.0f;
                pitchAmt = 0.6f; pitchTau = 0.025f;
                noiseA.set (sr, NoiseLayer::bandpass, 2500.0f, 1.0f, 0.3f, 0.003f);
                break;
            }
        }

        // shaping knobs scale each engine's own pitch sweep
        if (pitchAmt < 1.0e-6f && p.pitchEnv > 0.5f) pitchAmt = 0.1f;   // engines without a sweep can still get one
        pitchAmt *= pitchEnvFactor (p.pitchEnv);
        pitchTau *= pitchDecayFactor (p.pitchDecay);
        pitchEnv = 1.0f; pitchMul = std::exp (-1.0f / (pitchTau * sr));
        attackInc = attackSeconds (p.attack) > 0.0f ? 1.0f / (attackSeconds (p.attack) * sr) : 1.0f;
        attackRamp = attackInc >= 1.0f ? 1.0f : 0.0f;
        chokeGain = 1.0f; chokeMul = 1.0f;
        muteLp.setLowpass (sr, 700.0f, 0.7f); muteLp.reset();
        active = true;
    }

    float process() {
        if (! active) return 0.0f;
        const float bend = 1.0f + pitchAmt * pitchEnv;
        pitchEnv *= pitchMul;

        float body = 0.0f;
        for (int k = 0; k < nModes; ++k) {
            phase[k] += kTwoPi * freq[k] * bend / sr;
            if (phase[k] > kTwoPi) phase[k] -= kTwoPi;
            body += std::sin (phase[k]) * amp[k] * env[k];
            env[k] *= mul[k];
        }

        float out = body;
        if (fmOn) {
            fmMod += kTwoPi * fmFreq * fmRatio * bend / sr; if (fmMod > kTwoPi) fmMod -= kTwoPi;
            fmCar += kTwoPi * fmFreq * bend / sr;           if (fmCar > kTwoPi) fmCar -= kTwoPi;
            out += std::sin (fmCar + fmIndex * fmIdxEnv * std::sin (fmMod)) * fmEnv;
            fmEnv *= fmMul; fmIdxEnv *= fmIdxMul;
        }
        if (ksOn) out += processKs();

        const float n = rng.next();
        out += noiseA.process (n);
        out += noiseB.process (rng.next());
        if (muteAmt > 0.0f) out += muteLp.process (body) * muteAmt;

        out = std::tanh (out * drive) * gain * attackRamp * chokeGain;
        if (attackRamp < 1.0f) attackRamp = std::min (1.0f, attackRamp + attackInc);
        chokeGain *= chokeMul;
        if (chokeGain < 1.0e-4f) { active = false; return 0.0f; }

        const bool modesDone = nModes == 0 || env[0] < 1.0e-4f;
        if (modesDone && fmEnv < 1.0e-4f && ksEnergy < 1.0e-4f && noiseA.silent() && noiseB.silent()) active = false;
        return out;
    }

    // Choke group: fast fade (3 ms time constant)
    void choke() { if (active) chokeMul = std::exp (-1.0f / (0.003f * sr)); }

private:
    void addMode (float f, float a, float tau) {
        if (nModes >= kModes) return;
        const int k = nModes++;
        freq[k] = f; amp[k] = f < sr * 0.45f ? a : 0.0f; env[k] = 1.0f; phase[k] = 0.0f;
        mul[k] = std::exp (-1.0f / (std::max (tau, 0.002f) * sr));
    }

    static constexpr int kKsMax = 4096;
    void startKs (float f, float feedback, float level) {
        ksLen = std::clamp ((int) (sr / f), 2, kKsMax - 1);
        ksFb = feedback; ksAmp = level; ksPos = 0; ksLast = 0.0f; ksOn = true; ksEnergy = 1.0f;
        // short, soft burst = woody knock, not a bright string
        float lp = 0.0f; const float c = 1.0f - std::exp (-kTwoPi * 1200.0f / sr);
        for (int i = 0; i < ksLen; ++i) { lp += c * ((i < ksLen / 3 ? rng.next() : 0.0f) - lp); ksBuf[i] = lp * 3.0f; }
    }
    float processKs() {
        const int next = (ksPos + 1) % ksLen;
        const float y = ksBuf[ksPos];
        const float avg = 0.5f * (y + ksBuf[next]);
        ksLast = 0.6f * avg + 0.4f * ksLast;               // extra damping: wood, not steel
        ksBuf[ksPos] = ksLast * ksFb;
        ksPos = next;
        ksEnergy = 0.999f * ksEnergy + 0.001f * std::abs (y) * 50.0f;
        return y * ksAmp;
    }

    bool active = false;
    float sr = 48000.0f, gain = 0.0f, drive = 1.1f;
    float attackRamp = 1.0f, attackInc = 1.0f, chokeGain = 1.0f, chokeMul = 1.0f;
    int nModes = 0;
    float phase[kModes] {}, freq[kModes] {}, amp[kModes] {}, env[kModes] {}, mul[kModes] {};
    float pitchEnv = 0, pitchMul = 0, pitchAmt = 0, pitchTau = 0.02f;
    float muteAmt = 0;
    Biquad muteLp;
    NoiseLayer noiseA, noiseB;
    bool fmOn = false;
    float fmFreq = 0, fmRatio = 1, fmIndex = 0, fmEnv = 0, fmMul = 0, fmIdxEnv = 0, fmIdxMul = 0, fmCar = 0, fmMod = 0;
    bool ksOn = false;
    float ksBuf[kKsMax] {};
    int ksLen = 2, ksPos = 0;
    float ksFb = 0, ksAmp = 0, ksLast = 0, ksEnergy = 0;
    Rng rng;
};

} // namespace quemao
