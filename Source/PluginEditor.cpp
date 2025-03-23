/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChaoticSonicStitcherEditor::ChaoticSonicStitcherEditor (ChaoticSonicStitcherProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (800, 500);
}

ChaoticSonicStitcherEditor::~ChaoticSonicStitcherEditor()
{
}

//==============================================================================
void ChaoticSonicStitcherEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::blue);        // 1
    g.setColour (juce::Colours::white);
    g.setFont (40.0f);                      // 2
    g.drawFittedText ("My First Plug-in!",  // 3
        getLocalBounds(), juce::Justification::centred, 1);
}

void ChaoticSonicStitcherEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}
