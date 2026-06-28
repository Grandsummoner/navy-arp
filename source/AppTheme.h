#pragma once

#include <JuceHeader.h>

namespace AppTheme
{
    // Obsidian Dark Dashboard Backgrounds
    const juce::Colour obsidianBg           = juce::Colour::fromString ("#FF0F1116"); // #0F1116
    const juce::Colour dashboardBg          = juce::Colour::fromString ("#FF1E2127"); // #1E2127
    const juce::Colour darkShadow           = juce::Colour::fromString ("#FF050507");

    // Unified Ice Blue Highlights (Freeze Mode & Active States)
    const juce::Colour iceBlueActive        = juce::Colour::fromString ("#FF80D8FF"); // #80D8FF
    const juce::Colour iceBlueDim           = juce::Colour::fromString ("#FF00B0FF");
    
    // Core Functional Colors
    const juce::Colour textDim              = juce::Colour::fromString ("#FF9E9E9E");
    const juce::Colour textBright           = juce::Colours::white;
    const juce::Colour outlineGrey          = juce::Colour::fromString ("#FF303030");

    // Standard Theme Palettes
    inline juce::Colour getThemeColor (bool isFreezeActive)
    {
        return isFreezeActive ? iceBlueActive : juce::Colour::fromString ("#FF2196F3");
    }
}