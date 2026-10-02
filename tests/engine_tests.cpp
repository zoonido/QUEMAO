// QUEMAO engine checks (run on every push before the Mac build).
#include "lane.h"
#include "generator.h"
#include "fx.h"
#include "modseq.h"
#include <cstdio>
#include <vector>
#include <cmath>

using namespace quemao;

static int failures = 0, passes = 0;
static void check (bool ok, const char* what) {
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++passes : ++failures;
}

static std::vector<float> render (const VoiceParams& p, float vel, float seconds, float sr = 48000.0f) {
    DrumVoice v; v.trigger (p, vel, sr);
    std::vector<float> out ((size_t) (seconds * sr));
    for (auto& s : out) s = v.process();
    return out;
}
static float rms (const std::vector<float>& x, size_t a, size_t b) {
    double e = 0; b = std::min (b, x.size());
    for (size_t i = a; i < b; ++i) e += x[i] * x[i];
    return b > a ? (float) std::sqrt (e / (double) (b - a)) : 0.0f;
}
static float peak (const std::vector<float>& x) { float m = 0; for (float s : x) m = std::max (m, std::abs (s)); return m; }
static bool finite (const std::vector<float>& x) { for (float s : x) if (! std::isfinite (s)) return false; return true; }
// pitch estimate from upward zero crossings in a window, after low-passing
static float pitchEstimate (const std::vector<float>& x, size_t a, size_t b, float sr) {
    Biquad lp; lp.setLowpass (sr, 300.0f, 0.7f);
    int crossings = 0; float prev = 0; size_t first = 0, last = 0;
    for (size_t i = a; i < b; ++i) {
        const float y = lp.process (x[i]);
        if (prev <= 0 && y > 0) { if (crossings == 0) first = i; last = i; ++crossings; }
        prev = y;
    }
    return crossings > 1 ? sr * (float) (crossings - 1) / (float) (last - first) : 0.0f;
}
// brightness: energy above ~2 kHz via a crude first difference ratio
static float brightness (const std::vector<float>& x, size_t a, size_t b) {
    double d = 0, e = 0;
    for (size_t i = a + 1; i < b; ++i) { const double df = x[i] - x[i - 1]; d += df * df; e += x[i] * x[i]; }
    return e > 0 ? (float) (d / e) : 0.0f;
}

int main() {
    const float sr = 48000.0f;

    for (int eng = 0; eng < kNumEngines; ++eng)
     for (float artic : { 0.0f, 0.5f, 1.0f }) {
        VoiceParams p; p.engine = (Engine) eng; p.artic = artic;
        auto x = render (p, 1.0f, 3.5f);
        char name[48]; std::snprintf (name, sizeof name, "%s (artic %.1f)", engineName ((Engine) eng), artic);
        char msg[160];
        std::snprintf (msg, sizeof msg, "%s makes sound (peak %.2f)", name, peak (x)); check (peak (x) > 0.2f, msg);
        std::snprintf (msg, sizeof msg, "%s output is finite and below clipping", name); check (finite (x) && peak (x) <= 1.0f, msg);
        std::snprintf (msg, sizeof msg, "%s decays to silence within 3 s", name); check (rms (x, (size_t) (3.0f * sr), x.size()) < 1.0e-3f, msg);
    }

    {   // Tune: +12 semitones doubles the pitch
        VoiceParams p; p.engine = Engine::membrana; p.decay = 0.9f; p.artic = 0.0f;
        p.tune = -12.0f; auto lo = render (p, 1.0f, 0.6f);
        p.tune = 0.0f;   auto hi = render (p, 1.0f, 0.6f);
        const float fl = pitchEstimate (lo, (size_t) (0.2f * sr), (size_t) (0.5f * sr), sr);
        const float fh = pitchEstimate (hi, (size_t) (0.2f * sr), (size_t) (0.5f * sr), sr);
        char msg[160]; std::snprintf (msg, sizeof msg, "MEMBRANA tune: -12 st = %.1f Hz, 0 st = %.1f Hz (ratio %.2f)", fl, fh, fh / fl);
        check (std::abs (fh / fl - 2.0f) < 0.1f && std::abs (fh - 110.0f) < 6.0f, msg);
    }
    {   // Decay knob lengthens the tail
        VoiceParams p; p.engine = Engine::membrana;
        p.decay = 0.1f; auto s = render (p, 1.0f, 1.0f);
        p.decay = 0.9f; auto l = render (p, 1.0f, 1.0f);
        check (rms (l, (size_t) (0.3f * sr), (size_t) (0.5f * sr)) > 5.0f * rms (s, (size_t) (0.3f * sr), (size_t) (0.5f * sr)), "Decay knob lengthens the tail");
    }
    {   // MANO articulation: slap is brighter than open, muted is shorter than open
        VoiceParams p; p.engine = Engine::mano; p.decay = 0.5f;
        p.artic = 0.0f; auto muted = render (p, 1.0f, 0.5f);
        p.artic = 0.5f; auto open  = render (p, 1.0f, 0.5f);
        p.artic = 1.0f; auto slap  = render (p, 1.0f, 0.5f);
        const size_t a = 0, b = (size_t) (0.03f * sr);
        check (brightness (slap, a, b) > 1.5f * brightness (open, a, b), "MANO slap is brighter than open");
        check (rms (open, (size_t) (0.08f * sr), (size_t) (0.2f * sr)) > 2.0f * rms (muted, (size_t) (0.08f * sr), (size_t) (0.2f * sr)), "MANO open rings longer than muted");
    }
    {   // MEMBRANA edge strike is brighter than center
        VoiceParams p; p.engine = Engine::membrana;
        p.artic = 0.0f; auto c = render (p, 1.0f, 0.3f);
        p.artic = 1.0f; auto e = render (p, 1.0f, 0.3f);
        check (brightness (e, 0, (size_t) (0.05f * sr)) > 1.3f * brightness (c, 0, (size_t) (0.05f * sr)), "MEMBRANA edge is brighter than center");
    }
    {   // Velocity scales level
        VoiceParams p; auto loud = render (p, 1.0f, 0.2f); auto soft = render (p, 0.3f, 0.2f);
        check (peak (soft) < 0.6f * peak (loud), "Velocity scales the hit");
    }

    {   // CAJA: rimshot brighter than cross-stick; Decay lengthens the wires
        VoiceParams p; p.engine = Engine::caja;
        p.artic = 0.0f; auto cross = render (p, 1.0f, 0.5f);
        p.artic = 1.0f; auto rim = render (p, 1.0f, 0.5f);
        check (brightness (rim, 0, (size_t) (0.03f * sr)) > 1.3f * brightness (cross, 0, (size_t) (0.03f * sr)), "CAJA rimshot is brighter than cross-stick");
        p.artic = 0.5f; p.decay = 0.1f; auto tight = render (p, 1.0f, 0.6f);
        p.decay = 0.9f; auto loose = render (p, 1.0f, 0.6f);
        check (rms (loose, (size_t) (0.15f * sr), (size_t) (0.3f * sr)) > 3.0f * rms (tight, (size_t) (0.15f * sr), (size_t) (0.3f * sr)), "CAJA Decay lengthens the wires");
    }
    {   // ANALOGO: four circuits; rim is brightest, clap has several bursts, tom is lowest
        VoiceParams p; p.engine = Engine::analogo;
        p.artic = 0.1f; auto tom = render (p, 1.0f, 0.5f);
        p.artic = 0.6f; auto rim = render (p, 1.0f, 0.5f);
        p.artic = 0.9f; auto clap = render (p, 1.0f, 0.5f);
        check (analogType (0.1f) == 0 && analogType (0.3f) == 1 && analogType (0.6f) == 2 && analogType (1.0f) == 3, "ANALOGO Artic selects TOM / SNARE / RIM / CLAP");
        check (brightness (rim, 0, (size_t) (0.02f * sr)) > 4.0f * brightness (tom, 0, (size_t) (0.02f * sr)), "ANALOGO rim is brighter than tom");
        // count envelope peaks in the first 45 ms (1 ms RMS frames)
        int peaks = 0; float prev2 = 0, prev1 = 0;
        for (size_t f = 0; f < 45; ++f) {
            const float e = rms (clap, f * 48, (f + 1) * 48);
            if (prev1 > prev2 && prev1 > e && prev1 > 0.05f) ++peaks;
            prev2 = prev1; prev1 = e;
        }
        char msg[96]; std::snprintf (msg, sizeof msg, "ANALOGO clap has multiple bursts (%d peaks)", peaks);
        check (peaks >= 3, msg);
    }
    {   // MADERA: rim brighter than shell, short and woody
        VoiceParams p; p.engine = Engine::madera;
        p.artic = 0.0f; auto shell = render (p, 1.0f, 0.4f);
        p.artic = 1.0f; auto rimx = render (p, 1.0f, 0.4f);
        check (brightness (rimx, 0, (size_t) (0.03f * sr)) > 1.5f * brightness (shell, 0, (size_t) (0.03f * sr)), "MADERA rim is brighter than shell");
    }
    {   // FM: Metal raises brightness
        VoiceParams p; p.engine = Engine::fm; p.decay = 0.6f;
        p.artic = 0.0f; auto round = render (p, 1.0f, 0.3f);
        p.artic = 1.0f; auto metal = render (p, 1.0f, 0.3f);
        check (brightness (metal, 0, (size_t) (0.1f * sr)) > 2.0f * brightness (round, 0, (size_t) (0.1f * sr)), "FM Metal turns it from round to metallic");
    }
    {   // Tune tracks on every tonal engine: +12 st doubles the fundamental (MANO, FM)
        for (Engine e : { Engine::mano, Engine::fm }) {
            VoiceParams p; p.engine = e; p.decay = 0.9f; p.artic = e == Engine::fm ? 0.0f : 0.5f;
            p.tune = -12.0f; auto lo = render (p, 1.0f, 0.6f);
            p.tune = 0.0f;   auto hi = render (p, 1.0f, 0.6f);
            const float fl = pitchEstimate (lo, (size_t) (0.1f * sr), (size_t) (0.3f * sr), sr);
            const float fh = pitchEstimate (hi, (size_t) (0.1f * sr), (size_t) (0.3f * sr), sr);
            char msg[120]; std::snprintf (msg, sizeof msg, "%s tune: %.1f Hz -> %.1f Hz an octave up", engineName (e), fl, fh);
            check (std::abs (fh / fl - 2.0f) < 0.15f, msg);
        }
    }

    // ---- stage 3: shaping ----
    {   // P.ENV at 0 removes the pitch sweep, at 1 deepens it
        VoiceParams p; p.engine = Engine::membrana; p.decay = 0.9f; p.artic = 0.0f;
        auto early = [&] (float pe) { p.pitchEnv = pe; auto x = render (p, 1.0f, 0.3f); return pitchEstimate (x, 0, (size_t) (0.04f * sr), sr); };
        const float none = early (0.0f), normal = early (0.5f), deep = early (1.0f);
        char msg[140]; std::snprintf (msg, sizeof msg, "P.ENV: early pitch %.0f Hz (off) < %.0f Hz (default) < %.0f Hz (deep)", none, normal, deep);
        check (none < normal && normal < deep && std::abs (none - 110.0f) < 8.0f, msg);
    }
    {   // P.DEC: longer sweep keeps the pitch high for longer
        VoiceParams p; p.engine = Engine::membrana; p.decay = 0.9f; p.artic = 0.0f; p.pitchEnv = 1.0f;
        p.pitchDecay = 0.0f; auto fast = render (p, 1.0f, 0.4f);
        p.pitchDecay = 1.0f; auto slow = render (p, 1.0f, 0.4f);
        const float ff = pitchEstimate (fast, (size_t) (0.06f * sr), (size_t) (0.15f * sr), sr);
        const float fs = pitchEstimate (slow, (size_t) (0.06f * sr), (size_t) (0.15f * sr), sr);
        char msg[120]; std::snprintf (msg, sizeof msg, "P.DEC: slow sweep still at %.0f Hz vs %.0f Hz", fs, ff);
        check (fs > ff * 1.3f, msg);
    }
    {   // ATTACK softens the strike
        VoiceParams p; p.engine = Engine::mano;
        p.attack = 0.0f; auto hard = render (p, 1.0f, 0.1f);
        p.attack = 1.0f; auto soft = render (p, 1.0f, 0.1f);
        check (rms (soft, 0, 96) < 0.3f * rms (hard, 0, 96), "ATTACK softens the first 2 ms");
    }
    {   // Filter: LP closes the highs, HP removes the lows, open is transparent
        VoiceParams p; p.engine = Engine::caja; p.decay = 0.5f;
        auto dry = render (p, 1.0f, 0.3f);
        auto run = [&] (LaneShape sh) { LaneFx fx; fx.prepare (sr); std::vector<float> y (dry.size()); for (size_t i = 0; i < dry.size(); ++i) y[i] = fx.process (dry[i], sh); return y; };
        LaneShape open; auto o = run (open);
        LaneShape lp; lp.cutoff = 0.4f; auto l = run (lp);
        LaneShape hp; hp.highpass = true; hp.cutoff = 0.35f; auto h = run (hp);
        LaneShape hpOpen; hpOpen.highpass = true; auto ho = run (hpOpen);
        const size_t a = 0, b = (size_t) (0.1f * sr);
        check (std::abs (rms (o, a, b) / rms (dry, a, b) - 1.0f) < 0.1f && std::abs (rms (ho, a, b) / rms (dry, a, b) - 1.0f) < 0.1f, "Filter fully open is transparent in LP and HP");
        check (brightness (l, a, b) < 0.3f * brightness (dry, a, b), "LP cutoff darkens the hit");
        check (brightness (h, a, b) > 2.0f * brightness (dry, a, b), "HP cutoff thins out the body");
        check (std::abs (cutoffHz (1.0f, false) - 20000.0f) < 1.0f && std::abs (cutoffHz (1.0f, true) - 20.0f) < 0.1f, "Cutoff knob fully right = open in both modes");
    }
    {   // Tone tilt and drive
        VoiceParams p; p.engine = Engine::membrana; auto dry = render (p, 1.0f, 0.3f);
        auto run = [&] (LaneShape sh) { LaneFx fx; fx.prepare (sr); std::vector<float> y (dry.size()); for (size_t i = 0; i < dry.size(); ++i) y[i] = fx.process (dry[i], sh); return y; };
        LaneShape dark; dark.tone = 0.0f; LaneShape bright; bright.tone = 1.0f; LaneShape hot; hot.drive = 1.0f;
        const size_t a = 0, b = (size_t) (0.05f * sr);
        check (brightness (run (bright), a, b) > 2.0f * brightness (run (dark), a, b), "TONE tilts dark -> bright");
        auto d = run (hot);
        const float crestDry = peak (dry) / rms (dry, 0, dry.size()), crestHot = peak (d) / rms (d, 0, d.size());
        check (crestHot < 0.8f * crestDry && finite (d), "DRIVE saturates (lower crest factor)");
    }
    {   // Pan law
        float l, r; panGains (0.0f, l, r); const bool c = std::abs (l - 1.0f) < 1e-3f && std::abs (r - 1.0f) < 1e-3f;
        panGains (-1.0f, l, r); const bool left = r < 1e-3f;
        panGains (1.0f, l, r);  const bool right = l < 1e-3f;
        check (c && left && right, "PAN: centre unity, hard left / hard right");
    }
    {   // Choke: a hit on one lane silences the other within ~20 ms
        Lane open, closed; open.prepare (sr); closed.prepare (sr);
        LaneSettings s; s.trigger = true; s.voice.engine = Engine::mano; s.voice.decay = 1.0f;
        open.noteOn (1.0f, s);
        for (int i = 0; i < 2400; ++i) open.process (s, 120.0, false, 0.0);
        closed.noteOn (1.0f, s);
        if (closed.consumeFired()) open.chokeAll();
        std::vector<float> tail (2400);
        for (auto& x : tail) x = open.process (s, 120.0, false, 0.0);
        check (rms (tail, 1200, 2400) < 1.0e-3f, "CHOKE: the other lane in the group cuts off");
    }

    // ---- stage 4: generator and feel ----
    {
        bool allValid = true, deterministic = true;
        for (int g = 0; g < kNumGenres; ++g)
            for (int r = 0; r < 3; ++r) {
                GenSettings gs; gs.genre = (Genre) g; gs.role = (Role) r; gs.seed = 7;
                uint8_t a[16], b[16];
                generatePattern (gs, nullptr, a); generatePattern (gs, nullptr, b);
                for (int i = 0; i < 16; ++i) { if (a[i] > 3) allValid = false; if (a[i] != b[i]) deterministic = false; }
            }
        check (allValid, "GENERATE: every genre and role gives a valid pattern");
        check (deterministic, "GENERATE: the same seed gives the same pattern");
    }
    {   // locks are never touched
        GenSettings gs; gs.genre = Genre::tribalTechno; gs.role = Role::hand;
        uint8_t st[16]; bool lk[16] {};
        for (int i = 0; i < 16; ++i) st[i] = 3;
        lk[3] = lk[13] = true;
        bool ok = true;
        for (uint32_t seed = 1; seed < 50; ++seed) { gs.seed = seed; generatePattern (gs, lk, st); ok = ok && st[3] == 3 && st[13] == 3; }
        check (ok, "LOCK: locked steps survive every re-roll");
    }
    {   // density raises the hit count on average
        auto avgHits = [] (float d) {
            int hits = 0;
            for (uint32_t seed = 1; seed <= 200; ++seed) {
                GenSettings gs; gs.genre = Genre::tribalHouse; gs.role = Role::hand; gs.density = d; gs.seed = seed;
                uint8_t st[16]; generatePattern (gs, nullptr, st);
                for (auto x : st) if (x == 1 || x == 3) ++hits;
            }
            return hits / 200.0f;
        };
        const float lo = avgHits (0.1f), hi = avgHits (0.9f);
        char msg[100]; std::snprintf (msg, sizeof msg, "DENSITY: %.1f hits/bar at 10%% -> %.1f at 90%%", lo, hi);
        check (hi > lo * 1.8f, msg);
    }
    {   // genre identity: cumbia llamador on the off-beats, back-beat on 2 and 4 in house
        GenSettings gs; gs.genre = Genre::cumbia; gs.role = Role::back; gs.seed = 3;
        uint8_t st[16]; generatePattern (gs, nullptr, st);
        check (st[2] == 1 && st[6] == 1 && st[10] == 1 && st[14] == 1, "CUMBIA: llamador on every off-beat");
        gs.genre = Genre::tribalHouse; generatePattern (gs, nullptr, st);
        check (st[4] == 1 && st[12] == 1, "TRIBAL HOUSE: clap on 2 and 4");
    }
    {   // euclidean: exactly k hits, evenly spread
        GenSettings gs; gs.genre = Genre::euclidean; gs.role = Role::low; gs.density = 5.0f / 11.0f; gs.variation = 0.0f; gs.ghost = 0.0f;
        uint8_t st[16]; generatePattern (gs, nullptr, st);
        int hits = 0; for (auto x : st) if (x == 1 || x == 3) ++hits;
        check (hits == 6 && st[0] == 1, "EUCLIDEAN: density sets the hit count (6 of 16)");
    }
    {   // manual clears unlocked steps
        GenSettings gs; gs.genre = Genre::manual; uint8_t st[16]; bool lk[16] {}; lk[0] = true;
        for (auto& x : st) x = 1;
        generatePattern (gs, lk, st);
        int left = 0; for (auto x : st) left += x; check (left == 1 && st[0] == 1, "MANUAL: clears the row except locked steps");
    }
    {   // swing delays the off 16ths; prob 0 silences; humanize stays within 12 ms
        auto hitTimes = [&] (LaneSettings s, const char* pat) {
            Lane lane; lane.prepare (sr); lane.setPattern (pat);
            std::vector<int> t; int last = 0;
            lane.noteOn (1.0f, s);
            for (int i = 0; i < (int) (2.0f * sr); ++i) { lane.process (s, 120.0, false, 0.0); const int f = lane.uiFlash.load(); if (f != last) { t.push_back (i); last = f; } }
            return t;
        };
        LaneSettings s; s.swing = 1.0f;
        auto t = hitTimes (s, "xx..............");   // step 1 and step 2 at 120 BPM: a 16th = 6000 samples
        check (t.size() >= 2 && std::abs (t[1] - t[0] - 9000) <= 2, "SWING: full swing pushes the off 16th by half a step");
        LaneSettings p0; p0.prob = 0.0f;
        check (hitTimes (p0, "xxxxxxxxxxxxxxxx").empty(), "PROB: 0% plays nothing");
        LaneSettings h; h.human = 1.0f;
        auto th = hitTimes (h, "x...x...x...x...");
        bool within = th.size() == 4;
        for (size_t k = 0; within && k < th.size(); ++k) { const int ideal = (int) k * 24000; within = th[k] >= ideal && th[k] - ideal <= (int) (0.012f * sr) + 1; }
        check (within, "HUMAN: hits drift late by at most 12 ms");
    }
    {   // locks round trip
        Lane lane; lane.setLocks ("1000000000000001"); char buf[17]; lane.lockString (buf);
        check (std::strcmp (buf, "1000000000000001") == 0, "Locks save and restore");
    }

    // ---- stage 5: sends, returns and output ----
    {
        // an impulse on lane 1 at 120 BPM through the mixer
        auto runMixer = [&] (LaneSends snd, FxSettings fx, int samples, std::vector<float>& L, std::vector<float>& R, float pan = 0.0f) {
            Mixer m; m.prepare (sr);
            LaneSettings ls[4]; ls[0].shape.pan = pan; LaneSends sd[4]; sd[0] = snd;
            L.assign ((size_t) samples, 0.0f); R.assign ((size_t) samples, 0.0f);
            for (int n = 0; n < samples; ++n) {
                float x[4] = { n < 48 ? 0.8f * std::sin ((float) n * 0.5f) : 0.0f, 0, 0, 0 };
                m.process (x, ls, sd, fx, 120.0, L[(size_t) n], R[(size_t) n]);
            }
        };
        std::vector<float> L, R;
        FxSettings fx; LaneSends none;
        runMixer (none, fx, (int) sr, L, R);
        check (rms (L, 2000, L.size()) < 1.0e-6f, "Sends at zero: no echoes, no tail (dry only)");

        LaneSends d; d.delay = 1.0f; fx.delayMix = 1.0f; fx.delayTime = 4;   // 1/8 at 120 BPM = 12000 samples
        runMixer (d, fx, (int) sr, L, R);
        size_t at = 0; float best = 0;
        for (size_t i = 6000; i < 20000; ++i) if (std::abs (L[i]) > best) { best = std::abs (L[i]); at = i; }
        char msg[120]; std::snprintf (msg, sizeof msg, "DELAY: first echo at %zu samples (1/8 at 120 BPM = 12000)", at);
        check (std::abs ((int) at - 12000) < 80 && best > 0.02f, msg);
        size_t atR = 0; best = 0;
        for (size_t i = 18000; i < 30000; ++i) if (std::abs (R[i]) > best) { best = std::abs (R[i]); atR = i; }
        check (std::abs ((int) atR - 24000) < 160, "DELAY: second repeat ping-pongs to the right");

        d.delayTime = 1 + 1;   // lane override: 1/16 = 6000 samples
        runMixer (d, fx, (int) sr, L, R);
        at = 0; best = 0;
        for (size_t i = 3000; i < 9000; ++i) if (std::abs (L[i]) > best) { best = std::abs (L[i]); at = i; }
        check (std::abs ((int) at - 6000) < 80, "DELAY: the lane's own D.TIME overrides the global time");

        LaneSends v; v.reverb = 1.0f; FxSettings fr; fr.reverbMix = 1.0f;
        fr.reverbSize = 0.2f; runMixer (v, fr, (int) (3 * sr), L, R); const float small = rms (L, (size_t) (1.0f * sr), (size_t) (1.5f * sr));
        fr.reverbSize = 0.95f; runMixer (v, fr, (int) (3 * sr), L, R); const float big = rms (L, (size_t) (1.0f * sr), (size_t) (1.5f * sr));
        std::snprintf (msg, sizeof msg, "REVERB: Size lengthens the tail (%.5f -> %.5f)", small, big);
        check (big > 3.0f * small && finite (L) && finite (R), msg);
        bool typesOk = true;
        for (int t = 0; t < 3; ++t) { fr.reverbType = t; runMixer (v, fr, (int) sr, L, R); typesOk = typesOk && finite (L) && rms (L, 4000, 20000) > 1.0e-4f; }
        check (typesOk, "REVERB: ROOM, PLATE and HALL all ring");

        LaneSends c; c.chorus = 1.0f; FxSettings fc; fc.chorusMix = 1.0f;
        runMixer (c, fc, (int) (0.2f * sr), L, R);
        float diff = 0; for (size_t i = 0; i < L.size(); ++i) diff += std::abs (L[i] - R[i]);
        check (diff > 0.5f, "CHORUS: widens a centred hit into stereo");
    }
    {   // output stage: comp lowers the crest factor, crush quantises, gain scales
        std::vector<float> l (24000), r (24000);
        for (size_t i = 0; i < l.size(); ++i) { const float e = (i % 24000) < 300 ? 1.0f : 0.15f; l[i] = r[i] = e * std::sin ((float) i * 0.05f); }
        auto run = [&] (FxSettings fx) { OutputStage o; o.prepare (sr); std::vector<float> y (l.size()); for (size_t i = 0; i < l.size(); ++i) { float a = l[i] * 0.5f, b = r[i] * 0.5f; o.process (a, b, fx); y[i] = a; } return y; };
        FxSettings plain; plain.gain = 1.0f; auto p0 = run (plain);
        FxSettings comp = plain; comp.comp = 1.0f; auto pc = run (comp);
        // loud burst vs quiet body, after the attack has caught it
        const float r0 = rms (p0, 150, 300) / rms (p0, 14000, 23000), r1 = rms (pc, 150, 300) / rms (pc, 14000, 23000);
        char cm[120]; std::snprintf (cm, sizeof cm, "COMP: loud-to-quiet ratio %.1f -> %.1f (punchy, transient through)", r0, r1);
        check (r1 < 0.5f * r0, cm);
        FxSettings cr = plain; cr.crush = 1.0f; auto pq = run (cr);
        int distinct = 0; float lastV = 99.0f; std::vector<float> seen;
        for (float x : pq) { bool found = false; for (float s2 : seen) if (std::abs (s2 - x) < 1e-6f) { found = true; break; } if (! found && seen.size() < 200) seen.push_back (x); }
        distinct = (int) seen.size(); (void) lastV;
        char msg[100]; std::snprintf (msg, sizeof msg, "CRUSH: output reduced to %d levels", distinct);
        check (distinct < 40, msg);
        FxSettings quiet = plain; quiet.gain = 0.0f; auto pz = run (quiet);
        check (peak (pz) < 1e-6f, "GAIN at zero mutes the output");
    }

    // ---- stage 6: mod sequencer ----
    {
        check (modStepAt (0.0, 1) == 0 && modStepAt (0.25, 1) == 1 && modStepAt (3.99, 1) == 15 && modStepAt (4.0, 1) == 0, "MOD: 1/16 rate walks 16 steps per bar");
        check (modStepAt (1.0, 3) == 1 && modStepAt (15.9, 3) == 15 && modStepAt (16.0, 3) == 0, "MOD: 1/4 rate walks 16 beats");

        LaneSettings ls; LaneSends sd; ls.voice.tune = 0.0f;
        applyMod (0, 0.25f, 1.0f, ls, sd);   // +0.25 * half range (48 st) = +6 st
        check (std::abs (ls.voice.tune - 6.0f) < 1e-4f, "MOD: tune moves in whole semitones (+6 st)");
        LaneSettings l2; l2.voice.tune = 0.0f; applyMod (0, 0.11f, 1.0f, l2, sd);
        check (std::abs (l2.voice.tune - std::round (l2.voice.tune)) < 1e-6f, "MOD: tune offsets snap to semitones");
        LaneSettings l3; l3.shape.cutoff = 0.9f; applyMod (10, 1.0f, 1.0f, l3, sd);
        check (std::abs (l3.shape.cutoff - 1.0f) < 1e-6f, "MOD: values clamp to the knob's range");
        LaneSettings l4; l4.voice.artic = 0.5f; applyMod (6, -1.0f, 0.0f, l4, sd);
        check (std::abs (l4.voice.artic - 0.5f) < 1e-6f, "MOD: DEPTH 0 leaves the knob alone");

        // the sequencer moves lane 2's ARTIC per step, nothing else
        ModSequencer m; m.prepare (sr);
        const int t = 1 * kModTargetsPerLane + 6;   // lane 2 ARTIC
        for (int i = 0; i < 16; ++i) m.values[t][i].store (i % 2 ? 1.0f : -1.0f);
        m.scan();
        LaneSettings s4[4]; LaneSends d4[4];
        m.apply (0.0, true, 0.0, 1, 1.0f, s4, d4);
        const float a0 = s4[1].voice.artic;
        LaneSettings s5[4]; m.apply (0.25, true, 0.0, 1, 1.0f, s5, d4);
        check (m.activeCount() == 1 && a0 < 0.01f && s5[1].voice.artic > 0.99f && std::abs (s5[0].voice.artic - 0.5f) < 1e-6f,
               "MOD: one target steps lane 2's ARTIC between muted and slap, other lanes untouched");

        m.values[0][3].store (-0.5f);
        const std::string txt = m.serialise (t);
        ModSequencer m2; m2.deserialise (t, txt);
        bool same = true; for (int i = 0; i < 16; ++i) same = same && std::abs (m2.values[t][i].load() - m.values[t][i].load()) < 1e-3f;
        check (same && m.serialise (5).empty(), "MOD: lanes save and restore (flat lanes save nothing)");

        // stopped transport: free-runs at tempo
        ModSequencer m3; m3.prepare (sr); m3.values[0][0].store (0.1f); m3.scan();
        LaneSettings s6[4]; LaneSends d6[4];
        for (int i = 0; i < 6100; ++i) { for (auto& x : s6) x = LaneSettings(); m3.apply (0.0, false, 120.0 / 60.0 / sr, 1, 1.0f, s6, d6); }
        check (m3.uiStep.load() == 1, "MOD: keeps stepping at host tempo while Ableton is stopped");
    }

    // ---- clock ----
    check (stepOfHalf (0) == 0 && stepOfHalf (1) == 0 && stepOfHalf (2) == 1 && stepOfHalf (31) == 15 && stepOfHalf (32) == 0, "Half-steps map to the 16 steps");
    check (stepOfHalf (-1) == 15 && stepOfHalf (-2) == 15 && stepOfHalf (-3) == 14, "Negative positions wrap correctly");
    check (halfFires (0, stepHit) && ! halfFires (1, stepHit) && halfFires (1, stepRatchet) && ! halfFires (0, stepOff), "Ratchets fire twice, hits once");

    {   // Held launch: a pattern with one hit per beat at 120 BPM fires every 0.5 s, starting instantly
        Lane lane; lane.prepare (sr); lane.setPattern ("x...x...x...x...");
        LaneSettings s;
        std::vector<int> hitSamples; int lastFlash = 0;
        lane.noteOn (1.0f, s);
        for (int i = 0; i < (int) (2.0f * sr); ++i) {
            lane.process (s, 120.0, false, 0.0);
            const int f = lane.uiFlash.load(); if (f != lastFlash) { hitSamples.push_back (i); lastFlash = f; }
        }
        bool ok = hitSamples.size() == 4 && hitSamples[0] == 0;
        for (size_t k = 1; ok && k < hitSamples.size(); ++k) ok = std::abs (hitSamples[k] - hitSamples[k - 1] - 24000) <= 1;
        check (ok, "LAUNCH: note-on starts step 1 instantly, steps follow the tempo");
        lane.noteOff(); lane.process (s, 120.0, false, 0.0);
        check (lane.uiStatus.load() == statusIdle, "LAUNCH: note-off stops the lane");
    }
    {   // Latch follows transport ppq and locks to the grid
        Lane lane; lane.prepare (sr); lane.setPattern ("x...............");
        LaneSettings s; s.latch = true;
        const double bpm = 120.0, ppqPerSample = bpm / 60.0 / sr;
        int hits = 0, lastFlash = 0, firstHit = -1;
        const double start = 3.9;   // mid-bar start: should wait for the bar line at ppq 4
        for (int i = 0; i < (int) (4.0f * sr); ++i) {
            lane.process (s, bpm, true, start + i * ppqPerSample);
            const int f = lane.uiFlash.load(); if (f != lastFlash) { if (firstHit < 0) firstHit = i; ++hits; lastFlash = f; }
        }
        const int expected = (int) std::ceil ((4.0 - start) / ppqPerSample);
        char msg[160]; std::snprintf (msg, sizeof msg, "LATCH: first hit on the bar line (sample %d, expected %d), %d bars -> %d hits", firstHit, expected, 2, hits);
        check (std::abs (firstHit - expected) <= 1 && hits == 2, msg);
    }
    {   // Trigger mode plays immediately and ignores the pattern
        Lane lane; lane.prepare (sr); lane.setPattern ("xxxxxxxxxxxxxxxx");
        LaneSettings s; s.trigger = true;
        lane.noteOn (0.8f, s);
        float e = 0; for (int i = 0; i < 2000; ++i) e = std::max (e, std::abs (lane.process (s, 120.0, true, i * 1.0e-4)));
        const int flashes = lane.uiFlash.load();
        check (e > 0.1f && flashes == 1, "TRIGGER: note plays the drum at once, pattern rests");
    }
    {   // Pattern string round trip
        Lane lane; lane.setPattern (".gx.g.xg.gx.gxrg"); char buf[17]; lane.patternString (buf);
        check (std::strcmp (buf, ".gx.g.xg.gx.gxrg") == 0, "Pattern saves and restores");
    }

    std::printf ("\n%d passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
