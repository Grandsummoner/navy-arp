#ifndef NAVY_ARP_EDITOR_H
#define NAVY_ARP_EDITOR_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ==============================================================================
// Active OLED Display with lock-free atomic visual synchronization
// ==============================================================================
class OledDisplay : public juce::Component, public juce::Timer
{
public:
    OledDisplay (PluginProcessor& p) : processor (p) 
    {
        startTimerHz (30);
    }
    
    ~OledDisplay() override { stopTimer(); }

    void timerCallback() override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF000000));
        g.setColour (juce::Colour (0xFF112233));
        g.drawRect (getLocalBounds().toFloat(), 1.5f);

        g.setColour (juce::Colour (0xFF00D2FF));
        g.setFont (juce::FontOptions ("Consolas", 14.0f, juce::Font::bold));
        
        juce::String headerText = "--- NAVY-ARP OLED ACTIVE ---";
        g.drawFittedText (headerText, getLocalBounds().removeFromTop (30), juce::Justification::centred, 1);

        // Real-time OLED Context Information (Scale name, Key name, Chord type) [CRITICAL FIX]
        juce::String scaleName = processor.apvts.getParameter(IDs::scaleType.getParamID())->getCurrentValueAsText();
        juce::String keyName = processor.apvts.getParameter(IDs::rootKey.getParamID())->getCurrentValueAsText();
        int extType = processor.activeChordExtensionType.load();
        juce::String extText = (extType == 0) ? "TRIAD" : (extType == 1) ? "SUS" : "7th/9th";

        g.setFont (juce::FontOptions ("Consolas", 11.0f, juce::Font::plain));
        g.setColour (juce::Colour (0xFF888888));
        g.drawText (keyName + " " + scaleName + " (" + extText + ")", 15, 25, getWidth() - 30, 15, juce::Justification::centred);

        // Real-time Step VU-meter pulse lines
        auto area = getLocalBounds().reduced (15);
        area.removeFromTop (30);
        int barWidth = area.getWidth() / 8;
        int spacing = 4;

        for (int i = 0; i < 8; ++i)
        {
            // Lock-free safe load from public APVTS [CRITICAL FIX]
            float faderProb = processor.apvts.getRawParameterValue ("fader" + juce::String (i + 1))->load();
            int barHeight = static_cast<int>(area.getHeight() * faderProb * 0.7f);
            
            juce::Rectangle<int> bar (area.getX() + (i * barWidth) + spacing, 
                                      area.getBottom() - barHeight - 15, 
                                      barWidth - (spacing * 2), 
                                      barHeight);

            bool isPlaying = processor.isCurrentlyPlayingUI.load();

            if (i == processor.currentStep && isPlaying)
            {
                if (i == 0)      g.setColour (juce::Colour (0xFF33FF33)); 
                else if (i == 4) g.setColour (juce::Colour (0xFFFF3333)); 
                else             g.setColour (juce::Colour (0xFF00FFFF)); 
                g.fillRect (bar.expanded(2, 2));
            }
            else
            {
                if (i % 3 == 0)      g.setColour (juce::Colour (0xFF00D2FF)); 
                else if (i % 4 == 0) g.setColour (juce::Colour (0xFFB080FF)); 
                else                 g.setColour (juce::Colour (0xFFFFB300)); 
                g.fillRect (bar);
            }
        }
    }

private:
    PluginProcessor& processor;
};

// ==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown (const juce::MouseEvent& event) override
    {
        for (int i = 0; i < 8; ++i)
        {
            if (event.eventComponent == &presetButtons[i])
            {
                if (event.mods.isRightButtonDown())
                {
                    processor.savePreset(i);
                    presetButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF003344));
                }
            }
        }
    }

private:
    PluginProcessor& processor;
    OledDisplay oledDisplay;

    juce::Slider fader1, fader2, fader3, fader4, fader5, fader6, fader7, fader8;
    juce::Label faderLabel1, faderLabel2, faderLabel3, faderLabel4, faderLabel5, faderLabel6, faderLabel7, faderLabel8;

    juce::Slider rhythmMorphKnob, restKnob, legatoKnob;
    juce::Label rhythmMorphTitle, restTitle, legatoTitle;

    juce::Slider entropyKnob, harmonyKnob, chaosKnob;
    juce::Label entropyTitle, harmonyTitle, chaosTitle;

    juce::Slider morphCrossfader;

    juce::TextButton latchButton;
    juce::TextButton chordModeButton;
    juce::TextButton diceMelodyButton;
    juce::TextButton diceRhythmButton;
    juce::TextButton sceneAButton;
    juce::TextButton sceneBButton;
    juce::TextButton presetButtons[8];

    juce::ComboBox rootKeyBox;
    juce::ComboBox scaleTypeBox;
    juce::ComboBox cycleLengthBox;

    int presetPressStartTime[8] = { 0 };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fader1Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fader2Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fader3Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fader4Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fader5Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fader6Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fader7Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fader8Attachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rhythmMorphAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> restAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> legatoAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> entropyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chaosAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> latchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chordModeAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootKeyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cycleLengthAttachment;

    JURE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

#endif // NAVY_ARP_EDITOR_H