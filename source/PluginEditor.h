#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ChromaCapsLookAndFeel.h"
#include "OledDisplay.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::Slider::Listener,
                     private juce::Button::Listener
{
public:
    PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Timer callback to handle long presses and parameter overlay timeout
    void timerCallback() override;

    // Control listeners to trigger OLED overlay display updates
    void sliderValueChanged (juce::Slider* slider) override;
    void buttonClicked (juce::Button* button) override;

    // UI Helper Layout Routines
    void updateSliderTextBoxThemeColors();
    void paint3DPanelPartitioning (juce::Graphics& g);

    //==============================================================================
    PluginProcessor& audioProcessor;

    // LookAndFeel Instance
    ChromaCapsLookAndFeel customLookAndFeel;

    // Displays
    OledDisplay oledDisplay;

    // Morph Row Controls
    juce::TextButton sceneAButton { "A" };
    juce::TextButton sceneBButton { "B" };
    juce::Slider morphSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;

    // Top Center Bar Dropdowns
    juce::ComboBox midiChannelBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiChannelAttachment;
    
    juce::ComboBox rootKeyBox; // Layout alignment dropdown (C, C#, D...)

    // Sequential Modifiers / Latching Utility Buttons
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton recallButton { "RECALL" };
    juce::TextButton copyButton { "COPY" };
    juce::TextButton initButton { "INIT" };

    // Vector Dice / Matrix Randomizers
    juce::TextButton diceMeloButton { "MELO" };
    juce::TextButton diceArtiButton { "ARTI" };
    juce::TextButton diceTimeButton { "TIME" };
    juce::TextButton diceNavyButton { "NAVY" };

    // Freeze Trigger Button
    juce::ToggleButton freezeButton { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    // Step Sequencer Parameter Knobs & Faders (8 slots)
    juce::Slider stepFaders[8];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepFaderAttachments[8];

    juce::Slider knobSliders[8];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachments[8];

    juce::ComboBox lfoPresetBoxes[8];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoAttachments[8];

    // Press Timing States
    std::uint32_t sceneAPressTime = 0;
    std::uint32_t sceneBPressTime = 0;
    bool isAPressed = false;
    bool isBPressed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};