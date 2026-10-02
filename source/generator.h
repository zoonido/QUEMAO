#pragma once

// QUEMAO stage 4: the per-lane pattern generator.
// Each genre has a weight table per instrument ROLE. The role comes from the lane's engine:
//   LOW  = MEMBRANA, FM, ANALOGO tom      (toms, tambora)
//   HAND = MANO, MADERA                   (congas, bongos, alegre, cajon)
//   BACK = CAJA, ANALOGO snare/rim/clap   (snare, rim, palmas, llamador)
// GENERATE fills every unlocked step; locked steps are never touched.

#include "engine.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace quemao {

enum class Genre : int {
    minimalTechno = 0, tribalTechno, tribalHouse, organicHouse, progressive, cumbia, bullerengue, euclidean, manual
};
constexpr int kNumGenres = 9;
inline const char* genreName (Genre g) {
    static const char* n[kNumGenres] = { "MINIMAL TECHNO", "TRIBAL TECHNO", "TRIBAL HOUSE", "ORGANIC HOUSE",
                                         "PROGRESSIVE", "CUMBIA", "BULLERENGUE", "EUCLIDEAN", "MANUAL" };
    return n[std::clamp ((int) g, 0, kNumGenres - 1)];
}

enum class Role : int { low = 0, hand = 1, back = 2 };
inline Role roleFor (Engine e, float artic) {
    switch (e) {
        case Engine::membrana: case Engine::fm: return Role::low;
        case Engine::mano: case Engine::madera: return Role::hand;
        case Engine::caja: return Role::back;
        case Engine::analogo: return analogType (artic) == 0 ? Role::low : Role::back;
    }
    return Role::hand;
}

struct GenreTable { float w[3][16]; float ghost; float ratchet; };

inline const GenreTable& genreTable (Genre g) {
    static const GenreTable t[7] = {
        // MINIMAL TECHNO: sparse, off-beat, rim on 2 and 4
        { { { .25f,0,.3f,0, .15f,0,.5f,0, .1f,0,.3f,.2f, .15f,0,.4f,.1f },
            { 0,0,.45f,.1f, 0,.2f,.5f,0, 0,0,.45f,.2f, 0,.3f,.5f,.1f },
            { 0,0,0,0, .9f,0,0,0, 0,0,0,0, .9f,0,0,.15f } }, 0.3f, 0.2f },
        // TRIBAL TECHNO: rolling toms, busy hands
        { { { .9f,0,.4f,.7f, .3f,.2f,.8f,.2f, .5f,.2f,.8f,.3f, .7f,.3f,.6f,.5f },
            { .3f,.5f,.7f,.3f, .5f,.4f,.8f,.5f, .3f,.5f,.7f,.4f, .6f,.5f,.8f,.6f },
            { 0,0,0,0, .95f,0,0,.2f, 0,0,0,0, .95f,0,.3f,.3f } }, 0.8f, 0.5f },
        // TRIBAL HOUSE: congas on the off-beats, clap on 2 and 4
        { { { .6f,0,.3f,0, .2f,0,.6f,.2f, .5f,0,.4f,0, .3f,0,.6f,.3f },
            { .1f,.2f,.8f,.2f, .2f,.5f,.8f,.3f, .1f,.3f,.8f,.2f, .3f,.5f,.8f,.4f },
            { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,.2f } }, 0.6f, 0.3f },
        // ORGANIC HOUSE: laid back, open space
        { { { .5f,0,0,.3f, 0,0,.6f,0, .4f,0,0,.4f, 0,0,.5f,0 },
            { 0,.2f,.6f,0, .1f,.3f,.7f,.2f, 0,.2f,.6f,0, .2f,.3f,.7f,.3f },
            { 0,0,0,0, .9f,0,0,.1f, 0,0,.1f,0, .9f,0,0,.1f } }, 0.5f, 0.15f },
        // PROGRESSIVE: steady off-beat pulse, few surprises
        { { { .3f,0,.5f,0, 0,0,.5f,0, .3f,0,.5f,0, 0,.2f,.5f,0 },
            { 0,0,.7f,0, 0,0,.7f,0, 0,0,.7f,0, 0,.2f,.7f,.2f },
            { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 } }, 0.3f, 0.15f },
        // CUMBIA: tambora on 1 and 3, llamador on the off-beats, alegre fills
        { { { .9f,0,0,.3f, .2f,0,.6f,0, .9f,0,0,.3f, .2f,0,.6f,.2f },
            { .2f,.3f,.5f,.4f, .3f,.4f,.6f,.5f, .2f,.3f,.5f,.4f, .3f,.4f,.7f,.5f },
            { 0,0,.95f,0, 0,0,.95f,0, 0,0,.95f,0, 0,0,.95f,0 } }, 0.7f, 0.1f },
        // BULLERENGUE: 3+3+2 tambora and palmas, the alegre improvising on top
        { { { .9f,0,0,.7f, 0,0,.8f,0, .9f,0,0,.7f, 0,0,.8f,0 },
            { .4f,.5f,.6f,.5f, .4f,.6f,.7f,.5f, .4f,.5f,.6f,.6f, .5f,.6f,.8f,.6f },
            { .9f,0,0,.8f, 0,0,.8f,0, .9f,0,0,.8f, 0,0,.8f,0 } }, 0.8f, 0.3f },
    };
    return t[std::clamp ((int) g, 0, 6)];
}

// Evenly spread k hits over n steps (Bjorklund / Bresenham form), rotated.
inline void euclid (int k, int n, int rotate, bool* out) {
    k = std::clamp (k, 0, n);
    for (int i = 0; i < n; ++i) {
        const int j = ((i + rotate) % n + n) % n;
        out[i] = k > 0 && ((j * k) % n) < k;
    }
}

struct GenSettings {
    Genre genre = Genre::tribalTechno;
    Role role = Role::hand;
    float density = 0.5f;    // 0..1
    float ghost = 0.4f;      // 0..1
    float variation = 0.3f;  // 0..1
    uint32_t seed = 1;
};

// Writes 16 step states (0 off, 1 hit, 2 ghost, 3 ratchet) into steps, skipping locked ones.
inline void generatePattern (const GenSettings& g, const bool* locked, uint8_t* steps) {
    // hash the seed (murmur3 finaliser) so neighbouring seeds give unrelated patterns
    uint32_t h = g.seed * 0x9E3779B9u + (uint32_t) g.genre * 0x85EBCA6Bu + (uint32_t) g.role * 0xC2B2AE35u + 0x27D4EB2Fu;
    h ^= h >> 16; h *= 0x85EBCA6Bu; h ^= h >> 13; h *= 0xC2B2AE35u; h ^= h >> 16;
    Rng rng; rng.s = h != 0 ? h : 1u;
    auto r01 = [&rng] { return (float) ((rng.next() * 0.5f + 0.5f)); };
    for (int i = 0; i < 8; ++i) r01();   // warm up

    uint8_t out[16] {};

    if (g.genre == Genre::manual) {
        // nothing generated: clears the unlocked steps
    } else if (g.genre == Genre::euclidean) {
        bool hits[16];
        const int k = (int) std::lround (1.0f + g.density * 11.0f);
        const int rot = (int) std::lround (g.variation * 15.0f);
        euclid (k, 16, rot, hits);
        for (int i = 0; i < 16; ++i) {
            if (hits[i]) out[i] = (i >= 12 && r01() < 0.25f * g.variation) ? 3 : 1;
            else if (r01() < 0.35f * g.ghost) out[i] = 2;
        }
    } else {
        const auto& t = genreTable (g.genre);
        const float* w = t.w[(int) g.role];
        const float dens = 0.35f + g.density * 1.3f;
        for (int i = 0; i < 16; ++i) {
            const float mixW = w[i] * (1.0f - 0.6f * g.variation) + 0.6f * g.variation * r01() * 0.6f;
            const float p = std::clamp (mixW * dens, 0.0f, 1.0f);
            if (r01() < p) {
                const bool late = i >= 12;
                const float rp = t.ratchet * (0.3f + g.variation) * (late ? 1.0f : 0.25f);
                out[i] = r01() < rp ? 3 : 1;
            } else {
                const float gw = (i % 2 ? 0.6f : 0.3f) * t.ghost * g.ghost * (1.0f - 0.5f * w[i]);
                if (r01() < gw * 1.5f) out[i] = 2;
            }
        }
        // anchors keep the genre recognisable
        if (g.role == Role::low && w[0] >= 0.5f && g.variation < 0.7f) out[0] = 1;
        if (g.role == Role::back) for (int i : { 4, 12 }) if (w[i] >= 0.9f && g.density > 0.15f) out[i] = 1;
        if (g.genre == Genre::cumbia && g.role == Role::back) for (int i : { 2, 6, 10, 14 }) out[i] = 1;
    }

    for (int i = 0; i < 16; ++i)
        if (locked == nullptr || ! locked[i]) steps[i] = out[i];
}

} // namespace quemao
