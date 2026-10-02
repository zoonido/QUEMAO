// Renders an audio preview of QUEMAO stage 1 using the same engine and lane code as the plugin.
// Usage: preview out.wav
#include "lane.h"
#include "generator.h"
#include "fx.h"
#include "modseq.h"
#include <cstdio>
#include <vector>
#include <cmath>
#include <fstream>

using namespace quemao;

struct NoteEvent { double beat; int lane; bool on; float vel; };

int main (int argc, char** argv)
{
    // Stage 6 preview: the mod sequencer. Bars 1-2 static, bars 3-4 the conga's ARTIC steps through
    // muted / open / slap, bars 5-6 the tom's TUNE walks a melody, bars 7-8 the rim's CUTOFF and PAN dance.
    const char* path = argc > 1 ? argv[1] : "preview.wav";
    const double sr = 48000.0, bpm = 122.0;
    const int bars = 8;

    Lane lanes[4];
    LaneSettings s[4];
    LaneSends sends[4];
    FxSettings fx;
    Mixer mixer;
    mixer.prepare ((float) sr);
    ModSequencer mod;
    mod.prepare ((float) sr);
    for (auto& l : lanes) l.prepare ((float) sr);
    for (auto& x : s) x.latch = true;

    auto voice = [&] (int i, Engine e, float tune, float decay, float artic, float level, float pan) {
        s[i].voice.engine = e; s[i].voice.tune = tune; s[i].voice.decay = decay; s[i].voice.artic = artic;
        s[i].voice.level = level; s[i].shape.pan = pan;
    };
    auto gen = [&] (int i, Genre g, float dens, uint32_t seed) {
        GenSettings gs; gs.genre = g; gs.role = roleFor (s[i].voice.engine, s[i].voice.artic);
        gs.density = dens; gs.ghost = 0.45f; gs.variation = 0.3f; gs.seed = seed;
        uint8_t st[16]; generatePattern (gs, nullptr, st);
        for (int k = 0; k < 16; ++k) lanes[i].steps[k].store (st[k]);
    };
    voice (0, Engine::membrana, -5.0f, 0.45f, 0.3f, 0.85f, 0.0f);   s[0].voice.pitchEnv = 0.75f;
    voice (1, Engine::mano, 0.0f, 0.5f, 0.55f, 0.6f, -0.35f);
    voice (2, Engine::fm, 5.0f, 0.3f, 0.65f, 0.4f, 0.35f);
    voice (3, Engine::analogo, 0.0f, 0.4f, 0.6f, 0.5f, 0.1f);        // rim
    s[3].shape.res = 0.55f; s[3].shape.cutoff = 0.6f;
    gen (0, Genre::tribalHouse, 0.45f, 21); gen (1, Genre::tribalHouse, 0.55f, 22);
    gen (2, Genre::minimalTechno, 0.5f, 23); gen (3, Genre::tribalTechno, 0.55f, 24);
    voice (0, Engine::membrana, -7.0f, 0.5f, 0.25f, 0.9f, 0.0f);
    lanes[0].setPattern ("x..x..x...x.x..x");
    lanes[1].setPattern ("xgxxxgxxxgxxxgxx");
    for (int i = 0; i < 4; ++i) { sends[i].reverb = 0.2f; sends[i].delay = i == 3 ? 0.3f : 0.1f; }
    for (auto& x : s) x.human = 0.25f;
    fx.reverbType = 1; fx.reverbSize = 0.6f; fx.reverbMix = 0.6f; fx.delayMix = 0.55f; fx.delayFeedback = 0.5f; fx.chorusMix = 0.7f;

    const double samplesPerBeat = sr * 60.0 / bpm;
    const size_t total = (size_t) (bars * 4 * samplesPerBeat + 3.0 * sr);
    std::vector<float> outL (total), outR (total);
    int currentBar = -1;

    for (size_t n = 0; n < total; ++n)
    {
        const double beat = (double) n / samplesPerBeat;
        const int bar = (int) (beat / 4.0);
        if (bar != currentBar && bar < bars)
        {
            currentBar = bar;
            if (bar == 2) { const float art[16] = { -0.9f, 0.4f, 0.0f, -0.9f, 1.0f, -0.9f, 0.3f, 0.9f, -0.9f, 0.4f, 0.0f, -0.9f, 1.0f, 0.5f, -0.9f, 1.0f };
                            for (int i = 0; i < 16; ++i) mod.values[16 + 6][i].store (art[i]); }
            if (bar == 4) { const float tn[16] = { 0, 0, 0.125f, 0, 0, 0.21f, 0, 0, 0.29f, 0, 0.21f, 0, 0.125f, 0, 0, -0.08f };   // 0 +3 +5 +7 st
                            for (int i = 0; i < 16; ++i) mod.values[0][i].store (tn[i]); }
            if (bar == 6) { for (int i = 0; i < 16; ++i) { mod.values[48 + 10][i].store (std::sin (i * 0.785f) * 0.9f);
                                                             mod.values[48 + 12][i].store (i % 2 ? 0.9f : -0.9f); } }
            mod.scan();
        }
        const bool playing = beat < bars * 4.0;
        LaneSettings w[4]; LaneSends ws[4];
        for (int i = 0; i < 4; ++i) { w[i] = s[i]; ws[i] = sends[i]; }
        mod.apply (beat, playing, bpm / 60.0 / sr, 1, 1.0f, w, ws);
        float x[4];
        for (int i = 0; i < 4; ++i) x[i] = lanes[i].process (w[i], bpm, playing, beat);
        mixer.process (x, w, ws, fx, bpm, outL[n], outR[n]);
    }

    float peak = 0; for (size_t n = 0; n < total; ++n) peak = std::max ({ peak, std::abs (outL[n]), std::abs (outR[n]) });
    const float norm = peak > 0 ? 0.89f / peak : 1.0f;
    const size_t fade = (size_t) (1.0 * sr);
    for (size_t n = total - fade; n < total; ++n) { const float g = (float) (total - n) / (float) fade; outL[n] *= g; outR[n] *= g; }

    std::ofstream f (path, std::ios::binary);
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t dataBytes = (uint32_t) (total * 2 * 2);
    f.write ("RIFF", 4); w32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    f.write ("data", 4); w32 (dataBytes);
    for (size_t n = 0; n < total; ++n)
    {
        outL[n] = std::clamp (outL[n] * norm, -1.0f, 1.0f) / norm; outR[n] = std::clamp (outR[n] * norm, -1.0f, 1.0f) / norm;
        const int16_t a = (int16_t) std::lrint (outL[n] * norm * 32767.0f), b = (int16_t) std::lrint (outR[n] * norm * 32767.0f);
        f.write ((const char*) &a, 2); f.write ((const char*) &b, 2);
    }
    std::printf ("wrote %s (%.1f s, peak %.2f before normalising)\n", path, total / sr, peak);
    return 0;
}
