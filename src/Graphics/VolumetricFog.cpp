/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the volumetric fog

#include "VolumetricFog.h"

#include <cmath>

#include <algorithm>

using namespace openblack::graphics;
using namespace openblack::graphics::volumetric_fog;

namespace
{
float Smoothstep(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
} // namespace

float volumetric_fog::SliceDistance(float slice)
{
	return k_Nearest * std::pow(k_Farthest / k_Nearest, (slice + 0.5f) / static_cast<float>(k_Slices));
}

float volumetric_fog::SliceAt(float distance)
{
	return std::log(std::max(distance, 1e-3f) / k_Nearest) / std::log(k_Farthest / k_Nearest) * static_cast<float>(k_Slices) -
	       0.5f;
}

float volumetric_fog::DawnFog(float hour)
{
	const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
	// It builds through the night, is thickest round sunrise, and burns off through the morning
	const float night = 0.35f * Smoothstep(21.0f, 24.0f, h) + 0.35f * (1.0f - Smoothstep(0.0f, 0.5f, h));
	const float dawn = Smoothstep(0.0f, 4.5f, h) * (1.0f - Smoothstep(7.0f, 10.5f, h));
	return std::clamp(std::max(night, dawn * (0.35f + 0.65f * Smoothstep(2.0f, 5.0f, h))), 0.0f, 1.0f);
}

float volumetric_fog::FogTop(float hour)
{
	const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
	// Low and flat before sunrise; it lifts through the morning as it thins
	return 14.0f + 46.0f * Smoothstep(6.0f, 10.5f, h);
}

float volumetric_fog::StepWetness(float wetness, float rain, float seconds)
{
	if (seconds <= 0.0f)
	{
		return wetness;
	}
	if (rain > wetness)
	{
		return wetness + (rain - wetness) * (1.0f - std::exp(-seconds / 20.0f));
	}
	return wetness + (std::clamp(rain, 0.0f, 1.0f) - wetness) * (1.0f - std::exp(-seconds / 400.0f));
}

FogState volumetric_fog::FogFor(float hour, float wetness, float storm)
{
	const float calm = 1.0f - std::clamp(storm, 0.0f, 1.0f);
	FogState fog;
	// A thin haze always, denser in a storm's murk
	fog.hazeDensity = 0.00012f + 0.0009f * std::clamp(storm, 0.0f, 1.0f);
	fog.hazeHeight = 160.0f + 260.0f * std::clamp(storm, 0.0f, 1.0f);
	// The ground fog: dawn's and the wet land's, none in a storm
	fog.groundDensity = 0.016f * DawnFog(hour) * (1.0f + 1.5f * std::clamp(wetness, 0.0f, 1.0f)) * calm;
	fog.groundTop = FogTop(hour);
	return fog;
}
