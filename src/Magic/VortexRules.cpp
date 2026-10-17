/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexRules.h"

#include <cmath>

#include <algorithm>
#include <numeric>

namespace openblack::vortex
{
namespace
{
/// The game's milliseconds-to-seconds factor
constexpr float k_SecondsPerMillisecond = 0.001f;
/// A map cell's side, and its inverse as the game multiplies by it
constexpr float k_CellSize = 10.0f;
constexpr float k_CellsPerUnit = 0.1f;
/// Where the glow settles while the vortex is open
constexpr float k_OpenGlow = 0.6f;
/// The glow's last fade, after the vortex has shut
constexpr float k_GlowDieSeconds = 2.0f;
/// The swirl under the land: its depth before the levelling and once it is done
constexpr float k_SwirlStartDepth = 2.5f;
constexpr float k_SwirlEndDepth = 0.3f;

/// Eased in and out: 0 at 0, 1 at 1, flat at both ends
float SmoothStep(float x)
{
	return (3.0f - (x + x)) * x * x;
}
} // namespace

float ElapsedSeconds(uint32_t turnsSince, float turnFraction, uint32_t millisecondsPerTurn)
{
	return (static_cast<float>(turnsSince) + turnFraction) * static_cast<float>(millisecondsPerTurn) * k_SecondsPerMillisecond;
}

float Openness(VortexStateType state, float seconds)
{
	switch (state)
	{
	case VortexStateType::Active:
		return 1.0f;
	case VortexStateType::FadeIn:
		if (k_FadeDelaySeconds <= seconds)
		{
			return SmoothStep(std::clamp((seconds - k_FadeDelaySeconds) / k_FadeSeconds, 0.0f, 1.0f));
		}
		return 0.0f;
	case VortexStateType::FadeOut:
		if (seconds < k_FadeSeconds)
		{
			return SmoothStep(1.0f - (seconds / k_FadeSeconds));
		}
		return 0.0f;
	case VortexStateType::Inactive:
	default:
		return 0.0f;
	}
}

float LevelAmount(VortexStateType state, float seconds)
{
	const float open = state == VortexStateType::FadeOut ? 1.0f : Openness(state, seconds);
	const float left = 1.0f - open;
	return 1.0f - (left * left);
}

Step Advance(VortexStateType state, float seconds)
{
	if (!(k_FadeDelaySeconds + k_FadeSeconds < seconds))
	{
		return Step::Stay;
	}
	switch (state)
	{
	case VortexStateType::FadeIn:
		return Step::Open;
	case VortexStateType::FadeOut:
		return Step::Remove;
	default:
		return Step::Stay;
	}
}

float GlowBrightness(VortexStateType state, float seconds)
{
	switch (state)
	{
	case VortexStateType::Active:
		return k_OpenGlow;
	case VortexStateType::FadeIn:
		if (seconds < k_FadeDelaySeconds)
		{
			return seconds / k_FadeDelaySeconds;
		}
		return ((k_OpenGlow - 1.0f) * std::clamp((seconds - k_FadeDelaySeconds) / k_FadeSeconds, 0.0f, 1.0f)) + 1.0f;
	case VortexStateType::FadeOut:
		if (seconds < k_FadeSeconds)
		{
			return (seconds / k_FadeSeconds * (1.0f - k_OpenGlow)) + k_OpenGlow;
		}
		return 1.0f - std::clamp((seconds - k_FadeSeconds) / k_GlowDieSeconds, 0.0f, 1.0f);
	case VortexStateType::Inactive:
	default:
		return 0.0f;
	}
}

uint8_t GroundHoleThreshold(float openness)
{
	const auto alpha = static_cast<uint32_t>(static_cast<int32_t>(openness * 255.0f));
	return static_cast<uint8_t>(std::min<uint32_t>(alpha, k_GroundHoleMaxThreshold));
}

glm::vec2 GroundTextureCoordinates(glm::vec2 point, glm::vec2 centre, float baseScale)
{
	const auto offset = (point - centre) / (k_GroundTextureSpan * baseScale);
	return glm::vec2(offset.y, offset.x) + 0.5f;
}

float SwirlDepth(float levelAmount)
{
	// The difference is a float: 0.3 less 2.5 is -2.2 as a float, a little less than the true difference
	return ((k_SwirlEndDepth - k_SwirlStartDepth) * levelAmount) + k_SwirlStartDepth;
}

glm::ivec2 CentreCell(glm::vec2 centre)
{
	return {static_cast<int>(centre.x * k_CellsPerUnit), static_cast<int>(centre.y * k_CellsPerUnit)};
}

glm::ivec2 SquareCell(glm::ivec2 centreCell, size_t index)
{
	const auto x = static_cast<int>(index / k_LevelSide);
	const auto z = static_cast<int>(index % k_LevelSide);
	return centreCell - glm::ivec2(k_LevelHalfSide) + glm::ivec2(x, z);
}

float AverageAltitude(std::span<const uint8_t> altitudes)
{
	if (altitudes.empty())
	{
		return 0.0f;
	}
	// Summed as the game sums them, in a float
	const auto sum = std::accumulate(altitudes.begin(), altitudes.end(), 0.0f,
	                                 [](float total, uint8_t altitude) { return total + static_cast<float>(altitude); });
	return sum / static_cast<float>(altitudes.size());
}

float LevelWeight(float distance)
{
	if (distance > k_LevelOuterRadius)
	{
		return 0.0f;
	}
	if (distance < k_LevelFullRadius)
	{
		return 1.0f;
	}
	return (k_LevelOuterRadius - distance) / (k_LevelOuterRadius - k_LevelFullRadius);
}

uint8_t LevelledAltitude(uint8_t original, float average, float distance, float amount)
{
	// The pull towards the average, whole within the inner radius and falling away in a straight line to the outer. The
	// game also adds a height profile by distance, but every point of it is 0, so it never changes the land.
	const auto altitude = static_cast<float>(original);
	float pull = 0.0f;
	if (distance <= k_LevelOuterRadius)
	{
		pull = k_LevelFullRadius <= distance
		           ? (k_LevelOuterRadius - distance) * (average - altitude) / (k_LevelOuterRadius - k_LevelFullRadius)
		           : average - altitude;
	}
	const float height = (pull * amount) + altitude;
	// Rounded to the nearest, halves to even, and kept to a cell's heights
	const auto rounded = std::lrint(height);
	return static_cast<uint8_t>(std::clamp<long>(rounded, 0, 255));
}

std::array<uint8_t, k_LevelCellCount> LevelSquare(glm::vec2 centre, std::span<const uint8_t> originals, float average,
                                                  float amount)
{
	std::array<uint8_t, k_LevelCellCount> heights {};
	const auto centreCell = CentreCell(centre);
	for (size_t i = 0; i < heights.size() && i < originals.size(); ++i)
	{
		const auto cell = SquareCell(centreCell, i);
		const auto dx = (static_cast<float>(cell.x) * k_CellSize) - centre.x;
		const auto dz = (static_cast<float>(cell.y) * k_CellSize) - centre.y;
		const auto distance = std::sqrt((dx * dx) + (dz * dz));
		heights.at(i) = LevelledAltitude(originals[i], average, distance, amount);
	}
	return heights;
}

} // namespace openblack::vortex
