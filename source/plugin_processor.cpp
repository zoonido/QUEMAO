#include "plugin_processor.h"
#include "plugin_editor.h"

using namespace quemao;

QuemaoProcessor::QuemaoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "quemao", createLayout())
{
    for (int i = 0; i < 4; ++i)
        lanes[i].setPattern (kLaneDefaults[i].pattern);

    // safe defaults in case a host processes before calling prepareToPlay (some plugin scanners do)
    for (auto& l : lanes) l.prepare (48000.0f);
    mixer.prepare (48000.0f);
    modSeq.prepare (48000.0f);
}

juce::AudioProcessorValueTreeState::ParameterLayout QuemaoProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using namespace juce;

    for (int i = 0; i < 4; ++i)
    {
        const auto& d = kLaneDefaults[i];
        const String n = "Lane " + String (i + 1) + " ";

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid ("eng", i), 1 }, n + "Engine",
                                                            StringArray { "MEMBRANA", "MANO", "CAJA", "ANALOGO", "MADERA", "FM" }, (int) d.engine));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid ("mode", i), 1 }, n + "MIDI Mode",
                                                            StringArray { "LAUNCH", "TRIGGER" }, d.trigger ? 1 : 0));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid ("latch", i), 1 }, n + "Latch", false));

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("tune", i), 1 }, n + "Tune",
                        NormalisableRange<float> (-24.0f, 24.0f, 1.0f), d.tune,
                        AudioParameterFloatAttributes().withLabel ("st")));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("fine", i), 1 }, n + "Fine",
                        NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f,
                        AudioParameterFloatAttributes().withLabel ("ct")));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("decay", i), 1 }, n + "Decay",
                        NormalisableRange<float> (0.0f, 1.0f), d.decay));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("artic", i), 1 }, n + "Artic",
                        NormalisableRange<float> (0.0f, 1.0f), d.artic));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("level", i), 1 }, n + "Level",
                        NormalisableRange<float> (0.0f, 1.0f), 0.8f));

        // stage 3: shaping
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("penv", i), 1 }, n + "Pitch Env",
                        NormalisableRange<float> (0.0f, 1.0f), 0.5f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("pdec", i), 1 }, n + "Pitch Decay",
                        NormalisableRange<float> (0.0f, 1.0f), 0.5f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("attack", i), 1 }, n + "Attack",
                        NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("tone", i), 1 }, n + "Tone",
                        NormalisableRange<float> (0.0f, 1.0f), 0.5f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("drive", i), 1 }, n + "Drive",
                        NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("cutoff", i), 1 }, n + "Cutoff",
                        NormalisableRange<float> (0.0f, 1.0f), 1.0f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("res", i), 1 }, n + "Resonance",
                        NormalisableRange<float> (0.0f, 1.0f), 0.1f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("pan", i), 1 }, n + "Pan",
                        NormalisableRange<float> (-1.0f, 1.0f), kLanePans[i]));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid ("fmode", i), 1 }, n + "Filter Mode",
                                                            StringArray { "LP", "HP" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid ("choke", i), 1 }, n + "Choke Group",
                                                            StringArray { "-", "A", "B" }, 0));

        // stage 4: generator and feel
        StringArray genres;
        for (int g = 0; g < kNumGenres; ++g) genres.add (genreName ((Genre) g));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid ("genre", i), 1 }, n + "Genre", genres, (int) Genre::tribalTechno));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("dens", i), 1 }, n + "Density",
                        NormalisableRange<float> (0.0f, 1.0f), 0.5f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("ghost", i), 1 }, n + "Ghosts",
                        NormalisableRange<float> (0.0f, 1.0f), 0.4f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("var", i), 1 }, n + "Variation",
                        NormalisableRange<float> (0.0f, 1.0f), 0.3f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("prob", i), 1 }, n + "Probability",
                        NormalisableRange<float> (0.0f, 1.0f), 1.0f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("human", i), 1 }, n + "Humanize",
                        NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("swing", i), 1 }, n + "Swing",
                        NormalisableRange<float> (0.0f, 1.0f), 0.0f));

        // stage 5: sends
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("sendd", i), 1 }, n + "Delay Send",
                        NormalisableRange<float> (0.0f, 1.0f), 0.15f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("sendv", i), 1 }, n + "Reverb Send",
                        NormalisableRange<float> (0.0f, 1.0f), 0.2f));
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid ("sendc", i), 1 }, n + "Chorus Send",
                        NormalisableRange<float> (0.0f, 1.0f), 0.0f));
        StringArray dt { "GLOBAL" };
        for (int dv = 0; dv < kNumDivisions; ++dv) dt.add (divisionName (dv));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid ("dtime", i), 1 }, n + "Delay Time", dt, 0));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "master", 1 }, "Master",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));

    // stage 5: send effects and output stage
    auto f01 = [&layout] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), def));
    };
    f01 ("chtone", "Chorus Tone", 0.6f);  f01 ("chrate", "Chorus Rate", 0.3f);  f01 ("chmix", "Chorus Mix", 0.5f);
    {
        juce::StringArray d;
        for (int i = 0; i < kNumDivisions; ++i) d.add (divisionName (i));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "gdtime", 1 }, "Delay Time", d, kDefaultDivision));
    }
    f01 ("dfdbk", "Delay Feedback", 0.45f);  f01 ("dtone", "Delay Tone", 0.6f);  f01 ("dmix", "Delay Mix", 0.5f);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "vtype", 1 }, "Reverb Type",
                                                              juce::StringArray { "ROOM", "PLATE", "HALL" }, 1));
    f01 ("vsize", "Reverb Size", 0.55f);  f01 ("vdamp", "Reverb Damp", 0.4f);  f01 ("vmix", "Reverb Mix", 0.5f);
    f01 ("comp", "Compressor", 0.0f);  f01 ("crush", "Crush", 0.0f);

    // stage 6: mod sequencer
    {
        juce::StringArray r;
        for (int i = 0; i < kNumModRates; ++i) r.add (modRateName (i));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "modrate", 1 }, "Mod Rate", r, 1));
    }
    f01 ("moddepth", "Mod Depth", 0.6f);

    // stage 7
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "midiout", 1 }, "MIDI Out", true));
    return layout;
}

void QuemaoProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& l : lanes) l.prepare ((float) sampleRate);
    mixer.prepare ((float) sampleRate);
    modSeq.prepare ((float) sampleRate);
}

bool QuemaoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

LaneSettings QuemaoProcessor::readLane (int i) const
{
    LaneSettings s;
    auto v = [this, i] (const char* base) { return apvts.getRawParameterValue (pid (base, i))->load(); };
    s.voice.engine = (Engine) juce::jlimit (0, kNumEngines - 1, (int) std::lround (v ("eng")));
    s.trigger = v ("mode") > 0.5f;
    s.latch = v ("latch") > 0.5f;
    s.voice.tune = v ("tune");
    s.voice.fine = v ("fine");
    s.voice.decay = v ("decay");
    s.voice.artic = v ("artic");
    s.voice.level = v ("level");
    s.voice.pitchEnv = v ("penv");
    s.voice.pitchDecay = v ("pdec");
    s.voice.attack = v ("attack");
    s.shape.tone = v ("tone");
    s.shape.drive = v ("drive");
    s.shape.cutoff = v ("cutoff");
    s.shape.res = v ("res");
    s.shape.pan = v ("pan");
    s.shape.highpass = v ("fmode") > 0.5f;
    s.shape.choke = (int) std::lround (v ("choke"));
    s.prob = v ("prob");
    s.human = v ("human");
    s.swing = v ("swing");
    return s;
}

void QuemaoProcessor::generate (int lane, bool reroll)
{
    if (reroll)
        seeds[lane].store ((uint32_t) juce::Random::getSystemRandom().nextInt (0x7fffffff) | 1u);

    auto v = [this, lane] (const char* base) { return apvts.getRawParameterValue (pid (base, lane))->load(); };
    GenSettings g;
    g.genre = (Genre) juce::jlimit (0, kNumGenres - 1, (int) std::lround (v ("genre")));
    g.role = roleFor ((Engine) juce::jlimit (0, kNumEngines - 1, (int) std::lround (v ("eng"))), v ("artic"));
    g.density = v ("dens");
    g.ghost = v ("ghost");
    g.variation = v ("var");
    g.seed = seeds[lane].load();

    bool locked[16];
    uint8_t st[16];
    for (int i = 0; i < 16; ++i) { locked[i] = lanes[lane].locks[i].load(); st[i] = lanes[lane].steps[i].load(); }
    generatePattern (g, locked, st);
    for (int i = 0; i < 16; ++i) lanes[lane].steps[i].store (st[i]);
}

void QuemaoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0) { midi.clear(); return; }
    buffer.clear();

    double bpm = 120.0, ppq0 = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq0 = *p;
            playing = pos->getIsPlaying();
        }
    uiBpm.store (bpm);
    const double ppqPerSample = bpm / 60.0 / sr;

    LaneSettings settings[4];
    LaneSends sends[4];
    for (int i = 0; i < 4; ++i)
    {
        settings[i] = readLane (i);
        auto v = [this, i] (const char* base) { return apvts.getRawParameterValue (pid (base, i))->load(); };
        sends[i].delay = v ("sendd"); sends[i].reverb = v ("sendv"); sends[i].chorus = v ("sendc");
        sends[i].delayTime = (int) std::lround (v ("dtime"));
    }
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    FxSettings fx;
    fx.chorusTone = g ("chtone"); fx.chorusRate = g ("chrate"); fx.chorusMix = g ("chmix");
    fx.delayTime = (int) std::lround (g ("gdtime")); fx.delayFeedback = g ("dfdbk"); fx.delayTone = g ("dtone"); fx.delayMix = g ("dmix");
    fx.reverbType = (int) std::lround (g ("vtype")); fx.reverbSize = g ("vsize"); fx.reverbDamp = g ("vdamp"); fx.reverbMix = g ("vmix");
    fx.comp = g ("comp"); fx.crush = g ("crush");
    const float gainTarget = g ("master");
    const int modRate = (int) std::lround (g ("modrate"));
    const float modDepth = g ("moddepth");
    modSeq.scan();
    LaneSettings work[4];
    LaneSends workSends[4];

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto it = midi.cbegin();
    const auto end = midi.cend();
    juce::MidiBuffer midiOut;
    const bool sendMidi = g ("midiout") > 0.5f;
    const int noteLength = juce::jmax (1, (int) (sr * 60.0 / bpm / 8.0));   // a 1/32 note

    for (int n = 0; n < numSamples; ++n)
    {
        const double ppq = ppq0 + n * ppqPerSample;

        // the mod sequencer moves this sample's knob values (also for TRIGGER notes)
        for (int i = 0; i < 4; ++i) { work[i] = settings[i]; workSends[i] = sends[i]; }
        modSeq.apply (ppq, playing, ppqPerSample, modRate, modDepth, work, workSends);

        while (it != end && (*it).samplePosition <= n)
        {
            const auto msg = (*it).getMessage();
            for (int i = 0; i < 4; ++i)
            {
                if (msg.getNoteNumber() != kLaneNotes[i]) continue;
                if (msg.isNoteOn())       lanes[i].noteOn (msg.getFloatVelocity(), work[i]);
                else if (msg.isNoteOff()) lanes[i].noteOff();
            }
            ++it;
        }


        float x[4];
        for (int i = 0; i < 4; ++i)
            x[i] = lanes[i].process (work[i], bpm, playing, ppq);

        // choke groups: a hit on one lane cuts the other lanes in the same group
        // (a lane that struck on this same sample is left alone, so simultaneous hits both sound)
        // MIDI out: every pattern hit as a note on the lane's own note (C1 D1 E1 F1), channel 1
        for (int i = 0; i < 4; ++i)
        {
            if (noteOffIn[i] >= 0 && --noteOffIn[i] < 0)
                midiOut.addEvent (juce::MidiMessage::noteOff (1, kLaneNotes[i]), n);
            const float v = lanes[i].consumePatternHit();
            if (v > 0.0f && sendMidi)
            {
                if (noteOffIn[i] >= 0) midiOut.addEvent (juce::MidiMessage::noteOff (1, kLaneNotes[i]), n);
                midiOut.addEvent (juce::MidiMessage::noteOn (1, kLaneNotes[i], juce::jlimit (0.05f, 1.0f, v)), n);
                noteOffIn[i] = noteLength;
            }
        }

        bool fired[4];
        for (int i = 0; i < 4; ++i) fired[i] = lanes[i].consumeFired();
        for (int i = 0; i < 4; ++i)
            if (fired[i] && work[i].shape.choke != 0)
                for (int j = 0; j < 4; ++j)
                    if (j != i && ! fired[j] && work[j].shape.choke == work[i].shape.choke)
                        lanes[j].chokeAll();

        smoothGain += 0.001f * (gainTarget - smoothGain);
        fx.gain = smoothGain;
        float outL, outR;
        mixer.process (x, work, workSends, fx, bpm, outL, outR);
        if (right != nullptr) { left[n] = outL; right[n] = outR; }
        else                  left[n] = 0.5f * (outL + outR);
    }
    midi.swapWith (midiOut);
}

void QuemaoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int i = 0; i < 4; ++i)
    {
        char buf[17];
        lanes[i].patternString (buf);
        state.setProperty (juce::Identifier ("steps" + juce::String (i + 1)), juce::String (buf), nullptr);
        lanes[i].lockString (buf);
        state.setProperty (juce::Identifier ("locks" + juce::String (i + 1)), juce::String (buf), nullptr);
        state.setProperty (juce::Identifier ("seed" + juce::String (i + 1)), (int) seeds[i].load(), nullptr);
    }
    for (int t = 0; t < kNumModTargets; ++t)
    {
        const auto key = juce::Identifier ("mod" + juce::String (t));
        const auto txt = modSeq.serialise (t);
        if (txt.empty()) state.removeProperty (key, nullptr);
        else state.setProperty (key, juce::String (txt), nullptr);
    }
    state.setProperty ("modTarget", modTarget.load(), nullptr);
    {
        const juce::ScopedLock sl (nameLock);
        state.setProperty ("presetName", presetName, nullptr);
        for (int i = 0; i < 4; ++i) state.setProperty (juce::Identifier ("name" + juce::String (i + 1)), laneNames[i], nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void QuemaoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid() || ! state.hasType (apvts.state.getType())) return;
        for (int i = 0; i < 4; ++i)
        {
            const auto key = juce::Identifier ("steps" + juce::String (i + 1));
            if (state.hasProperty (key))
                lanes[i].setPattern (state.getProperty (key).toString().toRawUTF8());
            const auto lockKey = juce::Identifier ("locks" + juce::String (i + 1));
            if (state.hasProperty (lockKey))
                lanes[i].setLocks (state.getProperty (lockKey).toString().toRawUTF8());
            const auto seedKey = juce::Identifier ("seed" + juce::String (i + 1));
            if (state.hasProperty (seedKey))
                seeds[i].store ((uint32_t) (int) state.getProperty (seedKey));
        }
        for (int t = 0; t < kNumModTargets; ++t)
        {
            const auto key = juce::Identifier ("mod" + juce::String (t));
            modSeq.deserialise (t, state.hasProperty (key) ? state.getProperty (key).toString().toStdString() : std::string());
        }
        if (state.hasProperty ("modTarget"))
            modTarget.store (juce::jlimit (0, kNumModTargets - 1, (int) state.getProperty ("modTarget")));
        {
            const juce::ScopedLock sl (nameLock);
            if (state.hasProperty ("presetName")) presetName = state.getProperty ("presetName").toString();
            for (int i = 0; i < 4; ++i)
            {
                const auto key = juce::Identifier ("name" + juce::String (i + 1));
                if (state.hasProperty (key)) laneNames[i] = state.getProperty (key).toString();
            }
        }
        presetVersion.fetch_add (1);
        apvts.replaceState (state);
    }
}

// ---------------------------------------------------------------- presets

void QuemaoProcessor::applyFactoryPreset (int index)
{
    const auto& fp = factoryPreset (index);

    // everything back to its default, then the preset's own values
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto token : juce::StringArray::fromTokens (fp.params, " ", ""))
    {
        const auto id = token.upToFirstOccurrenceOf ("=", false, false);
        const auto value = token.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    for (int i = 0; i < 4; ++i)
    {
        lanes[i].setPattern (fp.patterns[i]);
        lanes[i].setLocks ("");
    }

    modSeq.clearAll();
    for (auto lane : juce::StringArray::fromTokens (fp.mods, ";", ""))
    {
        if (! lane.containsChar (':')) continue;
        const int t = lane.upToFirstOccurrenceOf (":", false, false).getIntValue();
        if (t >= 0 && t < kNumModTargets)
            modSeq.deserialise (t, lane.fromFirstOccurrenceOf (":", false, false).toStdString());
    }

    {
        const juce::ScopedLock sl (nameLock);
        presetName = fp.name;
        for (int i = 0; i < 4; ++i) laneNames[i] = fp.laneNames[i];
    }
    presetVersion.fetch_add (1);
}

juce::File QuemaoProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("ZOONIDO").getChildFile ("QUEMAO Presets");
}

juce::Array<juce::File> QuemaoProcessor::userPresets() const
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.quemao");
    files.sort();
    return files;
}

bool QuemaoProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = rawName.trim().toUpperCase();
    if (name.isEmpty()) return false;
    {
        const juce::ScopedLock sl (nameLock);
        presetName = name;
    }
    juce::MemoryBlock data;
    getStateInformation (data);
    auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;
    const auto file = folder.getChildFile (juce::File::createLegalFileName (name) + ".quemao");
    const bool ok = file.replaceWithData (data.getData(), data.getSize());
    presetVersion.fetch_add (1);
    return ok;
}

bool QuemaoProcessor::loadUserPreset (const juce::File& file)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data) || data.getSize() == 0) return false;
    setStateInformation (data.getData(), (int) data.getSize());
    {
        const juce::ScopedLock sl (nameLock);
        presetName = file.getFileNameWithoutExtension().toUpperCase();
    }
    presetVersion.fetch_add (1);
    return true;
}

juce::AudioProcessorEditor* QuemaoProcessor::createEditor() { return new QuemaoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QuemaoProcessor(); }
