#pragma once

// QUEMAO stage 7: factory presets.
// params: "id=value" pairs in real units (choices as their index). Anything not listed goes back to its default.
//   per lane (suffix 1-4): eng mode latch tune fine decay artic level penv pdec attack tone drive cutoff res pan
//                          fmode choke genre dens ghost var prob human swing sendd sendv sendc dtime
//   global: master chtone chrate chmix gdtime dfdbk dtone dmix vtype vsize vdamp vmix comp crush modrate moddepth midiout
//   engines: 0 MEMBRANA 1 MANO 2 CAJA 3 ANALOGO 4 MADERA 5 FM
//   genres:  0 MINIMAL 1 TRIBAL TECHNO 2 TRIBAL HOUSE 3 ORGANIC 4 PROGRESSIVE 5 CUMBIA 6 BULLERENGUE 7 EUCLIDEAN 8 MANUAL
//   lane dtime: 0 GLOBAL, else division+1 (1/32 1/16 1/16D 1/8T 1/8 3/16 1/4T 1/4 3/8 1/2)
// mods: "target:v0,v1,...,v15;target:..." with target = lane*16 + knob
//   knob: 0 TUNE 1 FINE 2 P.ENV 3 P.DEC 4 ATTACK 5 DECAY 6 ARTIC 7 TONE 8 DRIVE 9 LEVEL 10 CUTOFF 11 RES 12 PAN 13 DLY 14 VERB 15 CHOR

namespace quemao {

struct FactoryPreset {
    const char* name;
    const char* laneNames[4];
    const char* params;
    const char* patterns[4];
    const char* mods;
};

constexpr int kNumFactoryPresets = 9;

inline const FactoryPreset& factoryPreset (int i) {
    static const FactoryPreset p[kNumFactoryPresets] = {
        { "INIT",
          { "TOM GRAVE", "CONGA", "TOM ALTO", "BONGO" },
          "",
          { "x..x..x...x.x..g", ".gx.g.xg.gx.gxrg", "......x.....x.r.", "..g.x..g..g.x..." },
          "" },

        { "RITUAL 132",
          { "TOM GRAVE", "CONGA", "TOM FM", "PALMAS" },
          "eng1=0 tune1=-7 decay1=0.55 artic1=0.3 penv1=0.7 latch1=1 genre1=1 sendd1=0.15 sendv1=0.2 "
          "eng2=1 tune2=0 artic2=0.5 latch2=1 genre2=1 pan2=-0.4 sendd2=0.3 dtime2=6 level2=0.7 "
          "eng3=5 tune3=3 decay3=0.35 artic3=0.7 latch3=1 genre3=1 pan3=0.35 sendd3=0.45 level3=0.55 "
          "eng4=3 mode4=0 artic4=0.9 latch4=1 genre4=1 fmode4=1 cutoff4=0.85 sendv4=0.45 pan4=0.15 level4=0.6 "
          "vtype=1 vsize=0.5 comp=0.3 moddepth=1",
          { "x..x..x...x.x..g", ".gx.g.xg.gx.gxrg", "......x.....x.r.", "..g.x..g..g.x..." },
          "0:0,0,0.125,0,0,0.21,0,0,0.29,0,0.21,0,0.125,0,0,-0.08;"
          "22:0,0.6,0,-0.8,0.9,0,-0.8,0.5,0,0.6,0,-0.8,0.9,0.4,-0.8,0.8" },

        { "CUMBIA ROOTS",
          { "TAMBORA", "ALEGRE", "LLAMADOR", "PALMAS" },
          "eng1=4 tune1=-9 decay1=0.5 artic1=0.25 latch1=1 genre1=5 level1=0.9 "
          "eng2=1 tune2=2 artic2=0.55 latch2=1 genre2=5 pan2=-0.3 level2=0.7 "
          "eng3=1 tune3=7 decay3=0.3 artic3=0.35 latch3=1 genre3=5 pan3=0.3 level3=0.55 "
          "eng4=3 mode4=0 artic4=0.9 latch4=1 genre4=5 pan4=-0.15 level4=0.5 sendv4=0.3 "
          "swing1=0.15 swing2=0.15 swing3=0.15 swing4=0.15 human1=0.3 human2=0.35 human3=0.3 human4=0.3 "
          "vtype=0 vsize=0.4 moddepth=0.8",
          { "x...x.x.x...x.x.", ".gxg.gx..gxg.gxr", "..x...x...x...x.", "....x.......x..." },
          "22:0,0.5,-0.6,0,0.9,-0.6,0,0.5,0,0.5,-0.6,0,0.9,0.3,-0.6,1" },

        { "BULLERENGUE",
          { "TAMBORA", "ALEGRE", "PALMAS", "LLAMADOR" },
          "eng1=0 tune1=-5 decay1=0.45 artic1=0.2 latch1=1 genre1=6 level1=0.9 "
          "eng2=1 tune2=2 artic2=0.6 latch2=1 genre2=6 pan2=-0.3 level2=0.7 "
          "eng3=3 mode3=0 artic3=0.9 latch3=1 genre3=6 level3=0.5 pan3=0.1 sendv3=0.3 "
          "eng4=1 mode4=0 tune4=7 decay4=0.3 artic4=0.3 latch4=1 genre4=6 pan4=0.35 level4=0.5 "
          "human1=0.45 human2=0.5 human3=0.4 human4=0.45 swing1=0.2 swing2=0.2 swing3=0.2 swing4=0.2 "
          "vtype=0 vsize=0.45 moddepth=0.9",
          { "x..x..x.x..x..x.", "x.xgxx.xxg.xx.xr", "x..x..x.x..x..x.", "..x...x...x...x." },
          "22:0.4,0,0.8,-0.6,0.3,0.9,0,-0.6,0.4,0,0.8,-0.6,0.3,0.9,-0.6,1" },

        { "MINIMAL RIM",
          { "RIM", "FM TOM", "CROSS-STICK", "MUTED CONGA" },
          "eng1=3 artic1=0.6 latch1=1 genre1=0 res1=0.45 cutoff1=0.7 sendd1=0.5 dtime1=6 level1=0.6 "
          "eng2=5 tune2=-2 artic2=0.8 decay2=0.4 latch2=1 genre2=0 sendd2=0.5 pan2=0.3 level2=0.55 "
          "eng3=2 mode3=0 artic3=0.05 latch3=1 genre3=0 sendv3=0.25 pan3=-0.2 level3=0.65 "
          "eng4=1 mode4=0 artic4=0.1 latch4=1 genre4=0 pan4=-0.35 level4=0.6 "
          "dfdbk=0.55 dmix=0.6 vtype=1 vsize=0.45 comp=0.2 moddepth=1",
          { "..x...x..x....x.", "......x.......x.", "....x.......x...", ".g.g..x..g.g..x." },
          "10:0,0.6,0.9,0.6,0,-0.6,-0.9,-0.6,0,0.6,0.9,0.6,0,-0.6,-0.9,-0.6;"
          "12:-0.8,0.8,-0.8,0.8,-0.8,0.8,-0.8,0.8,-0.8,0.8,-0.8,0.8,-0.8,0.8,-0.8,0.8" },

        { "ORGANIC HOUSE",
          { "CAJON", "BONGO", "CONGA", "CAJA" },
          "eng1=4 tune1=-12 artic1=0.2 decay1=0.45 latch1=1 genre1=3 level1=0.9 "
          "eng2=1 tune2=9 artic2=0.6 decay2=0.35 latch2=1 genre2=3 pan2=0.35 level2=0.55 "
          "eng3=1 tune3=0 artic3=0.5 latch3=1 genre3=3 pan3=-0.3 level3=0.65 "
          "eng4=2 mode4=0 artic4=0.45 decay4=0.3 tone4=0.35 latch4=1 genre4=3 sendv4=0.4 level4=0.55 "
          "swing1=0.2 swing2=0.2 swing3=0.2 swing4=0.2 human1=0.35 human2=0.35 human3=0.35 human4=0.3 "
          "vtype=1 vsize=0.6 moddepth=0.7",
          { "x.....x...x.....", "..x.x.xg..x.x.gx", ".g..x..g.g..x..g", "....x.......x..g" },
          "38:0,-0.5,0.4,0,0,-0.5,0.6,0,0,-0.5,0.4,0,0,-0.5,0.8,0" },

        { "TRIBAL HOUSE",
          { "CONGA OPEN", "CONGA MUTE", "TOM", "CLAP" },
          "eng1=1 artic1=0.55 choke1=1 latch1=1 genre1=2 pan1=-0.35 level1=0.7 "
          "eng2=1 artic2=0.05 choke2=1 latch2=1 genre2=2 pan2=-0.35 level2=0.65 "
          "eng3=0 tune3=-4 decay3=0.5 penv3=0.65 latch3=1 genre3=2 sendd3=0.25 level3=0.85 "
          "eng4=3 mode4=0 artic4=0.9 latch4=1 genre4=2 sendv4=0.4 pan4=0.15 level4=0.6 "
          "human1=0.2 human2=0.2 vtype=1 vsize=0.55 comp=0.25",
          { "..x...x...x.x...", "x.g.g...g.g...g.", "x.....x.x.....x.", "....x.......x..." },
          "" },

        { "PROGRESSIVE DUB",
          { "TOM", "FM BELL", "RIM", "CLAP" },
          "eng1=0 tune1=-5 penv1=0.7 drive1=0.3 res1=0.45 cutoff1=0.55 latch1=1 genre1=4 sendd1=0.4 dtime1=6 level1=0.85 "
          "eng2=5 tune2=5 artic2=0.65 decay2=0.35 latch2=1 genre2=4 sendc2=0.7 sendd2=0.3 pan2=0.35 level2=0.45 "
          "eng3=3 mode3=0 artic3=0.6 latch3=1 genre3=4 fmode3=1 cutoff3=0.8 pan3=-0.3 sendd3=0.35 level3=0.5 "
          "eng4=3 mode4=0 artic4=0.9 latch4=1 genre4=4 sendv4=0.6 level4=0.55 "
          "gdtime=5 dfdbk=0.6 dmix=0.6 vtype=2 vsize=0.75 vmix=0.55 moddepth=0.8",
          { "..x...x...x...x.", "......x.....x.r.", "...x..x.....x...", "....x.......x..." },
          "10:-0.6,-0.5,-0.4,-0.3,-0.2,-0.1,0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9" },

        { "FUEGO LENTO",
          { "CONGA", "TOM BAJO", "CAJA", "TAMBORA" },
          "eng1=1 artic1=0.5 latch1=1 genre1=2 level1=0.75 sendd1=0.25 "
          "eng2=0 tune2=-7 decay2=0.55 latch2=1 genre2=0 level2=0.85 sendd2=0.4 "
          "eng3=2 mode3=0 artic3=0.5 latch3=1 genre3=4 fmode3=1 cutoff3=0.8 tone3=0.7 sendv3=0.5 pan3=0.2 level3=0.6 "
          "eng4=4 mode4=0 tune4=-9 artic4=0.6 latch4=1 genre4=5 choke4=0 pan4=0.3 level4=0.7 "
          "vtype=1 moddepth=0.8",
          { "x.g.x.gxx.g.r.gx", "..x...x...x.r...", "....x..g....x.gr", "x..gx.x.x..gx.xg" },
          "6:0,0.6,-0.4,0.3,0.9,-0.2,-0.6,0.4,0,0.7,-0.5,0.2,1,-0.3,-0.7,0.5" },
    };
    return p[std::max (0, std::min (i, kNumFactoryPresets - 1))];
}

} // namespace quemao
