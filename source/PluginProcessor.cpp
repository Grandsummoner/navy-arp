#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      ),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    sceneA = SceneState();
    sceneB = SceneState();
}

PluginProcessor::~PluginProcessor() {}

const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }
bool PluginProcessor::acceptsMidi() const { return true; }
bool PluginProcessor::producesMidi() const { return true; }

bool PluginProcessor::isMidiEffect() const
{
    return false; // MUST be standard Instrument to allow 2-track routing in Ableton
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

// ==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl (noteLock);
    mSampleRate = sampleRate;
    mLastStep = -1;
    mLastNotePlayed = -1;
    mNoteOffTime = 0;
    mTimeInSamples = 0;
    activeHeldNotes.clear();
    latchedNotes.clear();
    isFirstNoteOfNewChord = true;
    isCurrentlyPlayingUI.store (false);
    juce::ignoreUnused (samplesPerBlock);
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

// ==============================================================================
// 100% NULL-SAFE PARAMETER READING HELPER (RESOLVES SEGFAULT 139)
// ==============================================================================
float PluginProcessor::getParameterValue (const juce::String& paramId) const
{
    if (auto* param = apvts.getRawParameterValue (paramId))
        return param->load();
    return 0.5f; // Safe fallback
}

std::vector<int> PluginProcessor::generateEuclideanPattern (int steps, int pulses)
{
    std::vector<int> pattern(steps, 0);
    if (pulses <= 0) return pattern;
    if (pulses >= steps) { std::fill(pattern.begin(), pattern.end(), 1); return pattern; }

    int bucket = 0;
    for (int i = 0; i < steps; ++i)
    {
        bucket += pulses;
        if (bucket >= steps) { bucket -= steps; pattern[i] = 1; }
    }
    return pattern;
}

void PluginProcessor::updateLfoModulations (int numSamples, double bpm)
{
    double samplesPerBeat = mSampleRate * (60.0 / (bpm > 0 ? bpm : 120.0));
    double samplesPerBar = samplesPerBeat * 4.0;
    double sampleDelta = numSamples;

    // Uses safe parameter getter to prevent crashes during rapid automation
    float rawCycleIndex = getParameterValue (IDs::cycleLength.getParamID());
    int cycleIndex = juce::jlimit (0, 3, static_cast<int> (rawCycleIndex));
    int cycleBars = 4;
    if (cycleIndex == 0)      cycleBars = 1;
    else if (cycleIndex == 1) cycleBars = 2;
    else if (cycleIndex == 2) cycleBars = 4;
    else if (cycleIndex == 3) cycleBars = 8;

    if (cycleBars > 0)
        currentBarInCycle = (static_cast<int>(std::floor(mSongPositionPPQ / 4.0)) % cycleBars) + 1;
    else
        currentBarInCycle = 1;

    float baseEntropy = getParameterValue (IDs::entropy.getParamID());

    float absEntropy = std::abs(baseEntropy);
    int stepInterval = 1; 
    if (absEntropy > 0.33f && absEntropy <= 0.66f) stepInterval = 2; 
    else if (absEntropy > 0.66f) stepInterval = 4; 

    if (currentStep == 0 && mLastStep == 7) 
    {
        if (baseEntropy > 0.05f) accumulatedPitchOffset += stepInterval;
        else if (baseEntropy < -0.05f) accumulatedPitchOffset -= stepInterval;
        if (currentBarInCycle == 1) accumulatedPitchOffset = 0.0f;
    }

    lfoPhaseLegato += (sampleDelta / (samplesPerBar * 2.0)); 
    if (lfoPhaseLegato >= 1.0) lfoPhaseLegato -= 1.0;
    modLegato = getParameter(IDs::legato.getParamID())->getValue() + 0.2f * static_cast<float>(std::sin(lfoPhaseLegato * juce::MathConstants<double>::twoPi));
    modLegato = juce::jlimit(0.1f, 1.0f, modLegato);

    modRest = getParameter(IDs::rest.getParamID())->getValue();
    modHarmony = getParameter(IDs::harmony.getParamID())->getValue();
    modChaos = getParameter(IDs::chaos.getParamID())->getValue();
}

void PluginProcessor::scheduleNoteOff (juce::MidiBuffer& midi, int pitch, int delaySamples)
{
    if (delaySamples <= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (1, pitch), 0);
    }
    else
    {
        scheduledNoteOffs.push_back ({ pitch, delaySamples });
    }
}

// ==============================================================================
// REAL-TIME DUAL-CLOCK SEQUENCER ENGINE (BLUEARP & BLEASS STANDARDS)
// ==============================================================================
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    const juce::ScopedLock sl (noteLock);

    bool isPlaying = false;
    double bpm = 120.0;
    mSongPositionPPQ = 0.0;

    if (auto* playhead = getPlayHead())
    {
        if (auto pos = playhead->getPosition())
        {
            isPlaying = pos->getIsPlaying();
            auto bpmOpt = pos->getBpm();
            if (bpmOpt.hasValue()) bpm = *bpmOpt;
            auto ppqOpt = pos->getPpqPosition();
            if (ppqOpt.hasValue()) mSongPositionPPQ = *ppqOpt;
        }
    }

    int numSamples = buffer.getNumSamples();
    updateLfoModulations (numSamples, bpm);

    bool isLatchActive = getParameterValue (IDs::latch.getParamID()) > 0.5f;

    // Read fader probabilities safely
    float activeFaderProb[8];
    for (int i = 0; i < 8; ++i)
        activeFaderProb[i] = getParameterValue (juce::String ("fader" + juce::String (i + 1)));

    // Process Note-Off Queue
    juce::MidiBuffer processedMidi;
    for (auto it = scheduledNoteOffs.begin(); it != scheduledNoteOffs.end();)
    {
        it->second -= numSamples;
        if (it->second <= 0)
        {
            processedMidi.addEvent (juce::MidiMessage::noteOff (1, it->first), 0);
            it = scheduledNoteOffs.erase(it);
        }
        else ++it;
    }

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            if (std::find (activeHeldNotes.begin(), activeHeldNotes.end(), note) == activeHeldNotes.end())
            {
                activeHeldNotes.push_back (note);
                std::sort (activeHeldNotes.begin(), activeHeldNotes.end());
            }

            if (isLatchActive)
            {
                if (isFirstNoteOfNewChord)
                {
                    for (int n : latchedNotes) scheduleNoteOff (processedMidi, n, 0);
                    latchedNotes.clear();
                    isFirstNoteOfNewChord = false;
                }
                if (std::find (latchedNotes.begin(), latchedNotes.end(), note) == latchedNotes.end())
                {
                    latchedNotes.push_back (note);
                    std::sort (latchedNotes.begin(), latchedNotes.end());
                }
            }
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            activeHeldNotes.erase (std::remove (activeHeldNotes.begin(), activeHeldNotes.end(), note), activeHeldNotes.end());
            if (activeHeldNotes.empty()) isFirstNoteOfNewChord = true;
        }
    }
    midiMessages.clear();

    const auto& notesToPlay = isLatchActive ? latchedNotes : activeHeldNotes;
    
    // Store active playhead state atomically (Lock-free UI read)
    isCurrentlyPlayingUI.store (! notesToPlay.empty());

    if (mLastNotePlayed != -1)
    {
        mNoteOffTime -= numSamples;
        if (mNoteOffTime <= 0)
        {
            processedMidi.addEvent (juce::MidiMessage::noteOff (1, mLastNotePlayed), 0);
            mLastNotePlayed = -1;
        }
    }

    if (! notesToPlay.empty())
    {
        if (isPlaying)
        {
            // Clock 1: PPQ-based DAW Grid Sync
            double stepLengthPPQ = 0.25;
            int stepIndex = static_cast<int> (std::floor (mSongPositionPPQ / 0.25)) % 8;

            if (stepIndex != mLastStep)
            {
                mLastStep = stepIndex;
                currentStep = stepIndex;
                triggerArpStep (activeFaderProb[currentStep], notesToPlay, processedMidi, bpm);
            }
        }
        else
        {
            // Clock 2: Standalone Free-Running Sample-Clock
            mTimeInSamples += numSamples;
            if (mTimeInSamples >= stepSamples)
            {
                mTimeInSamples = 0;
                currentStep = (currentStep + 1) % 8;
                mLastStep = currentStep;
                triggerArpStep (activeFaderProb[currentStep], notesToPlay, processedMidi, bpm);
            }
        }
    }
    else
    {
        mLastStep = -1;
        currentStep = 0;
    }

    midiMessages.swapWith (processedMidi);
}

void PluginProcessor::triggerArpStep (float stepProbability, const std::vector<int>& notesToPlay, juce::MidiBuffer& processedMidi, double bpm)
{
    bool shouldPlay = (juce::Random::getSystemRandom().nextFloat() <= stepProbability);
    bool isRest = (juce::Random::getSystemRandom().nextFloat() <= modRest);

    if (shouldPlay && ! isRest && ! notesToPlay.empty())
    {
        if (mLastNotePlayed != -1)
        {
            processedMidi.addEvent (juce::MidiMessage::noteOff (1, mLastNotePlayed), 0);
            mLastNotePlayed = -1;
        }

        // Safe parameter reading prevents out-of-bounds array access under automation
        int rootKeyIdx = juce::jlimit (0, 11, static_cast<int> (getParameterValue (IDs::rootKey.getParamID())));
        int scaleIdx = juce::jlimit (0, 9, static_cast<int> (getParameterValue (IDs::scaleType.getParamID())));

        std::vector<int> scaleOffsets = { 0, 2, 4, 5, 7, 9, 11, 12 }; 
        if (scaleIdx == 1)      scaleOffsets = { 0, 2, 3, 5, 7, 8, 10, 12 }; 
        else if (scaleIdx == 2) scaleOffsets = { 0, 3, 5, 7, 10, 12, 15, 17 }; 
        else if (scaleIdx == 3) scaleOffsets = { 0, 2, 4, 7, 9, 12, 14, 16 };  
        else if (scaleIdx == 4) scaleOffsets = { 0, 2, 3, 5, 7, 9, 10, 12 };  
        else if (scaleIdx == 5) scaleOffsets = { 0, 1, 3, 5, 7, 8, 10, 12 };  
        else if (scaleIdx == 6) scaleOffsets = { 0, 2, 4, 6, 7, 9, 11, 12 };  
        else if (scaleIdx == 7) scaleOffsets = { 0, 2, 4, 5, 7, 9, 10, 12 };  
        else if (scaleIdx == 8) scaleOffsets = { 0, 2, 3, 5, 7, 8, 11, 12 };  
        else if (scaleIdx == 9) scaleOffsets = { 0, 2, 3, 5, 7, 9, 11, 12 };  

        int rawPitch = notesToPlay[currentStep % notesToPlay.size()];
        int octave = (rawPitch / 12) * 12;
        int targetPitch = octave + rootKeyIdx + scaleOffsets[currentStep] + static_cast<int>(accumulatedPitchOffset);

        if (modChaos > 0.2f && juce::Random::getSystemRandom().nextFloat() <= modChaos)
            targetPitch += (juce::Random::getSystemRandom().nextBool() ? 12 : -12);

        targetPitch = juce::jlimit(0, 127, targetPitch);
        int durationSamples = static_cast<int>(mSampleRate * (60.0 / (bpm > 0 ? bpm : 120.0)) * 0.25 * modLegato);

        processedMidi.addEvent (juce::MidiMessage::noteOn (1, targetPitch, static_cast<juce::uint8>(100)), 0);
        mLastNotePlayed = targetPitch;
        mNoteOffTime = durationSamples;
    }
}

// ==============================================================================
void PluginProcessor::savePreset (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 8) return;
    for (int i = 0; i < 8; ++i) presets[slotIndex].faders[i] = getParameterValue (juce::String ("fader" + juce::String (i + 1)));
    presets[slotIndex].rhythmMorph = getParameterValue (IDs::rhythmMorph.getParamID());
    presets[slotIndex].rest = getParameterValue (IDs::rest.getParamID());
    presets[slotIndex].legato = getParameterValue (IDs::legato.getParamID());
    presets[slotIndex].entropy = getParameterValue (IDs::entropy.getParamID());
    presets[slotIndex].harmony = getParameterValue (IDs::harmony.getParamID());
    presets[slotIndex].chaos = getParameterValue (IDs::chaos.getParamID());
    presetSlotsSaved[slotIndex] = true;
}

void PluginProcessor::loadPreset (int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 8 || ! presetSlotsSaved[slotIndex]) return;
    apvts.getParameter (IDs::rhythmMorph.getParamID())->setValueNotifyingHost (presets[slotIndex].rhythmMorph);
    apvts.getParameter (IDs::rest.getParamID())->setValueNotifyingHost (presets[slotIndex].rest);
    apvts.getParameter (IDs::legato.getParamID())->setValueNotifyingHost (presets[slotIndex].legato);
    apvts.getParameter (IDs::entropy.getParamID())->setValueNotifyingHost (presets[slotIndex].entropy);
    apvts.getParameter (IDs::harmony.getParamID())->setValueNotifyingHost (presets[slotIndex].harmony);
    apvts.getParameter (IDs::chaos.getParamID())->setValueNotifyingHost (presets[slotIndex].chaos);

    for (int i = 0; i < 8; ++i)
        apvts.getParameter (juce::String ("fader" + juce::String (i + 1)))->setValueNotifyingHost (presets[slotIndex].faders[i]);
}

void PluginProcessor::captureSceneA()
{
    for (int i = 0; i < 8; ++i) sceneA.faders[i] = getParameterValue (juce::String ("fader" + juce::String (i + 1)));
    sceneA.rhythmMorph = getParameterValue (IDs::rhythmMorph.getParamID());
    sceneA.rest = getParameterValue (IDs::rest.getParamID());
    sceneA.legato = getParameterValue (IDs::legato.getParamID());
    sceneA.entropy = getParameterValue (IDs::entropy.getParamID());
    sceneA.harmony = getParameterValue (IDs::harmony.getParamID());
    sceneA.chaos = getParameterValue (IDs::chaos.getParamID());
    hasSceneA = true;
}

void PluginProcessor::captureSceneB()
{
    for (int i = 0; i < 8; ++i) sceneB.faders[i] = getParameterValue (juce::String ("fader" + juce::String (i + 1)));
    sceneB.rhythmMorph = getParameterValue (IDs::rhythmMorph.getParamID());
    sceneB.rest = getParameterValue (IDs::rest.getParamID());
    sceneB.legato = getParameterValue (IDs::legato.getParamID());
    sceneB.entropy = getParameterValue (IDs::entropy.getParamID());
    sceneB.harmony = getParameterValue (IDs::harmony.getParamID());
    sceneB.chaos = getParameterValue (IDs::chaos.getParamID());
    hasSceneB = true;
}

void PluginProcessor::diceMelody()
{
    auto* random = &juce::Random::getSystemRandom();
    for (int i = 1; i <= 8; ++i) apvts.getParameter ("fader" + juce::String(i))->setValueNotifyingHost (random->nextFloat());
}

void PluginProcessor::diceRhythm()
{
    auto* random = &juce::Random::getSystemRandom();
    apvts.getParameter (IDs::rhythmMorph.getParamID())->setValueNotifyingHost (random->nextFloat());
    apvts.getParameter (IDs::rest.getParamID())->setValueNotifyingHost (random->nextFloat() * 0.5f);
    apvts.getParameter (IDs::legato.getParamID())->setValueNotifyingHost (0.2f + random->nextFloat() * 0.8f);
}

void PluginProcessor::resetAccumulator() { const juce::ScopedLock sl (noteLock); accumulatedPitchOffset = 0.0f; }
void PluginProcessor::resetRhythm() { apvts.getParameter(IDs::rhythmMorph.getParamID())->setValueNotifyingHost(0.0f); }

bool PluginProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    for (int i = 1; i <= 8; ++i)
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("fader" + juce::String (i), 1), "Fader " + juce::String (i), 0.0f, 1.0f, 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (IDs::rhythmMorph, "Rhythm Morph", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (IDs::rest, "Rest", 0.0f, 1.0f, 0.1f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (IDs::legato, "Legato", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (IDs::entropy, "Entropy", -1.0f, 1.0f, 0.0f)); // Bipolar -1.0 to +1.0
    params.push_back (std::make_unique<juce::AudioParameterFloat> (IDs::harmony, "Harmony", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (IDs::chaos, "Chaos", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (IDs::morph, "Morph Crossfader", 0.0f, 1.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (IDs::latch, "Latch Mode", false));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }