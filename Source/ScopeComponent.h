/*
  ==============================================================================

	ScopeComponent.h
	Created: 19 Jun 2026 9:38:18am
	Author:  SchoeDam

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "AudioState.h"


class ScopeComponent : public juce::Component, private juce::Timer
{
public:
	ScopeComponent(AudioState& state) : audioState(state), forwardFFT(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann)
	{
		juce::zeromem(fifo, sizeof(fifo));
		juce::zeromem(fftData, sizeof(fftData));

		rebuildFFTLookup();

		setOpaque(true);
		setBufferedToImage(true); // use component buffering

		// higher update rate for smoother rendering
		startTimerHz(120);

		// create offscreen image for double-buffered drawing
		if (getWidth() > 0 && getHeight() > 0)
			spectrumImage = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);



	}

	~ScopeComponent()
	{
	}

	// wichter optimisierungscheis!!! mit steigender fftOrder brauchen wir lange bis fifo voll ist, deswegen öffters rendern mit /8 für 14te zB
	void pushNextSampleIntoFifo(float sample) noexcept
	{
		if (fifoIndex == fftSize / 16)
		{
			if (!nextFFTBlockReady)
			{
				juce::zeromem(fftData, sizeof(fftData));
				memcpy(fftData, fifo, sizeof(fifo));
				nextFFTBlockReady = true;
			}

			fifoIndex = 0;
		}
		fifo[fifoIndex++] = sample;
	}

	void rebuildFFTLookup()
	{
		float nyquist = audioState.currentSampleRate.load() * 0.5f;

		constexpr float minFreq = 20.0f;

		const auto numPoints = fftLookup.size();

		if (numPoints == 0)	return;

		for (size_t i = 0; i < numPoints; ++i)
		{
			float proportion = static_cast<float>(i) / static_cast<float>(numPoints - 1);

			float freq = minFreq * std::pow(nyquist / minFreq, proportion);

			float bin = freq * fftSize / audioState.currentSampleRate.load();

			int bin0 = static_cast<int>(bin);
			int bin1 = juce::jmin(bin0 + 1, fftSize / 2);

			fftLookup[i] = { bin0,	bin1,	bin - static_cast<float>(bin0) };
		}
	}

private:

	static constexpr int fftOrder = 14;		//def 11
	static constexpr int fftSize = 1 << fftOrder;
	//static constexpr int scopeSize = 512;	//def 512	Obsolete da mir die size aus der component getWidth() ableiten


	juce::dsp::FFT forwardFFT;
	juce::dsp::WindowingFunction<float> window;

	float fifo[fftSize];

	float fftData[2 * fftSize];

	//float scopeData[scopeSize];
	std::vector<float> scopeData;

	//float fftSmoothed[scopeSize];
	std::vector<float> fftSmoothed;

	int fifoIndex = 0;
	bool nextFFTBlockReady = false;

	//struct FFTLookup { int index0;		int index1;		float frac; };
	struct FFTLookup { int bin0;		int bin1;		float interp; };
	std::vector<FFTLookup> fftLookup;

	AudioState& audioState;

public:
	// Offscreen image and path for double-buffered drawing
	juce::Image spectrumImage;
	juce::Path spectrumPath;
	std::function<void(const juce::Image&)> onRenderImageChanged;
	std::function<void(const std::vector<float>&)> onSpectrumDataChanged;
	// protects concurrent access to spectrumImage
	juce::CriticalSection spectrumImageLock;
	float autoNormPeakSmoothed = 1.0f;



private:


	////////////////////////////////////////////////////////	


	float frequencyToX(float freq, float width)
	{
		constexpr float minFreq = 20.0f;			//def 20.0f
		constexpr float maxFreq = 20000.0f;			//def 20000.0f

		auto norm = (std::log10(freq) - std::log10(minFreq)) / (std::log10(maxFreq) - std::log10(minFreq));

		return norm * width;
	}

	void updateRenderResolution()
	{
		const float scale = juce::jlimit(0.1f, 1.0f, audioState.scopeRenderScale.load());
		const int desiredPoints = juce::jmax(1, (int)std::round((float)getWidth() * scale));

		if ((int)scopeData.size() != desiredPoints)
		{
			scopeData.resize((size_t)desiredPoints);
			fftSmoothed.resize((size_t)desiredPoints);
			fftLookup.resize((size_t)desiredPoints);
			rebuildFFTLookup();
		}
	}



	void drawNextFrameOfSpectrum()
	{
		updateRenderResolution();

		window.multiplyWithWindowingTable(fftData, fftSize);

		forwardFFT.performFrequencyOnlyForwardTransform(fftData);
		float normScale = juce::jmax(0.01f, audioState.scopeNormFactor.load());
		float normFactor = (float)fftSize * normScale;
		const bool autoNormalize = audioState.scopeAutoNormalize.load();
		float framePeak = 1.0e-6f;
		float frameMagnitude = 0.0f;

		// compute magnitudes and apply smoothing
		for (size_t i = 0; i < scopeData.size(); ++i)
		{
			const auto& l = fftLookup[i];

			float fftValue = fftData[l.bin0] + l.interp * (fftData[l.bin1] - fftData[l.bin0]);

			fftSmoothed[i] = fftSmoothed[i] * audioState.fftSmooth.load() + fftValue * (1.0f - audioState.fftSmooth.load());

			if (autoNormalize)
				framePeak = juce::jmax(framePeak, fftSmoothed[i]);
		}

		if (autoNormalize)
		{
			constexpr float attack = 0.25f;
			constexpr float release = 0.05f;
			autoNormPeakSmoothed += (framePeak > autoNormPeakSmoothed ? attack : release) * (framePeak - autoNormPeakSmoothed);
			normFactor = juce::jmax(1.0e-4f, autoNormPeakSmoothed * normScale);
		}

		for (size_t i = 0; i < scopeData.size(); ++i)
		{
			float level = fftSmoothed[i] / normFactor;
			level = juce::jlimit(0.0f, 1.0f, level);

			frameMagnitude = juce::jmax(frameMagnitude, level);

			scopeData[i] = scopeData[i] + audioState.displaySmooth.load() * (level - scopeData[i]);
		}

		// Rendering is handled by OpenGLScopeView; keep this component focused on analysis.
		return;

		// Legacy offscreen renderer retained below for reference.		// check mol ob mor des no brauchn...
		{
			juce::ScopedLock lock(spectrumImageLock);
			if (spectrumImage.isNull() || spectrumImage.getWidth() != getWidth() || spectrumImage.getHeight() != getHeight())
			{
				if (getWidth() > 0 && getHeight() > 0)
					spectrumImage = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
			}

			if (!spectrumImage.isNull())
			{
				juce::Graphics gi(spectrumImage);
				gi.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
				gi.fillAll(juce::Colours::black);

				spectrumPath.clear();
				const float firstY = juce::jmap<float>(scopeData.empty() ? 0.0f : scopeData[0], 0.0f, 1.0f, (float)getHeight(), 0.0f);
				spectrumPath.startNewSubPath(0.0f, firstY);

				for (size_t i = 1; i < scopeData.size(); ++i)
				{
					auto x = juce::jmap<float>(
						static_cast<float>(i),
						0.0f,
						static_cast<float>(scopeData.size() - 1),
						0.0f,
						(float)getWidth());

					auto y = juce::jmap<float>(
						scopeData[i],
						0.0f,
						1.0f,
						(float)getHeight(),
						0.0f);

					// ensure the rightmost (last) point of the path stays on the x-axis (bottom)
					if (i == scopeData.size() - 1)
						y = (float)getHeight();
					spectrumPath.lineTo(x, y);

				}

				gi.setColour(juce::Colours::lime.withAlpha(0.2f));
				juce::Path spectrumFillPath(spectrumPath);
				spectrumFillPath.lineTo(0.0f, (float)getHeight());
				spectrumFillPath.closeSubPath();
				gi.fillPath(spectrumFillPath);


				// bloom/glow scaled by UI gain
				// use dedicated glow control if provided
				float glow = audioState.glow.load();
				float glowAmount = juce::jlimit(0.0f, 1.0f, audioState.glowAmount.load());
				float glowScale = juce::jmap(glowAmount, 1.0f, juce::jlimit(0.0f, 1.0f, frameMagnitude));
				glow *= glowScale;

				// subtle layered strokes for glow; clamp alpha to 1.0
				float a1 = juce::jmin(1.0f, 0.125f * glow);
				float a2 = juce::jmin(1.0f, 0.112f * glow);
				gi.setColour(juce::Colours::lime.withAlpha(a1));
				gi.strokePath(spectrumPath, juce::PathStrokeType(12.0f * glow, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
				gi.setColour(juce::Colours::lime.withAlpha(a2));
				gi.strokePath(spectrumPath, juce::PathStrokeType(28.0f * glow, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

				// main stroke (stronger when gain is high)
				float mainAlpha = juce::jmin(1.0f, 0.95f * glow);
				gi.setColour(juce::Colours::lime.withAlpha(mainAlpha));
				gi.strokePath(spectrumPath, juce::PathStrokeType(1.5f * juce::jmax(1.0f, glow * 0.5f)));


			}

		}
	}

	void timerCallback() override
	{
		if (nextFFTBlockReady)
		{
			drawNextFrameOfSpectrum();
			nextFFTBlockReady = false;
			if (onRenderImageChanged)
				onRenderImageChanged(spectrumImage);
			if (onSpectrumDataChanged)
				onSpectrumDataChanged(scopeData);
			repaint();


		}
	}


	void paint(juce::Graphics& g) override
	{
		g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

		auto bounds = getLocalBounds().toFloat();

		// If we have an offscreen rendered image, blit it — much cheaper than re-drawing the path every frame
		if (!spectrumImage.isNull())
		{
			g.drawImageAt(spectrumImage, 0, 0);
		}
		else
		{
			g.fillAll(juce::Colours::black);
			g.setColour(juce::Colours::lime);

			auto bounds = getLocalBounds().toFloat();

			juce::Path tmpPath;
			const float firstY = juce::jmap<float>(scopeData.empty() ? 0.0f : scopeData[0], 0.0f, 1.0f, bounds.getBottom(), bounds.getY());
			tmpPath.startNewSubPath(0.0f, firstY);

			for (size_t i = 1; i < scopeData.size(); ++i)
			{
				auto x = juce::jmap<float>(
					static_cast<float>(i),
					0.0f,
					static_cast<float>(scopeData.size() - 1),
					0.0f,
					bounds.getWidth());

				auto y = juce::jmap<float>(
					scopeData[i],
					0.0f,
					1.0f,
					bounds.getBottom(),
					bounds.getY());

				tmpPath.lineTo(x, y);
			}

			g.setColour(juce::Colours::lime.withAlpha(0.2f));
			juce::Path tmpFillPath(tmpPath);
			tmpFillPath.lineTo(0.0f, bounds.getBottom());
			tmpFillPath.closeSubPath();
			g.fillPath(tmpFillPath);

			g.setColour(juce::Colours::lime.withAlpha(0.9f));
			g.strokePath(tmpPath, juce::PathStrokeType(1.5f));
		}


		////soft glow
		//g.setColour(juce::Colours::lime.withAlpha(0.08f));
		//g.strokePath(spectrumPath, juce::PathStrokeType(6.0f));

		//g.setColour(juce::Colours::lime.withAlpha(0.4f));
		//g.strokePath(spectrumPath, juce::PathStrokeType(3.0f));

		g.setColour(juce::Colours::lime.withAlpha(1.0f));
		g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));

		//draw frequ achse

		//g.setColour(juce::Colours::grey);

		//std::array<float, 10> freqs =
		//{
		//	20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
		//	1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
		//};

		//for (auto freq : freqs)
		//{
		//	auto x = frequencyToX(freq, bounds.getWidth());

		//	// Tick
		//	g.drawVerticalLine((int)x, (int)bounds.getBottom() - 10, (int)bounds.getBottom());

		//	// FabFilter-style dashed guide line
		//	g.setColour(juce::Colours::grey.withAlpha(0.28f));
		//	const float dashLengths[] = { 3.0f, 3.0f };
		//	g.drawDashedLine(juce::Line<float>(x, bounds.getY() + 6.0f, x, bounds.getBottom() - 12.0f), dashLengths, 2, 1.0f);
		//	g.setColour(juce::Colours::grey);

		//	// Beschriftung
		//	juce::String label;

		//	if (freq >= 1000.0f)	label = juce::String(freq / 1000.0f, 0) + "k";
		//	else					label = juce::String((int)freq);

		//	g.drawText(label,
		//		(int)x - 20,
		//		(int)bounds.getBottom() - 20,
		//		40,
		//		15,
		//		juce::Justification::centred);
		//}

		// simple dB hint: only label + arrow at top-left
		g.setColour(juce::Colours::grey.withAlpha(0.8f));
		g.drawText("dB",
			(int)bounds.getX() + 4,
			(int)bounds.getY() + 32,
			24,
			14,
			juce::Justification::left);

		const float arrowX = bounds.getX() + 12.0f;
		juce::Path dbArrow;
		dbArrow.startNewSubPath(arrowX, bounds.getY() + 28.0f);
		dbArrow.lineTo(arrowX, bounds.getY() + 10.0f);
		dbArrow.lineTo(arrowX - 3.5f, bounds.getY() + 14.0f);
		dbArrow.startNewSubPath(arrowX, bounds.getY() + 10.0f);
		dbArrow.lineTo(arrowX + 3.5f, bounds.getY() + 14.0f);
		g.strokePath(dbArrow, juce::PathStrokeType(1.0f));

	}

	void resized() override
	{
		updateRenderResolution();


	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeComponent)
};