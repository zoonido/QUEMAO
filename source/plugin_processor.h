#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "lane.h"
#include "generator.h"
#include "fx.h"
#include "modseq.h"
#include "presets.h"

class QuemaoProcessor : public juce::AudioProcessor
{
public:
    QuemaoProcessor();
    ~QuemaoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "quemao"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::String pid (const char* base, int lane) { return juce::String (base) + juce::String (lane + 1); }

    juce::AudioProcessorValueTreeState apvts;
    quemao::Lane lanes[4];
    std::atomic<uint32_t> seeds[4] { { 1 }, { 2 }, { 3 }, { 4 } };
    quemao::ModSequencer modSeq;
    std::atomic<int> modTarget { 1 * quemao::kModTargetsPerLane + 6 };   // the knob the editor shows (lane 2 ARTIC)

    // stage 7: presets and lane names (message thread)
    void applyFactoryPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    static juce::File userPresetFolder();
    juce::Array<juce::File> userPresets() const;
    juce::String getPresetName() const { const juce::ScopedLock sl (nameLock); return presetName; }
    juce::String getLaneName (int lane) const { const juce::ScopedLock sl (nameLock); return laneNames[lane]; }
    void setLaneName (int lane, const juce::String& n) { const juce::ScopedLock sl (nameLock); laneNames[lane] = n.toUpperCase().substring (0, 14); }
    std::atomic<int> presetVersion { 0 };   // bumps when a preset loads, so the editor refreshes

    // Fill a lane's unlocked steps from its genre. reroll = new random seed (the GENERATE button);
    // otherwise the current seed is kept, so Density / Ghost / Variation reshape the same pattern.
    void generate (int lane, bool reroll);
    std::atomic<double> uiBpm { 120.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    quemao::LaneSettings readLane (int lane) const;
    quemao::Mixer mixer;
    juce::CriticalSection nameLock;
    juce::String presetName { "INIT" };
    juce::String laneNames[4] { "TOM GRAVE", "CONGA", "TOM ALTO", "BONGO" };
    int noteOffIn[4] { -1, -1, -1, -1 };   // MIDI out: samples until each lane's note-off
    float smoothGain = 0.8f;

    double sr = 48000.0;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuemaoProcessor)
};
