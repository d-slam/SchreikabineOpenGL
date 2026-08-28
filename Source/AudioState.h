/*
  ==============================================================================

	AudioState.h
	Created: 19 Jun 2026 10:21:45am
	Author:  SchoeDam

  ==============================================================================
*/

#pragma once

#include <atomic>

struct AudioState
{
	
	std::atomic<double> currentSampleRate{ 48000 };


	// scope comp
	std::atomic<float> fftSmooth{ 0.8f };

	std::atomic<float> displaySmooth{ 0.15f };

	std::atomic<float> dbMin{ -100 };		//sollte int sein...
	std::atomic<float> scopeNormFactor{ 1.68f };
	std::atomic<bool> scopeAutoNormalize{ true };
	std::atomic<float> scopeRenderScale{ 1.0f };


	// spec comp


	std::atomic<int> spec_dBVisible{ -65 };
	std::atomic<int> spec_dBFloor{ -80 };


	//filter

	// filter cutoff (high-pass)
	std::atomic<double> hp_cutoff{ 60 };

	// particle parameters for ScopeComponent (modifiable via UI)
	std::atomic<float> particleGravity{ 716 };
	std::atomic<float> particleInitVy{ 130.0f };
	std::atomic<float> particleFadeRate{ 3.46f };
	std::atomic<float> particleRadius{ 0.1f };
	std::atomic<int> particleSpawnStep{ 10 };
	std::atomic<int> particleMaxCount{ 50000 };
	std::atomic<bool> particleVisibilityTestMode{ false };
	std::atomic<float> meshBackFade{ 0.92f };

	// global gain for display / processing (in dB)
	std::atomic<float> gain_dB{ 60.0f };
	// separate glow multiplier for spectrum rendering (0.0 .. 3.0)
	std::atomic<float> glow{ 0.75f };
	// scales how strongly signal magnitude affects glow (0..1)
	std::atomic<float> glowAmount{ 0.91f };




		

};