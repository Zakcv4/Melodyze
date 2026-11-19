#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>

struct MelNote {
    int midiNote = 60;
    double startBeat = 0.0;
    double lengthBeats = 0.5;
    float velocity = 0.8f;
    int layer = 0; // 0 = Lead, 1 = Piano, 2 = Strings
};

struct SimpleVoice {
    bool active = false;
    int note = 60;
    int layer = 0;
    double phase = 0.0;
    double phaseInc = 0.0;
    float env = 0.0f;
    float velocity = 0.8f;
    double releaseBeat = 0.0;
};

class MelodyzeAudioProcessor : public juce::AudioProcessor {
public:
    MelodyzeAudioProcessor();
    ~MelodyzeAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Melodyze"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Melodyze Controls
    void setChordProgression(const juce::String& text);
    juce::String getChordProgression() const { return chordText; }

    void setStyle(int styleIndex);
    int getStyle() const { return currentStyle; }

    void setDensity(float d);
    float getDensity() const { return density; }

    void setHumanize(float h);
    float getHumanize() const { return humanize; }

    void setPreviewEnabled(bool enabled) { previewEnabled = enabled; }
    bool isPreviewEnabled() const { return previewEnabled; }

    void regenerate();
    juce::File exportMidiToTempFile(int layerFilter); // -1 = All, 0 = Lead, 1 = Piano, 2 = Strings

private:
    struct ParsedChord {
        int root = 9; // A
        std::vector<int> intervals = { 0, 3, 7 }; // Minor triad
    };

    std::vector<ParsedChord> parseChords(const juce::String& text);
    void generatePatterns();

    juce::CriticalSection lock;
    std::vector<MelNote> notes;

    juce::String chordText = "Am F C G";
    int currentStyle = 0; // 0: Trap Pluck, 1: Emotional Piano, 2: Cinematic Strings, 3: Lo-Fi Bounce
    float density = 0.65f;
    float humanize = 0.25f;
    bool previewEnabled = true;
    int randomSeed = 1337;

    double currentSampleRate = 44100.0;
    double lastPpq = -1.0;
    static constexpr double loopLengthBeats = 16.0; // 4 bars

    std::array<SimpleVoice, 24> voices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MelodyzeAudioProcessor)
};
