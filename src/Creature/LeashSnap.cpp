/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashSnap.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::leash_snap;

namespace
{
/// The collar's rope is half as long again as the leash's
constexpr float k_CollarRopeScale = 1.5f;
/// Let go, the leash rests at half its length
constexpr float k_SlackShare = 0.5f;
/// The rope is never shorter than twice what it is tied to is round
constexpr float k_RadiusReach = 2.0f;
/// Holding the rope, the pinch is held halfway through its animation
constexpr float k_HeldShare = 0.5f;

bool Within(int value, int low, int high)
{
	return low <= value && value <= high;
}

/// Whether the cursor is inside the disc of a radius round a point, inside its square first
bool InDisc(glm::ivec2 cursor, glm::ivec2 centre, int radius)
{
	if (!Within(cursor.x, centre.x - radius, centre.x + radius) || !Within(cursor.y, centre.y - radius, centre.y + radius))
	{
		return false;
	}
	const auto offset = cursor - centre;
	return (offset.y * offset.y) + (offset.x * offset.x) < radius * radius;
}
} // namespace

int leash_snap::TouchPixels(float screenWidth, float halfWidth, float depth)
{
	// Whole pixels, cut towards zero
	const auto pixels = static_cast<int>(screenWidth * halfWidth * k_TouchWidthScale / depth);
	return std::max(pixels, k_MinTouchPixels);
}

std::optional<float> leash_snap::TouchAlong(glm::ivec2 cursor, glm::ivec2 a, int ra, glm::ivec2 b, int rb)
{
	// First the box round the segment, widened by the wider end
	const auto reach = std::max(rb, ra);
	if (!Within(cursor.x, std::min(a.x, b.x) - reach, std::max(a.x, b.x) + reach) ||
	    !Within(cursor.y, std::min(a.y, b.y) - reach, std::max(a.y, b.y) + reach))
	{
		return std::nullopt;
	}
	if (InDisc(cursor, a, ra))
	{
		return 0.0f;
	}
	if (InDisc(cursor, b, rb))
	{
		return 1.0f;
	}
	// Between the ends: the share is the cosine between the cursor's direction from a and the segment's, and the cursor's
	// distance from a, less that share of the segment's length, must be within the reach there (the game's own measure,
	// which is not the distance from the line)
	const auto toCursor = cursor - a;
	const auto along = b - a;
	const auto cursorLength = std::sqrt(static_cast<float>((toCursor.x * toCursor.x) + (toCursor.y * toCursor.y)));
	const auto segmentLength = std::sqrt(static_cast<float>((along.x * along.x) + (along.y * along.y)));
	if (segmentLength * cursorLength < k_MinSegmentSpan)
	{
		return std::nullopt;
	}
	const auto share = static_cast<float>((toCursor.x * along.x) + (toCursor.y * along.y)) / (segmentLength * cursorLength);
	if (share < 0.0f || share > 1.0f)
	{
		return std::nullopt;
	}
	const auto width = (static_cast<float>(rb - ra) * share) + static_cast<float>(ra);
	if ((cursorLength * cursorLength) - (share * share * segmentLength * segmentLength) > width * width)
	{
		return std::nullopt;
	}
	return share;
}

std::optional<float> leash_snap::TouchDepth(std::span<const ScreenPoint> points, glm::ivec2 cursor, float screenWidth,
                                            float halfWidth)
{
	for (size_t i = 1; i < points.size(); ++i)
	{
		const auto& a = points[i - 1];
		const auto& b = points[i];
		if (!a.inFront || !b.inFront || a.depth <= k_MinDepth || b.depth <= k_MinDepth)
		{
			continue;
		}
		const auto along = TouchAlong(cursor, glm::ivec2(a.pixel), TouchPixels(screenWidth, halfWidth, a.depth),
		                              glm::ivec2(b.pixel), TouchPixels(screenWidth, halfWidth, b.depth));
		if (along.has_value())
		{
			return a.depth + ((b.depth - a.depth) * *along);
		}
	}
	return std::nullopt;
}

creature_leash::Lengths leash_snap::LengthsOnLettingGo(const glm::vec3& hand, const glm::vec3& tied, float tiedRadius)
{
	const auto length = std::max(glm::distance(hand, tied), k_RadiusReach * tiedRadius);
	return {.slack = length * k_SlackShare, .max = length};
}

creature_leash::Lengths leash_snap::CollarRopeLengths(const creature_leash::Lengths& leash)
{
	return {.slack = leash.slack * k_CollarRopeScale, .max = leash.max * k_CollarRopeScale};
}

float leash_snap::PinchShare(Hand hand)
{
	return hand == Hand::Holding ? k_HeldShare : 0.0f;
}
