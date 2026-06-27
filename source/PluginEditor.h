#ifndef NAVY_ARP_EDITOR_H
#define NAVY_ARP_EDITOR_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// ==============================================================================
// Dynamically Compiled Global Color Themes Structure
// ==============================================================================
struct AppTheme
{
    juce::Colour background;
    juce::Colour border;
    juce::Colour leftAccent;
    juce::Colour rightAccent;
    juce::Colour textDim;
    juce::Colour trackBg;
    juce::Colour slotOutline;
    juce::Colour faderCap;
    juce::Colour unlitLed;

    static AppTheme get (int index)
    {
        AppTheme t;
        if (index == 1) // Skyline Eurorack (Light Beige Panel)
        {
            t.background    = juce::Colour (0xFFE2E0D8);
            t.border        = juce::Colour (0xFFB8B5AB);
            t.leftAccent    = juce::Colour (0xFFFF3B30); // Eurorack Red LED
            t.rightAccent   = juce::Colour (0xFF3A3A38); // Matte Charcoal
            t.textDim       = juce::Colour (0xFF1A1A18); // Calibrated dark grey for high contrast
            t.trackBg       = juce::Colour (0xFFD4D1C9);
            t.slotOutline   = juce::Colour (0xFFA8A59C);
            t.faderCap      = juce::Colour (0xFF1E1E1E);
            t.unlitLed      = juce::Colour (0xFFB8B5AB);
        }
        else if (index == 2) // Monochrome Minimal
        {
            t.background    = juce::Colour (0xFF101010);
            t.border        = juce::Colour (0xFF242424);
            t.leftAccent    = juce::Colour (0xFFFFFFFF); // Clean White
            t.rightAccent   = juce::Colour (0xFF88888A); // Mid Grey
            t.textDim       = juce::Colour (0xFF66666A);
            t.trackBg       = juce::Colour (0xFF080808);
            t.slotOutline   = juce::Colour (0xFF2D2D32);
            t.faderCap      = juce::Colour (0xFF222222);
            t.unlitLed      = juce::Colour (0xFF1C1C1F);
        }
        else if (index == 3) // Matrix Terminal
        {
            t.background    = juce::Colour (0xFF030A05);
            t.border        = juce::Colour (0xFF112A18);
            t.leftAccent    = juce::Colour (0xFF33FF33); // Matrix Green
            t.rightAccent   = juce::Colour (0xFF22AA22); 
            t.textDim       = juce::Colour (0xFF1E5F2E);
            t.trackBg       = juce::Colour (0xFF020603);
            t.slotOutline   = juce::Colour (0xFF0E451E);
            t.faderCap      = juce::Colour (0xFF112A18);
            t.unlitLed      = juce::Colour (0xFF0E1A11);
        }
        else // Default Navy Cyber (Dark default)
        {
            t.background    = juce::Colour (0xFF16181F);
            t.border        = juce::Colour (0xFF2A2E3D);
            t.leftAccent    = juce::Colour (0xFF00D2FF); // Cyan
            t.rightAccent   = juce::Colour (0xFFFFB300); // Amber
            t.textDim       = juce::Colour (0xFF55555C);
            t.trackBg       = juce::Colour (0xFF181C24);
            t.slotOutline   = juce::Colour (0xFF242835);
            t.faderCap      = juce::Colour (0xFF1E212A);
            t.unlitLed      = juce::Colour (0xFF1C1E24);
        }
        return t;
    }
};

// ==============================================================================
// Custom DJ TechTools / Chroma Caps Style Rotary & Linear LookAndFeel
// ==============================================================================
class ChromaCapsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChromaCapsLookAndFeel (PluginProcessor& p) : processor (p)
    {
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, 
                           float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, 
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto knobBounds = bounds.reduced (16.0f); // Leave room on boundaries for the 15 LEDs [5]
        auto radius = juce::jmin (knobBounds.getWidth(), knobBounds.getHeight()) / 2.0f;
        auto toX = bounds.getCentreX();
        auto toY = bounds.getCentreY();

        // Load active theme colors dynamically
        int themeIdx = static_cast<int> (processor.apvts.getRawParameterValue ("panelTheme")->load());
        auto t = AppTheme::get (themeIdx);

        // 1. Subtle drop shadow beneath the rubber cap [5]
        g.setColour (juce::Colour (themeIdx == 1 ? 0x25000000 : 0x45000000));
        g.fillEllipse (toX - radius + 2.0f, toY - radius + 4.0f, radius * 2.0f, radius * 2.0f);

        // 2. Matte rubberized cylindrical body [5]
        juce::Colour rubberBaseCol = (themeIdx == 1) ? juce::Colour (0xFF1E212A) : juce::Colour (0xFF1A1C24); 
        juce::ColourGradient grad (rubberBaseCol.brighter (0.12f), toX, toY - radius, 
                                   rubberBaseCol.darker (0.25f), toX, toY + radius, false);
        g.setGradientFill (grad);
        g.fillEllipse (toX - radius, toY - radius, radius * 2.0f, radius * 2.0f);

        // 3. Highlighted outer lip edge of the cap
        g.setColour (juce::Colour (0xFF2D313D));
        g.drawEllipse (toX - radius, toY - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // 4. Colored rubber indicator pointer strip [5]
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        
        // Symmetrically map accents based on left and right slider columns
        juce::Colour accentCol = (slider.getComponentID() == "rhythmMorph" || slider.getComponentID() == "rest" || 
                                  slider.getID() == "legato" || slider.getComponentID() == "rate")
                                 ? t.leftAccent : t.rightAccent;

        juce::Path pointerPath;
        float pointerLength = radius * 0.95f;
        float pointerThickness = radius * 0.16f;
        pointerPath.addRoundedRectangle (-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, pointerThickness * 0.3f);

        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (angle).translated (toX, toY));
        g.setColour (accentCol);
        g.fillPath (pointerPath);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (pointerPath, juce::PathStrokeType (0.7f)); 
        g.restoreState();

        // Center Cap dome
        float centerRadius = radius * 0.38f;
        g.setColour (rubberBaseCol.darker (0.5f));
        g.fillEllipse (toX - centerRadius, toY - centerRadius, centerRadius * 2.0f, centerRadius * 2.0f);

        // 5. Circular 15-LED rings with real-time modulation visual updates [5]
        float ledRingRadius = radius + 9.5f; 
        int numLeds = 15;
        juce::Colour ledActiveCol = accentCol;
        float visualValue = sliderPos;
        bool lfoActive = false;

        // Safely identify the rotary slider using the component ID [5]
        juce::String pId = slider.getComponentID();
        if (pId.isNotEmpty())
        {
            int lfoRateVal = 0;
            
            if (pId == "rhythmMorph") {
                lfoRateVal = static_cast<int> (*processor.apvts.getRawParameterValue (IDs::rhythmMorphLfoRate.getParamID()));
                visualValue = (lfoRateVal > 0) ? processor.activeMorph : sliderPos;
            }
            else if (pId == "rest") {
                lfoRateVal = static_cast<int> (*processor.apvts.getRawParameterValue (IDs::restLfoRate.getParamID()));
                visualValue = (lfoRateVal > 0) ? processor.activeRest : sliderPos;
            }
            else if (pId == "legato") {
                lfoRateVal = static_cast<int> (*processor.apvts.getRawParameterValue (IDs::legatoLfoRate.getParamID()));
                float baseLegato = static_cast<float> (*processor.apvts.getRawParameterValue (IDs::legato.getParamID()));
                float rawLegato = (lfoRateVal > 0) ? processor.activeLegato : baseLegato;
                visualValue = (rawLegato - 0.1f) / 0.9f;
            }
            else if (pId == "rate") {
                lfoRateVal = static_cast<int> (*processor.apvts.getRawParameterValue (IDs::rateLfoRate.getParamID()));
                float baseRate = static_cast<float> (*processor.apvts.getRawParameterValue (IDs::rate.getParamID()));
                float rawRate = (lfoRateVal > 0) ? static_cast<float>(processor.activeRateIdx) : baseRate;
                visualValue = rawRate / 3.0f;
            }
            else if (pId == "entropy") {
                lfoRateVal = static_cast<int> (*processor.apvts.getRawParameterValue (IDs::entropyLfoRate.getParamID()));
                float baseEntropy = static_cast<float> (*processor.apvts.getRawParameterValue (IDs::entropy.getParamID()));
                float rawEntropy = (lfoRateVal > 0) ? processor.activeEntropy : baseEntropy;
                visualValue = (rawEntropy + 1.0f) * 0.5f;
            }
            else if (pId == "harmony") {
                lfoRateVal = static_cast<int> (*processor.apvts.getRawParameterValue (IDs::harmonyLfoRate.getParamID()));
                visualValue = (lfoRateVal > 0) ? processor.activeHarmony : sliderPos;
            }
            else if (pId == "chaos") {
                lfoRateVal = static_cast<int> (*processor.apvts.getRawParameterValue (IDs::chaosLfoRate.getParamID()));
                visualValue = (lfoRateVal > 0) ? processor.activeChaos : sliderPos;
            }
            else if (pId == "octaves") {
                lfoRateVal = static_cast<int> (*processor.apvts.getRawParameterValue (IDs::octavesLfoRate.getParamID()));
                float baseOctaves = static_cast<float> (*processor.apvts.getRawParameterValue (IDs::octaves.getParamID()));
                float rawOctaves = (lfoRateVal > 0) ? static_cast<float>(processor.activeOctavesVal) : baseOctaves;
                visualValue = (rawOctaves + 3.0f) / 6.0f; // Symmetrical scale layout matching -3 to +3
            }
            
            lfoActive = (lfoRateVal > 0);
            
            // Shift neon LED colors to indicate focus
            if (lfoActive)
            {
                ledActiveCol = (pId == "rhythmMorph" || pId == "rest" || pId == "legato" || pId == "rate") 
                               ? juce::Colour (0xFFFF00D2)  // Neon Magenta (Left)
                               : juce::Colour (0xFF9933FF); // Deep Purple (Right)
            }
        }

        // Draw 15 discrete circular LED dots
        for (int i = 0; i < numLeds; ++i)
        {
            float pct = static_cast<float>(i) / static_cast<float>(numLeds - 1);
            float ledAngle = rotaryStartAngle + pct * (rotaryEndAngle - rotaryStartAngle);
            
            float ledX = toX + ledRingRadius * std::sin (ledAngle);
            float ledY = toY - ledRingRadius * std::cos (ledAngle);
            
            bool isLit = (pct <= visualValue);
            
            if (isLit)
            {
                g.setColour (ledActiveCol);
                g.fillEllipse (ledX - 1.8f, ledY - 1.8f, 3.6f, 3.6f);
                
                g.setColour (ledActiveCol.withAlpha (0.22f));
                g.drawEllipse (ledX - 3.2f, ledY - 3.2f, 6.4f, 6.4f, 1.2f);
            }
            else
            {
                g.setColour (t.unlitLed); // Custom unlit led color mapped to panel theme
                g.fillEllipse (ledX - 1.5f, ledY - 1.5f, 3.0f, 3.0f);
            }
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        // Custom vertical mixer-style faders
        if (style == juce::Slider::LinearVertical)
        {
            auto trackWidth = 4.0f;
            auto trackX = x + width * 0.5f - trackWidth * 0.5f;
            
            // Load active theme colors dynamically [NEW]
            int themeIdx = static_cast<int> (processor.apvts.getRawParameterValue ("panelTheme")->load());
            auto t = AppTheme::get (themeIdx);

            // Recessed slot
            g.setColour (t.trackBg);
            g.fillRoundedRectangle (trackX, (float)y, trackWidth, (float)height, trackWidth * 0.5f);
            
            g.setColour (t.slotOutline);
            g.drawRoundedRectangle (trackX - 1.0f, (float)y, trackWidth + 2.0f, (float)height, trackWidth * 0.5f, 1.0f);

            // Tactile Chroma Fader Cap
            float capWidth = juce::jmin (26.0f, width * 0.8f);
            float capHeight = 14.0f;
            float capX = x + width * 0.5f - capWidth * 0.5f;
            float capY = sliderPos - capHeight * 0.5f;

            // Shadow
            g.setColour (juce::Colour (themeIdx == 1 ? 0x15000000 : 0x45000000));
            g.fillRoundedRectangle (capX + 1.0f, capY + 3.0f, capWidth, capHeight, 2.0f);

            // Tactile Rubberized Fader Cap Body
            juce::Colour capBaseCol = t.faderCap;
            juce::ColourGradient capGrad (capBaseCol.brighter (0.1f), capX, capY,
                                         capBaseCol.darker (0.2f), capX, capY + capHeight, false);
            g.setGradientFill (capGrad);
            g.fillRoundedRectangle (capX, capY, capWidth, capHeight, 2.0f);

            // Borders
            g.setColour (juce::Colour (0xFF3A3F4E));
            g.drawRoundedRectangle (capX, capY, capWidth, capHeight, 2.0f, 1.0f);

            // Horizontal high-contrast stripe
            g.setColour (slider.findColour (juce::Slider::thumbColourId));
            float stripeHeight = 2.0f;
            g.fillRect (capX + 2.0f, capY + capHeight * 0.5f - stripeHeight * 0.5f, capWidth - 4.0f, stripeHeight);
        }
        else if (style == juce::Slider::LinearHorizontal) // Crossfader
        {
            // Load active theme colors dynamically
            int themeIdx = static_cast<int> (processor.apvts.getRawParameterValue ("panelTheme")->load());
            auto t = AppTheme::get (themeIdx);

            auto trackHeight = 4.0f;
            auto trackY = y + height * 0.5f - trackHeight * 0.5f;

            // Recessed horizontal slot
            g.setColour (t.trackBg);
            g.fillRoundedRectangle ((float)x, trackY, (float)width, trackHeight, trackHeight * 0.5f);
            g.setColour (t.slotOutline);
            g.drawRoundedRectangle ((float)x, trackY, (float)width, trackHeight, trackHeight * 0.5f, 1.0f);

            // 3D DJ-Style Crossfader Cap
            float capWidth = 28.0f;
            float capHeight = 16.0f;
            float capX = sliderPos - capWidth * 0.5f;
            float capY = y + height * 0.5f - capHeight * 0.5f;

            // Fader shadow
            g.setColour (juce::Colour (themeIdx == 1 ? 0x15000000 : 0x45000000));
            g.fillRoundedRectangle (capX + 1.0f, capY + 3.0f, capWidth, capHeight, 2.0f);

            // Tactile Rubberized Cap Body
            juce::Colour capBaseCol = t.faderCap;
            juce::ColourGradient capGrad (capBaseCol.brighter (0.1f), capX, capY,
                                         capBaseCol.darker (0.2f), capX, capY + capHeight, false);
            g.setGradientFill (capGrad);
            g.fillRoundedRectangle (capX, capY, capWidth, capHeight, 2.0f);

            // Bord