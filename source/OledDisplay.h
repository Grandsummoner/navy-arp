#pragma once

#include <JuceHeader.h>

class OledDisplay : public juce::Component,
                    private juce::Timer
{
public:
    OledDisplay();
    ~OledDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Triggered when any parameter is adjusted in the editor
    void showParameterOverlay (const juce::String& paramName, float baseValue, const juce::String& lfoVibeText);

    // Track active freeze visual state
    void setFreezeActive (bool shouldBeActive);

private:
    void timerCallback() override;

    // Active screen visual state indicators
    bool isOverlayActive = false;
    bool isFreezeActive = false;

    juce::String activeParamName;
    float activeParamValue = 0.0f;
    juce::String activeLfoVibe;

    // VU Levels (used when overlay is inactive)
    float leftVuLevel = 0.0f;
    float rightVuLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OledDisplay)
};