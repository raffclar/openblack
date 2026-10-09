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
/// The game's milliseconds-to-seconds factor, a float
constexpr double k_SecondsPerMillisecond = static_cast<double>(0.001f);
/// A map cell's side, and its inverse as the game multiplies by it
constexpr float k_CellSize = 10.0f;
constexpr double k_CellsPerUnit = static_cast<double>(0.1f);
/// Where the glow settles while the vortex is open
constexpr double k_OpenGlow = static_cast<double>(0.6f);
/// The glow's last fade, after the vortex has shut
constexpr double k_GlowDieSeconds = 2.0;
/// The swirl under the land: its depth before the levelling and once it is done
constexpr double k_SwirlStartDepth = 2.5;
constexpr double k_SwirlEndDepth = static_cast<double>(0.3f);

/// Eased in and out: 0 at 0, 1 at 1, flat at both ends
double SmoothStep(double x)
{
	return (3.0 - (x + x)) * x * x;
}
} // namespace

double ElapsedSeconds(uint32_t turnsSince, float turnFraction, uint32_t millisecondsPerTurn)
{
	return (static_cast<double>(turnsSince) + static_cast<double>(turnFraction)) * static_cast<double>(millisecondsPerTurn) *
	       k_SecondsPerMillisecond;
}

float Openness(VortexStateType state, float seconds)
{
	const auto t = static_cast<double>(seconds);
	switch (state)
	{
	case VortexStateType::Active:
		return 1.0f;
	case VortexStateType::FadeIn:
		if (static_cast<double>(k_FadeDelaySeconds) <= t)
		{
			const auto x = std::clamp((t - k_FadeDelaySeconds) / k_FadeSeconds, 0.0, 1.0);
			return static_cast<float>(SmoothStep(x));
		}
		return 0.0f;
	case VortexStateType::FadeOut:
		if (t < static_cast<double>(k_FadeSeconds))
		{
			return static_cast<float>(SmoothStep(1.0 - t / k_FadeSeconds));
		}
		return 0.0f;
	case VortexStateType::Inactive:
	default:
		return 0.0f;
	}
}

float LevelAmount(VortexStateType state, float seconds)
{
	const auto open = state == VortexStateType::FadeOut ? 1.0 : static_cast<double>(Openness(state, seconds));
	return static_cast<float>(1.0 - (1.0 - open) * (1.0 - open));
}

Step Advance(VortexStateType state, double seconds)
{
	if (!(static_cast<double>(k_FadeDelaySeconds) + static_cast<double>(k_FadeSeconds) < seconds))
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
	const auto t = static_cast<double>(seconds);
	switch (state)
	{
	case VortexStateType::Active:
		return static_cast<float>(k_OpenGlow);
	case VortexStateType::FadeIn:
		if (t < static_cast<double>(k_FadeDelaySeconds))
		{
			return static_cast<float>(t / k_FadeDelaySeconds);
		}
		return static_cast<float>((k_OpenGlow - 1.0) * std::clamp((t - k_FadeDelaySeconds) / k_FadeSeconds, 0.0, 1.0) + 1.0);
	case VortexStateType::FadeOut:
		if (t < static_cast<double>(k_FadeSeconds))
		{
			return static_cast<float>(t / k_FadeSeconds * (1.0 - k_OpenGlow) + k_OpenGlow);
		}
		return static_cast<float>(1.0 - std::clamp((t - k_FadeSeconds) / k_GlowDieSeconds, 0.0, 1.0));
	case VortexStateType::Inactive:
	default:
		return 0.0f;
	}
}

uint8_t GroundHoleThreshold(float openness)
{
	const auto alpha = static_cast<uint32_t>(static_cast<int64_t>(static_cast<double>(openness) * 255.0));
	return static_cast<uint8_t>(std::min<uint32_t>(alpha, k_GroundHoleMaxThreshold));
}

glm::vec2 GroundTextureCoordinates(glm::vec2 point, glm::vec2 centre, float baseScale)
{
	const auto offset = (point - centre) / (k_GroundTextureSpan * baseScale);
	return glm::vec2(offset.y, offset.x) + 0.5f;
}

float SwirlDepth(float levelAmount)
{
	return static_cast<float>((k_SwirlEndDepth - k_SwirlStartDepth) * static_cast<double>(levelAmount) + k_SwirlStartDepth);
}

glm::ivec2 CentreCell(glm::vec2 centre)
{
	return {static_cast<int>(static_cast<double>(centre.x) * k_CellsPerUnit),
	        static_cast<int>(static_cast<double>(centre.y) * k_CellsPerUnit)};
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
	const auto d = static_cast<double>(distance);
	if (d > static_cast<double>(k_LevelOuterRadius))
	{
		return 0.0f;
	}
	if (d < static_cast<double>(k_LevelFullRadius))
	{
		return 1.0f;
	}
	return static_cast<float>((k_LevelOuterRadius - d) / (k_LevelOuterRadius - k_LevelFullRadius));
}

uint8_t LevelledAltitude(uint8_t original, float average, float distance, float amount)
{
	// The pull towards the average, whole within the inner radius and falling away in a straight line to the outer. The
	// game also adds a height profile by distance, but every point of it is 0, so it never changes the land.
	const auto d = static_cast<double>(distance);
	const auto toAverage = static_cast<double>(average) - static_cast<double>(original);
	double pull = 0.0;
	if (d <= static_cast<double>(k_LevelOuterRadius))
	{
		pull = static_cast<double>(k_LevelFullRadius) <= d
		           ? toAverage * (k_LevelOuterRadius - d) / (k_LevelOuterRadius - k_LevelFullRadius)
		           : toAverage;
	}
	const auto height = static_cast<float>(pull * static_cast<double>(amount) + static_cast<double>(original));
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
		const auto dx = static_cast<float>(cell.x) * k_CellSize - centre.x;
		const auto dz = static_cast<float>(cell.y) * k_CellSize - centre.y;
		const auto distance = static_cast<float>(std::sqrt(static_cast<double>(dz) * dz + static_cast<double>(dx) * dx));
		heights.at(i) = LevelledAltitude(originals[i], average, distance, amount);
	}
	return heights;
}

} // namespace openblack::vortex
