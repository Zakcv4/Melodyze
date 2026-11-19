#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MidiDragButton : public juce::TextButton {
public:
    MidiDragButton(const juce::String& text, MelodyzeAudioProcessor& p, int layer)
        : juce::TextButton(text), processor(p), layerFilter(layer) {}

    void mouseDrag(const juce::MouseEvent& e) override {
        if (e.getDistanceFromDragStart() > 4 && !dragging) {
            dragging = true;
            auto file = processor.exportMidiToTempFile(layerFilter);
            juce::DragAndDropContainer::performExternalDragDropOfFiles({ file.getFullPathName() }, false, this);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override {
        dragging = false;
        juce::TextButton::mouseUp(e);
    }

private:
    MelodyzeAudioProcessor& processor;
    int layerFilter;
    bool dragging = false;
};

class MelodyzeAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit MelodyzeAudioProcessorEditor(MelodyzeAudioProcessor& p)
        : AudioProcessorEditor(&p), processor(p),
          dragLead("Drag Lead MIDI", p, 0),
          dragPiano("Drag Piano MIDI", p, 1),
          dragStrings("Drag Strings MIDI", p, 2),
          dragAll("Drag All MIDI", p, -1) {

        setSize(520, 340);

        chordInput.setText(processor.getChordProgression());
        chordInput.onReturnKey = [this] { processor.setChordProgression(chordInput.getText()); };
        chordInput.onFocusLost = [this] { processor.setChordProgression(chordInput.getText()); };
        addAndMakeVisible(chordInput);

        styleBox.addItemList({ "Trap Pluck", "Emotional Piano", "Cinematic Strings", "Lo-Fi Bounce" }, 1);
        styleBox.setSelectedId(processor.getStyle() + 1);
        styleBox.onChange = [this] { processor.setStyle(styleBox.getSelectedId() - 1); };
        addAndMakeVisible(styleBox);

        densitySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        densitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        densitySlider.setRange(0.15, 1.0, 0.01);
        densitySlider.setValue(processor.getDensity());
        densitySlider.onValueChange = [this] { processor.setDensity(static_cast<float>(densitySlider.getValue())); };
        addAndMakeVisible(densitySlider);

        humanizeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        humanizeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        humanizeSlider.setRange(0.0, 1.0, 0.01);
        humanizeSlider.setValue(processor.getHumanize());
        humanizeSlider.onValueChange = [this] { processor.setHumanize(static_cast<float>(humanizeSlider.getValue())); };
        addAndMakeVisible(humanizeSlider);

        regenButton.setButtonText("Regenerate (Dice)");
        regenButton.onClick = [this] {
            processor.setChordProgression(chordInput.getText());
            processor.regenerate();
        };
        addAndMakeVisible(regenButton);

        previewToggle.setButtonText("Preview Audio");
        previewToggle.setToggleState(processor.isPreviewEnabled(), juce::dontSendNotification);
        previewToggle.onClick = [this] { processor.setPreviewEnabled(previewToggle.getToggleState()); };
        addAndMakeVisible(previewToggle);

        addAndMakeVisible(dragLead);
        addAndMakeVisible(dragPiano);
        addAndMakeVisible(dragStrings);
        addAndMakeVisible(dragAll);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff181a20));
        g.setColour(juce::Colours::white);
        g.setFont(22.0f);
        g.drawText("MELODYZE v1", 20, 12, 200, 30, juce::Justification::left);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(13.0f);
        g.drawText("Chords (e.g. Am F C G):", 20, 52, 200, 20, juce::Justification::left);
        g.drawText("Style:", 310, 52, 100, 20, juce::Justification::left);
        g.drawText("Density", 45, 120, 100, 20, juce::Justification::centred);
        g.drawText("Humanize", 185, 120, 100, 20, juce::Justification::centred);
        g.drawText("Click & Drag into FL Studio:", 20, 255, 250, 20, juce::Justification::left);
    }

    void resized() override {
        chordInput.setBounds(20, 74, 270, 30);
        styleBox.setBounds(310, 74, 190, 30);

        densitySlider.setBounds(45, 140, 100, 95);
        humanizeSlider.setBounds(185, 140, 100, 95);

        regenButton.setBounds(320, 150, 180, 38);
        previewToggle.setBounds(320, 200, 180, 28);

        dragLead.setBounds(20, 280, 115, 36);
        dragPiano.setBounds(142, 280, 115, 36);
        dragStrings.setBounds(264, 280, 115, 36);
        dragAll.setBounds(386, 280, 114, 36);
    }

private:
    MelodyzeAudioProcessor& processor;
    juce::TextEditor chordInput;
    juce::ComboBox styleBox;
    juce::Slider densitySlider, humanizeSlider;
    juce::TextButton regenButton;
    juce::ToggleButton previewToggle;
    MidiDragButton dragLead, dragPiano, dragStrings, dragAll;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MelodyzeAudioProcessorEditor)
};
