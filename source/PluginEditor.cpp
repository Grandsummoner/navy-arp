#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), oledDisplay()
{
    // Apply modern LookAndFeel overrides cleanly
    setLookAndFeel (&customLookAndFeel);

    // 1. Setup OLED Monitor Display
    addAndMakeVisible (oledDisplay);

    // 2. Setup Shortened Centered Crossfader (200px width track centered horizontally)
    addAndMakeVisible (morphSlider);
    morphSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    morphSlider.addListener (this);
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.apvts, "morph", morphSlider);

    // Flank Buttons A & B flanking the shortened track
    addAndMakeVisible (sceneAButton);
    addAndMakeVisible (sceneBButton);
    sceneAButton.setClickingTogglesState (true);
    sceneBButton.setClickingTogglesState (true);
    sceneAButton.addListener (this);
    sceneBButton.addListener (this);

    // 3. Setup Dropdowns (Top Center Row layout)
    addAndMakeVisible (midiChannelBox);
    midiChannelBox.addItemList ({ "Ch 1", "Ch 2", "Ch 3", "Ch 4", "Ch 5", "Ch 6", "Ch 7", "Ch 8", 
                                  "Ch 9", "Ch 10", "Ch 11", "Ch 12", "Ch 13", "Ch 14", "Ch 15", "Ch 16" }, 1);
    midiChannelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.apvts, "midiChannel", midiChannelBox);

    addAndMakeVisible (rootKeyBox);
    rootKeyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    rootKeyBox.setSelectedId (1);

    // 4. Setup Sequential Modifiers (Utility panel controls)
    addAndMakeVisible (saveButton);
    addAndMakeVisible (recallButton);
    addAndMakeVisible (copyButton);
    addAndMakeVisible (initButton);
    saveButton.setClickingTogglesState (true);
    recallButton.setClickingTogglesState (true);
    copyButton.setClickingTogglesState (true);
    initButton.setClickingTogglesState (true);

    // 5. Setup Vector Square Randomizer Dice buttons
    addAndMakeVisible (diceMeloButton);
    addAndMakeVisible (diceArtiButton);
    addAndMakeVisible (diceTimeButton);
    addAndMakeVisible (diceNavyButton);
    diceMeloButton.addListener (this);
    diceArtiButton.addListener (this);
    diceTimeButton.addListener (this);
    diceNavyButton.addListener (this);

    // 6. Setup Freeze Toggle
    addAndMakeVisible (freezeButton);
    freezeButton.addListener (this);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.apvts, "freeze", freezeButton);

    // 7. Setup Step Sliders, Knobs & LFO Config Boxes
    for (int i = 0; i < 8; ++i)
    {
        juce::String idStr = juce::String (i);

        addAndMakeVisible (stepFaders[i]);
        stepFaders[i].setSliderStyle (juce::Slider::LinearVertical);
        stepFaders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 16);
        stepFaders[i].addListener (this);
        stepFaderAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, "step" + idStr, stepFaders[i]);

        addAndMakeVisible (knobSliders[i]);
        knobSliders[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knobSliders[i].setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 16);
        knobSliders[i].addListener (this);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, "knob" + idStr, knobSliders[i]);

        addAndMakeVisible (lfoPresetBoxes[i]);
        lfoPresetBoxes[i].addItemList ({ "Off", "Drift", "Bounce", "Vibe", "Tremor" }, 1);
        lfoAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            audioProcessor.apvts, "lfoPreset" + idStr, lfoPresetBoxes[i]);
    }

    // Set consistent dark text colors dynamically inside boxes
    updateSliderTextBoxThemeColors();

    // Size of the standard window
    setSize (850, 480);

    // Start UI timer for handling multi-second momentary actions and long presses (50ms interval)
    startTimer (50);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    // Draw Dark Obsidian Dashboard Frame background
    g.fillAll (juce::Colour::fromString ("#FF1E2127"));

    paint3DPanelPartitioning (g);
}

void PluginEditor::paint3DPanelPartitioning (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Horizontal Section Divider Lines
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawHorizontalLine (50, 0.0f, bounds.getWidth());
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (51, 0.0f, bounds.getWidth());

    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawHorizontalLine (150, 0.0f, bounds.getWidth());
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine (151, 0.0f, bounds.getWidth());

    // Sidebar partition boundary line (grouping global modifiers left)
    float sidebarLineX = 180.0f;
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawVerticalLine (static_cast<int> (sidebarLineX), 0.0f, bounds.getHeight());
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawVerticalLine (static_cast<int> (sidebarLineX + 1.0f), 0.0f, bounds.getHeight());

    // Draw Section Header Typography Labels
    g.setColour (juce::Colours::lightgrey);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("MODIFIERS", 15, 12, 120, 16, juce::Justification::left);
    g.drawText ("MATRIX CONTROLS", 195, 12, 150, 16, juce::Justification::left);

    // Draw fader key numbers header titles inside vertical tracks
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (juce::Colours::grey);
    float stepAreaX = 195.0f;
    float stepSlotWidth = (bounds.getWidth() - stepAreaX - 15.0f) / 8.0f;
    for (int i = 0; i < 8; ++i)
    {
        g.drawText ("S-" + juce::String (i + 1), 
                    static_cast<int> (stepAreaX + i * stepSlotWidth), 160, 
                    static_cast<int> (stepSlotWidth), 16, 
                    juce::Justification::centred);
    }
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();

    // 1. Shortened Centered Crossfader layout (200px width track aligned horizontally)
    const int crossfaderWidth = 200;
    const int flankButtonWidth = 40;
    const int totalRowWidth = crossfaderWidth + (flankButtonWidth * 2) + 20; // 300px width
    const int startX = (getWidth() - totalRowWidth) / 2; // Centers horizontal group completely
    const int rowY = 70;

    sceneAButton.setBounds (startX, rowY, flankButtonWidth, 24);
    morphSlider.setBounds (startX + flankButtonWidth + 10, rowY, crossfaderWidth, 24);
    sceneBButton.setBounds (startX + flankButtonWidth + 10 + crossfaderWidth + 10, rowY, flankButtonWidth, 24);

    // 2. Align Left Top Bar Dropdowns
    midiChannelBox.setBounds (15, 70, 45, 24);
    rootKeyBox.setBounds (65, 70, 45, 24);

    // 3. Align Sidebar modifiers on the left column (X spacing < 180px)
    saveButton.setBounds (15, 160, 65, 24);
    recallButton.setBounds (95, 160, 65, 24);
    copyButton.setBounds (15, 195, 65, 24);
    initButton.setBounds (95, 195, 65, 24);

    // Vector Dice controls stacked vertical
    diceMeloButton.setBounds (15, 245, 145, 24);
    diceArtiButton.setBounds (15, 280, 145, 24);
    diceTimeButton.setBounds (15, 315, 145, 24);
    diceNavyButton.setBounds (15, 350, 145, 24);

    freezeButton.setBounds (15, 410, 145, 24);

    // 4. Align OLED Display inside Top Center Right Area
    oledDisplay.setBounds (getWidth() - 250, 12, 235, 120);

    // 5. Align step sequencer slots horizontally inside the main dashboard area
    const float stepAreaX = 195.0f;
    const float stepAreaWidth = static_cast<float> (getWidth()) - stepAreaX - 15.0f;
    const float stepSlotWidth = stepAreaWidth / 8.0f;

    for (int i = 0; i < 8; ++i)
    {
        const int slotX = static_cast<int> (stepAreaX + (static_cast<float> (i) * stepSlotWidth));
        
        // Vertical step sliders
        stepFaders[i].setBounds (slotX + 5, 185, static_cast<int> (stepSlotWidth - 10.0f), 150);

        // Knob dial rows below step vertical slots
        knobSliders[i].setBounds (slotX + 5, 350, static_cast<int> (stepSlotWidth - 10.0f), 55);

        // LFO rate selection boxes at the bottom of the column slot
        lfoPresetBoxes[i].setBounds (slotX + 5, 415, static_cast<int> (stepSlotWidth - 10.0f), 20);
    }
}

//==============================================================================
void PluginEditor::timerCallback()
{
    // A. Track button long presses (>1000ms triggers switch of active parameter editing anchor)
    if (isAPressed)
    {
        sceneAPressTime += 50;
        if (sceneAPressTime >= 1000)
        {
            sceneAPressTime = 0;
            isAPressed = false;
            audioProcessor.captureScene (true); // Swap scene buffer snapshot targets
            audioProcessor.setSceneBActive (false);
            audioProcessor.triggerSceneAnchorSwitch();
            sceneAButton.setToggleState (true, juce::dontSendNotification);
            sceneBButton.setToggleState (false, juce::dontSendNotification);
        }
    }

    if (isBPressed)
    {
        sceneBPressTime += 50;
        if (sceneBPressTime >= 1000)
        {
            sceneBPressTime = 0;
            isBPressed = false;
            audioProcessor.captureScene (false);
            audioProcessor.setSceneBActive (true);
            audioProcessor.triggerSceneAnchorSwitch();
            sceneBButton.setToggleState (true, juce::dontSendNotification);
            sceneAButton.setToggleState (false, juce::dontSendNotification);
        }
    }

    // B. Handle dynamic pill visual highlights based on active freeze flags
    bool activeFreezeState = audioProcessor.apvts.getRawParameterValue ("freeze")->load();
    oledDisplay.setFreezeActive (activeFreezeState);
    updateSliderTextBoxThemeColors();
}

void PluginEditor::sliderValueChanged (juce::Slider* slider)
{
    // C. Trigger Momentary Parameter overlay cards on dial modifications
    for (int i = 0; i < 8; ++i)
    {
        if (slider == &knobSliders[i])
        {
            juce::String name = "Knob " + juce::String (i + 1);
            float val = static_cast<float> (slider->getValue());
            int activePresetIndex = lfoPresetBoxes[i].getSelectedId() - 1;
            
            juce::String vibeText = (activePresetIndex >= 0 && activePresetIndex <= 4) 
                                    ? lfoPresetBoxes[i].getItemText (activePresetIndex) : "Off";
            
            oledDisplay.showParameterOverlay (name, val, vibeText);
            return;
        }
        else if (slider == &stepFaders[i])
        {
            juce::String name = "Step " + juce::String (i + 1);
            float val = static_cast<float> (slider->getValue());
            oledDisplay.showParameterOverlay (name, val, "No Modulation");
            return;
        }
    }

    if (slider == &morphSlider)
    {
        oledDisplay.showParameterOverlay ("Morph Track", static_cast<float> (slider->getValue()), "Crossfader");
    }
}

void PluginEditor::buttonClicked (juce::Button* button)
{
    // D. Sequential Click callbacks / latching operations
    if (button == &sceneAButton)
    {
        if (sceneAButton.isDown())
        {
            isAPressed = true;
            sceneAPressTime = 0;
        }
        else
        {
            isAPressed = false;
        }
    }
    else if (button == &sceneBButton)
    {
        if (sceneBButton.isDown())
        {
            isBPressed = true;
            sceneBPressTime = 0;
        }
        else
        {
            isBPressed = false;
        }
    }

    // Modifiers latch completions (e.g. INIT operations)
    if (initButton.getToggleState())
    {
        if (button == &diceMeloButton)
        {
            // Reset step faders back to defaults
            for (int i = 0; i < 8; ++i)
                stepFaders[i].setValue (1.0f);
            
            initButton.setToggleState (false, juce::sendNotification);
            initButton.repaint();
        }
    }
}

void PluginEditor::updateSliderTextBoxThemeColors()
{
    // Apply unified colors and floating pill aesthetics dynamically based on freeze state
    const bool isFrozen = audioProcessor.apvts.getRawParameterValue ("freeze")->load();
    const juce::Colour textDimColor = isFrozen ? juce::Colour::fromString ("#FF80D8FF") : juce::Colours::lightgrey;
    const juce::Colour pillBg = juce::Colour::fromString ("#FF0F1116");

    for (int i = 0; i < 8; ++i)
    {
        stepFaders[i].setColour (juce::Slider::textBoxTextColourId, textDimColor);
        stepFaders[i].setColour (juce::Slider::textBoxBackgroundColourId, pillBg);
        stepFaders[i].setColour (juce::Slider::textBoxOutlineColourId, isFrozen ? juce::Colour::fromString ("#FF80D8FF").withAlpha (0.3f) : juce::Colours::transparentBlack);

        knobSliders[i].setColour (juce::Slider::textBoxTextColourId, textDimColor);
        knobSliders[i].setColour (juce::Slider::textBoxBackgroundColourId, pillBg);
        knobSliders[i].setColour (juce::Slider::textBoxOutlineColourId, isFrozen ? juce::Colour::fromString ("#FF80D8FF").withAlpha (0.3f) : juce::Colours::transparentBlack);
    }
}