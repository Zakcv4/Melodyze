#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

MelodyzeAudioProcessor::MelodyzeAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    generatePatterns();
}

void MelodyzeAudioProcessor::prepareToPlay(double sampleRate, int) {
    currentSampleRate = sampleRate;
    for (auto& v : voices) v.active = false;
    lastPpq = -1.0;
}

void MelodyzeAudioProcessor::setChordProgression(const juce::String& text) {
    chordText = text.isEmpty() ? "Am F C G" : text;
    generatePatterns();
}

void MelodyzeAudioProcessor::setStyle(int styleIndex) {
    currentStyle = juce::jlimit(0, 3, styleIndex);
    generatePatterns();
}

void MelodyzeAudioProcessor::setDensity(float d) {
    density = juce::jlimit(0.15f, 1.0f, d);
    generatePatterns();
}

void MelodyzeAudioProcessor::setHumanize(float h) {
    humanize = juce::jlimit(0.0f, 1.0f, h);
    generatePatterns();
}

void MelodyzeAudioProcessor::regenerate() {
    randomSeed = juce::Random::getSystemRandom().nextInt();
    generatePatterns();
}

std::vector<MelodyzeAudioProcessor::ParsedChord> MelodyzeAudioProcessor::parseChords(const juce::String& text) {
    std::vector<ParsedChord> result;
    auto tokens = juce::StringArray::fromTokens(text, " ,|-", "");

    for (auto tok : tokens) {
        tok = tok.trim();
        if (tok.isEmpty()) continue;

        int root = 0;
        juce::juce_wchar c = juce::CharacterFunctions::toUpperCase(tok[0]);
        switch (c) {
            case 'C': root = 0; break;
            case 'D': root = 2; break;
            case 'E': root = 4; break;
            case 'F': root = 5; break;
            case 'G': root = 7; break;
            case 'A': root = 9; break;
            case 'B': root = 11; break;
            default: continue;
        }

        int idx = 1;
        if (tok.length() > 1) {
            if (tok[1] == '#') { root = (root + 1) % 12; idx = 2; }
            else if (tok[1] == 'b') { root = (root + 11) % 12; idx = 2; }
        }

        auto qual = tok.substring(idx).toLowerCase();
        ParsedChord chord;
        chord.root = root;

        if (qual.startsWith("dim")) chord.intervals = { 0, 3, 6 };
        else if (qual.startsWith("m") && !qual.startsWith("maj")) chord.intervals = { 0, 3, 7 };
        else chord.intervals = { 0, 4, 7 };

        if (qual.contains("maj7")) chord.intervals.push_back(11);
        else if (qual.contains("7")) chord.intervals.push_back(10);

        result.push_back(chord);
    }

    if (result.empty()) result.push_back({ 9, { 0, 3, 7 } }); // Default Am
    return result;
}

void MelodyzeAudioProcessor::generatePatterns() {
    auto chords = parseChords(chordText);
    juce::Random rng(randomSeed);
    std::vector<MelNote> generated;

    double beatsPerChord = loopLengthBeats / static_cast<double>(chords.size());

    for (size_t i = 0; i < chords.size(); ++i) {
        const auto& chord = chords[i];
        double chordStart = i * beatsPerChord;

        // 1. PIANO LAYER (Layer 1)
        if (currentStyle == 1 || currentStyle == 3) {
            // Broken/Arp piano or Lo-Fi bounce
            double step = (currentStyle == 1) ? 0.5 : 1.0;
            int steps = static_cast<int>(beatsPerChord / step);
            for (int s = 0; s < steps; ++s) {
                int interval = chord.intervals[s % chord.intervals.size()];
                int note = 48 + chord.root + interval + (s >= 4 ? 12 : 0);
                double jitter = (rng.nextFloat() - 0.5f) * 0.04 * humanize;
                float vel = juce::jlimit(0.3f, 0.95f, 0.72f + (rng.nextFloat() - 0.5f) * 0.25f * humanize);
                generated.push_back({ note, juce::jmax(0.0, chordStart + s * step + jitter), step * 0.9, vel, 1 });
            }
        } else {
            // Warm block chords
            for (int interval : chord.intervals) {
                int note = 48 + chord.root + interval;
                double strum = rng.nextFloat() * 0.03 * humanize;
                generated.push_back({ note, chordStart + strum, beatsPerChord * 0.92, 0.65f, 1 });
            }
        }

        // 2. STRINGS LAYER (Layer 2) - Sustained upper harmony
        int stringInterval = chord.intervals[chord.intervals.size() > 1 ? 1 : 0];
        int stringNote = 60 + chord.root + stringInterval;
        generated.push_back({ stringNote, chordStart, beatsPerChord * 0.98, 0.55f, 2 });
        generated.push_back({ 48 + chord.root, chordStart, beatsPerChord * 0.98, 0.50f, 2 });

        // 3. LEAD MELODY LAYER (Layer 0)
        double grid = (currentStyle == 0) ? 0.25 : 0.5; // 16th notes for Trap, 8th for others
        int numSteps = static_cast<int>(beatsPerChord / grid);

        for (int s = 0; s < numSteps; ++s) {
            bool isStrongBeat = (s % 4 == 0);
            float chance = isStrongBeat ? juce::jmin(0.95f, density + 0.25f) : density * 0.75f;
            if (rng.nextFloat() > chance) continue;

            int toneIdx = rng.nextInt(static_cast<int>(chord.intervals.size()));
            int interval = chord.intervals[toneIdx];

            // Passing tone on weak beats for melodic movement
            if (!isStrongBeat && rng.nextFloat() < 0.35f) {
                interval += (rng.nextBool() ? 2 : -2);
            }

            int octave = (rng.nextFloat() < 0.25f) ? 84 : 72;
            int midiNote = octave + chord.root + interval;
            double len = grid * (rng.nextBool() ? 1.0 : 1.8);
            if (chordStart + s * grid + len > chordStart + beatsPerChord)
                len = (chordStart + beatsPerChord) - (chordStart + s * grid);

            double jitter = (rng.nextFloat() - 0.5f) * 0.03 * humanize;
            float vel = juce::jlimit(0.4f, 1.0f, (isStrongBeat ? 0.88f : 0.72f) + (rng.nextFloat() - 0.5f) * 0.25f * humanize);
            generated.push_back({ midiNote, juce::jmax(0.0, chordStart + s * grid + jitter), juce::jmax(0.1, len), vel, 0 });
        }
    }

    const juce::ScopedLock sl(lock);
    notes = std::move(generated);
}

void MelodyzeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    buffer.clear();
    bool isPlaying = false;
    double ppq = 0.0;
    double bpm = 120.0;

    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            isPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto b = pos->getBpm()) bpm = *b;
        }
    }

    if (!isPlaying) {
        for (auto& v : voices) v.active = false;
        lastPpq = -1.0;
        return;
    }

    double beatsPerSample = (bpm / 60.0) / currentSampleRate;
    double blockBeats = beatsPerSample * buffer.getNumSamples();
    double loopPos = std::fmod(ppq, loopLengthBeats);
    if (loopPos < 0.0) loopPos += loopLengthBeats;

    {
        const juce::ScopedLock sl(lock);
        for (const auto& n : notes) {
            bool triggered = (n.startBeat >= loopPos && n.startBeat < loopPos + blockBeats);
            if (triggered) {
                int sampleOffset = juce::jlimit(0, buffer.getNumSamples() - 1,
                    static_cast<int>((n.startBeat - loopPos) / beatsPerSample));

                // Output live MIDI (Channel 1=Lead, 2=Piano, 3=Strings)
                midiMessages.addEvent(juce::MidiMessage::noteOn(n.layer + 1, n.midiNote, n.velocity), sampleOffset);

                // Trigger built-in preview synth
                if (previewEnabled) {
                    for (auto& v : voices) {
                        if (!v.active) {
                            v.active = true;
                            v.note = n.midiNote;
                            v.layer = n.layer;
                            v.phase = 0.0;
                            double freq = juce::MidiMessage::getMidiNoteInHertz(n.midiNote);
                            v.phaseInc = (2.0 * juce::MathConstants<double>::pi * freq) / currentSampleRate;
                            v.env = 1.0f;
                            v.velocity = n.velocity;
                            v.releaseBeat = ppq + n.lengthBeats;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Render lightweight preview audio
    if (previewEnabled) {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int s = 0; s < buffer.getNumSamples(); ++s) {
            double currentBeat = ppq + s * beatsPerSample;
            float mix = 0.0f;

            for (auto& v : voices) {
                if (!v.active) continue;
                float sample = 0.0f;

                if (v.layer == 0)      sample = static_cast<float>(std::sin(v.phase) * 0.7 + std::sin(v.phase * 2.0) * 0.3); // Lead pluck
                else if (v.layer == 1) sample = static_cast<float>(std::sin(v.phase)); // Soft EPiano
                else                   sample = static_cast<float>((v.phase / juce::MathConstants<double>::pi) - 1.0) * 0.4f; // Strings

                v.phase += v.phaseInc;
                if (v.phase >= 2.0 * juce::MathConstants<double>::pi) v.phase -= 2.0 * juce::MathConstants<double>::pi;

                float decay = (v.layer == 2) ? 0.99985f : 0.9996f;
                if (currentBeat >= v.releaseBeat) decay = 0.995f;
                v.env *= decay;

                if (v.env < 0.001f) v.active = false;
                mix += sample * v.env * v.velocity * 0.14f;
            }

            left[s] = juce::jlimit(-0.9f, 0.9f, mix);
            if (right) right[s] = left[s];
        }
    }

    lastPpq = ppq;
}

juce::File MelodyzeAudioProcessor::exportMidiToTempFile(int layerFilter) {
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(480);
    juce::MidiMessageSequence seq;

    {
        const juce::ScopedLock sl(lock);
        for (const auto& n : notes) {
            if (layerFilter != -1 && n.layer != layerFilter) continue;
            int startTick = static_cast<int>(n.startBeat * 480.0);
            int endTick = static_cast<int>((n.startBeat + n.lengthBeats) * 480.0);
            seq.addEvent(juce::MidiMessage::noteOn(1, n.midiNote, n.velocity), startTick);
            seq.addEvent(juce::MidiMessage::noteOff(1, n.midiNote), endTick);
        }
    }

    seq.updateMatchedPairs();
    midiFile.addTrack(seq);

    juce::String name = (layerFilter == 0) ? "Melodyze_Lead.mid" :
                        (layerFilter == 1) ? "Melodyze_Piano.mid" :
                        (layerFilter == 2) ? "Melodyze_Strings.mid" : "Melodyze_All.mid";

    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
    tempFile.deleteFile();
    if (auto stream = tempFile.createOutputStream()) {
        midiFile.writeTo(*stream);
    }
    return tempFile;
}

void MelodyzeAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree state("MelodyzeState");
    state.setProperty("chords", chordText, nullptr);
    state.setProperty("style", currentStyle, nullptr);
    state.setProperty("density", density, nullptr);
    state.setProperty("humanize", humanize, nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void MelodyzeAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (state.isValid()) {
        chordText = state.getProperty("chords", "Am F C G").toString();
        currentStyle = state.getProperty("style", 0);
        density = state.getProperty("density", 0.65f);
        humanize = state.getProperty("humanize", 0.25f);
        generatePatterns();
    }
}

juce::AudioProcessorEditor* MelodyzeAudioProcessor::createEditor() {
    return new MelodyzeAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MelodyzeAudioProcessor();
}
