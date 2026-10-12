/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StormClouds.h"

#include <cmath>

#include <algorithm>

using namespace openblack;
using namespace openblack::storm_clouds;

namespace
{
/// Each step covers this much of the way to the target
constexpr float k_StepShare = 1.0f / static_cast<float>(k_StepsPerLeg);
/// Clouds spread over half the storm's reach either side of its middle
constexpr float k_HalfReach = 0.5f;
/// The cloud's opacity at full strength, of 255
constexpr double k_OpacityAtFullStrength = 0.75;
/// The storm's shadow: its shade plus this, times its strength
constexpr float k_ShadowShadeBase = 0.7f;
constexpr float k_FaintestShadow = 0.01f;
/// Metres to a cell of the land
constexpr float k_CellSize = 10.0f;

/// A whole number cut towards nought, kept to its lowest byte as the game's bytes keep it
uint8_t LowByte(double value) noexcept
{
	return static_cast<uint8_t>(static_cast<int32_t>(value));
}
} // namespace

int storm_clouds::Wanted(int32_t cloudCount) noexcept
{
	return std::clamp(cloudCount, 0, k_MostClouds);
}

NewCloud storm_clouds::MakeCloud(const Random& random, float cloudHeight)
{
	// In this order, as the game draws its numbers
	NewCloud cloud {};
	cloud.edgeShrink = random(2.5f, 5.0f);
	const float size = random(0.01f, 0.015f);
	cloud.sizeFactor = size + size;
	cloud.drift.place.x = random(-1.0f, 1.0f);
	cloud.drift.place.y = random(-10.0f, 10.0f) + cloudHeight;
	cloud.drift.place.z = random(-1.0f, 1.0f);
	cloud.drift.stepsLeft = 0;
	return cloud;
}

void storm_clouds::Step(Drift& drift, const Random& random, float cloudHeight)
{
	if (drift.stepsLeft == 0)
	{
		// It stays put this step and sets off on the next
		drift.target.x = random(-1.0f, 1.0f);
		drift.target.y = random(0.0f, 20.0f) + cloudHeight;
		drift.target.z = random(-1.0f, 1.0f);
		drift.step = (drift.target - drift.place) * k_StepShare;
		drift.stepsLeft = k_StepsPerLeg;
		return;
	}
	--drift.stepsLeft;
	drift.place += drift.step;
}

Placement storm_clouds::Place(const Drift& drift, float sizeFactor, const glm::vec3& stormCentre, float innerRadius,
                              float outerRadius)
{
	const double reach = static_cast<double>(outerRadius) + static_cast<double>(innerRadius);
	const auto across = [reach](float fraction, float middle) {
		return static_cast<float>((reach * fraction * k_HalfReach) + middle);
	};
	return {
	    .xz = {across(drift.place.x, stormCentre.x), across(drift.place.z, stormCentre.z)},
	    .heightAboveGround = drift.place.y,
	    .size = static_cast<float>(reach * sizeFactor),
	};
}

uint8_t storm_clouds::Alpha(float strength) noexcept
{
	// Three quarters of the land colour's own full opacity
	constexpr double k_FullOpacity = 255.0;
	return LowByte(static_cast<double>(strength) * k_OpacityAtFullStrength * k_FullOpacity);
}

uint32_t storm_clouds::Tint(uint32_t landColour, float shade) noexcept
{
	uint32_t colour = landColour & 0xFFFFFFu;
	if (!(shade > 0.0f))
	{
		return colour;
	}
	const auto factor = static_cast<double>(1.0f - (shade * 0.5f));
	colour = 0;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		const auto channel = static_cast<double>((landColour >> shift) & 0xFFu);
		colour |= static_cast<uint32_t>(LowByte(channel * factor)) << shift;
	}
	return colour;
}

uint8_t storm_clouds::FlashGrey(float glow) noexcept
{
	constexpr double k_Scale = 127.0;
	// Cut to a whole number, at most 255 (a negative one counts as more)
	const auto grey = static_cast<uint32_t>(static_cast<int32_t>(static_cast<double>(glow) * k_Scale));
	return static_cast<uint8_t>(std::min(grey, 255u));
}

std::optional<uint8_t> storm_clouds::ShadowAlpha(float shade, float strength) noexcept
{
	const double amount = (static_cast<double>(shade) + static_cast<double>(k_ShadowShadeBase)) * static_cast<double>(strength);
	float kept = 1.0f;
	if (!(amount > 1.0))
	{
		if (!(amount > static_cast<double>(k_FaintestShadow)))
		{
			return std::nullopt;
		}
		kept = static_cast<float>(amount);
	}
	constexpr float k_Full = 255.0f;
	return static_cast<uint8_t>(static_cast<int32_t>(std::clamp(kept * k_Full, 0.0f, k_Full)));
}

glm::vec2 storm_clouds::ShadowCorner(glm::vec2 stormCentre) noexcept
{
	// Back by half its side, less half a cell
	const float back = static_cast<float>(k_ShadowCells - 1) * (k_CellSize * 0.5f);
	return stormCentre - back;
}

bool storm_clouds::StepDue(std::chrono::duration<float, std::milli>& sinceStep,
                           std::chrono::duration<float, std::milli> frameTime) noexcept
{
	sinceStep += frameTime;
	if (sinceStep < k_StepInterval)
	{
		return false;
	}
	// One step a frame at most: a long frame leaves no steps owed, only its share of the next
	sinceStep =
	    std::chrono::duration<float, std::milli>(std::fmod((sinceStep - k_StepInterval).count(), k_StepInterval.count()));
	return true;
}
