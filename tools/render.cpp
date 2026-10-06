// Local-only tool: plays the real plugin (processor, presets, MIDI out) offline and writes a WAV.
#include "plugin_processor.h"

struct FakeHost : juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing);
        return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0; const int block = 256;
    struct Part { int preset; double bpm; int bars; };
    const Part parts[] = { { 2, 100.0, 4 }, { 3, 100.0, 4 }, { 1, 132.0, 4 }, { 8, 118.0, 4 } };

    juce::AudioBuffer<float> all (2, 0);
    std::vector<float> L, R;
    int midiNotes = 0;
    for (auto part : parts)
    {
        QuemaoProcessor proc;
        FakeHost host; host.bpm = part.bpm;
        proc.setPlayHead (&host);
        proc.setPlayConfigDetails (0, 2, sr, block);
        proc.prepareToPlay (sr, block);
        proc.applyFactoryPreset (part.preset);
        const int total = (int) (part.bars * 4 * 60.0 / part.bpm * sr);
        juce::AudioBuffer<float> buf (2, block);
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            for (const auto m : midi) if (m.getMessage().isNoteOn()) ++midiNotes;
            for (int i = 0; i < block; ++i) { L.push_back (buf.getSample (0, i)); R.push_back (buf.getSample (1, i)); }
            host.ppq += block * part.bpm / 60.0 / sr;
        }
        std::printf ("%-16s %5.0f BPM  %d bars\n", quemao::factoryPreset (part.preset).name, part.bpm, part.bars);
    }
    float peak = 0.0f; for (size_t i = 0; i < L.size(); ++i) peak = std::max ({ peak, std::abs (L[i]), std::abs (R[i]) });
    juce::File out (argc > 1 ? argv[1] : "render.wav");
    out.deleteFile();
    juce::FileOutputStream os (out);
    auto w32 = [&] (int v) { os.writeInt (v); };
    auto w16 = [&] (short v) { os.writeShort (v); };
    const int bytes = (int) L.size() * 4;
    os.write ("RIFF", 4); w32 (36 + bytes); os.write ("WAVE", 4);
    os.write ("fmt ", 4); w32 (16); w16 (1); w16 (2); w32 ((int) sr); w32 ((int) sr * 4); w16 (4); w16 (16);
    os.write ("data", 4); w32 (bytes);
    for (size_t i = 0; i < L.size(); ++i) { w16 ((short) std::lrint (L[i] * 32767.0f)); w16 ((short) std::lrint (R[i] * 32767.0f)); }
    std::printf ("peak %.2f\n", peak);
    std::printf ("MIDI out: %d notes\n", midiNotes);
    return 0;
}
