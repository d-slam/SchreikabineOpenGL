/*
  ==============================================================================

	AudioGeraete.h
	Created: 23 Jun 2026 4:56:39pm
	Author:  Dami

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


class AudioGeraete : public juce::Component, public juce::ChangeListener
{
public:
	AudioGeraete()
	{
		setOpaque(true);

		juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, [this](bool granted)
			{
				int numInputChannels = granted ? 2 : 0;
				audioDeviceManager.initialise(numInputChannels, 2, nullptr, true, {}, nullptr);
			});


		audioSetupComp.reset(new juce::AudioDeviceSelectorComponent(audioDeviceManager, 1, 256, 0, 256, false, false, false, false));
		addAndMakeVisible(audioSetupComp.get());

		addAndMakeVisible(diagnosticsBox);
		diagnosticsBox.setMultiLine(true);
		diagnosticsBox.setReturnKeyStartsNewLine(true);
		diagnosticsBox.setReadOnly(true);
		diagnosticsBox.setScrollbarsShown(true);
		diagnosticsBox.setCaretVisible(false);
		diagnosticsBox.setPopupMenuEnabled(true);

		audioDeviceManager.addChangeListener(this);

		logMessage("Audio device diagnostics:\n");
		dumpDeviceInfo();

		setSize(500, 600);
	}

	~AudioGeraete()
	{
		audioDeviceManager.removeChangeListener(this);
	}

	void paint(juce::Graphics& g) override
	{
		g.fillAll(juce::Colours::black);
	}
	void resized() override
	{
		auto r = getLocalBounds().reduced(4);
		audioSetupComp->setBounds(r.removeFromTop(proportionOfHeight(0.65f)));
		diagnosticsBox.setBounds(r);
	}

	void dumpDeviceInfo()
	{
		logMessage("--------------------------------------");
		logMessage("Current audio device type: " + (audioDeviceManager.getCurrentDeviceTypeObject() != nullptr
			? audioDeviceManager.getCurrentDeviceTypeObject()->getTypeName()
			: "<none>"));

		if (juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
		{
			logMessage("Current audio device: " + device->getName().quoted());
			logMessage("Sample rate: " + juce::String(device->getCurrentSampleRate()) + " Hz");
			logMessage("Block size: " + juce::String(device->getCurrentBufferSizeSamples()) + " samples");
			logMessage("Output Latency: " + juce::String(device->getOutputLatencyInSamples()) + " samples");
			logMessage("Input Latency: " + juce::String(device->getInputLatencyInSamples()) + " samples");
			logMessage("Bit depth: " + juce::String(device->getCurrentBitDepth()));
			logMessage("Input channel names: " + device->getInputChannelNames().joinIntoString(", "));
			logMessage("Active input channels: " + getListOfActiveBits(device->getActiveInputChannels()));
			logMessage("Output channel names: " + device->getOutputChannelNames().joinIntoString(", "));
			logMessage("Active output channels: " + getListOfActiveBits(device->getActiveOutputChannels()));
		}
		else
		{
			logMessage("No audio device open");
		}
	}

	void logMessage(const juce::String& m)
	{
		diagnosticsBox.moveCaretToEnd();
		diagnosticsBox.insertTextAtCaret(m + juce::newLine);
	}

private:

	juce::AudioDeviceManager audioDeviceManager;
	std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSetupComp;
	juce::TextEditor diagnosticsBox;

	void changeListenerCallback(juce::ChangeBroadcaster*) override
	{
		dumpDeviceInfo();
	}

	void lookAndFeelChanged() override
	{
		diagnosticsBox.applyFontToAllText(diagnosticsBox.getFont());
	}

	static juce::String getListOfActiveBits(const juce::BigInteger& b)
	{
		juce::StringArray bits;

		for (int i = 0; i <= b.getHighestBit(); ++i)
			if (b[i])
				bits.add(juce::String(i));

		return bits.joinIntoString(", ");
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioGeraete)

};
