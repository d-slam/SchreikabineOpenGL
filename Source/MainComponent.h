#pragma once

#include <JuceHeader.h>
#include "AudioState.h"
#include "ScopeComponent.h"
#include "UIComponent.h"
#include "AudioGeraete.h"
#include "OpenGLScopeView.h"

//==============================================================================
/*
	This component lives inside our window, and this is where you should put all
	your controls and content.
*/
class MainComponent : public juce::AudioAppComponent
{
public:
	//==============================================================================
	MainComponent();
	~MainComponent() override;

	//==============================================================================
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
	void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
	void releaseResources() override;

	//==============================================================================
	void paint(juce::Graphics& g) override;
	void resized() override;
	bool keyPressed(const juce::KeyPress& key) override;
	void visibilityChanged() override;

private:

	double lastSampleRate;

	double currentSampleRate;

	AudioState audioState;

	std::unique_ptr<UIComponent> uiComponent;
	std::unique_ptr<ScopeComponent> scopeComponent;
	std::unique_ptr<AudioGeraete> audioGeraet;
	bool uiVisible { true };

	std::unique_ptr<OpenGLScopeView> openGLScopeView;

	// OpenGLScopeView openGLScopeView;


	using IIRStage = juce::dsp::IIR::Filter<float>;
	using IIRCoeffs = juce::dsp::IIR::Coefficients<float>;
	using StageDuplicator = juce::dsp::ProcessorDuplicator<IIRStage, IIRCoeffs>;

	juce::dsp::ProcessorChain<StageDuplicator, StageDuplicator> filter;

	// store last cutoff to update filter coefficients when parameter changes
	float lastLpFreq { 0.0f }; // no-op placeholder to keep edit history consistent

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
