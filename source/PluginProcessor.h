#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <map>

//==============================================================================
struct SceneSnapshot
{
    float stepFaders[8] { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float knobValues[8] { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    int lfoPresets[8] { 0, 0, 0, 0, 0, 0, 0, 0 };
};

struct LfoPresetConfig
{
    juce::String name;
    float rate;  // Speed relative to tempo (e.g. cycles per beat)
    float depth; // Scale factor (0.0 to 1.0)
};

//==============================================================================
class PluginProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    PluginProcessor();
    ~PluginProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Shared State & Target Snapping
    //==============================================================================
    void captureScene (bool targetB);
    void triggerSceneAnchorSwitch();
    
    bool isSceneBActive() const { return isSceneBActiveAnchor.load(); }
    void setSceneBActive (bool shouldBeB) { isSceneBActiveAnchor.store (shouldBeB); }

    SceneSnapshot& getSceneA() { return sceneA; }
    SceneSnapshot& getSceneB() { return sceneB; }

    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Internal DSP States
    SceneSnapshot sceneA;
    SceneSnapshot sceneB;
    SceneSnapshot frozenCache;

    std::atomic<bool> isSceneBActiveAnchor { false };
    std::atomic<bool> isFrozen { false };
    
    // MIDI Sequence Clock & State Tracking
    double currentSampleRate = 44100.0;
    int clockStep = 0;
    double playheadPosition = 0.0;
    
    // Kept note state for sample-accurate note-offs
    std::vector<std::pair<int, int>> activeNotes; // pair of <midiNoteNumber, remainingSamples>

    // Static mapping of curated LFO configs
    std::vector<LfoPresetConfig> lfoPresets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};