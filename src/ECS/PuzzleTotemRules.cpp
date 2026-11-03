/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PuzzleTotemRules.h"

#include <cmath>

#include "3D/MapCoords.h"

using namespace openblack::ecs;

float puzzle_totem::SinkDepth(float halfHeight, float scale)
{
	// Each step rounds to a float, in the game's order
	const float hidden = 1.0f - k_ShownWhenDown;
	const float depth = (hidden * halfHeight) * scale;
	return depth + depth;
}

float puzzle_totem::ShareOf(int32_t height, int32_t highest)
{
	if (highest == k_TapOnly)
	{
		return 1.0f;
	}
	return static_cast<float>(height) / static_cast<float>(highest);
}

float puzzle_totem::OffsetFor(float share, float depth)
{
	return (share * depth) - depth;
}

int32_t puzzle_totem::HeightForShare(float share, int32_t highest)
{
	if (highest == k_TapOnly)
	{
		return k_TapOnly;
	}
	const int32_t height = map_coords::FtoL(static_cast<float>(highest) * share);
	return height > highest ? highest : height;
}

float puzzle_totem::ShareForOffset(float offset, float depth)
{
	const float share = (depth - std::abs(offset)) / depth;
	// Less than none, or no number at all, is none
	if (!(share >= 0.0f))
	{
		return 0.0f;
	}
	return share > 1.0f ? 1.0f : share;
}

float puzzle_totem::HoldAfterSlide(float hold, float slide, float depth)
{
	const float moved = (slide / k_SlideDivisor) + hold;
	if (-depth > moved)
	{
		return -depth;
	}
	return moved > 0.0f ? 0.0f : moved;
}

int32_t puzzle_totem::HeightForHold(float hold, float depth, int32_t highest)
{
	if (highest == k_TapOnly)
	{
		return k_TapOnly;
	}
	const float shown = (depth - std::abs(hold)) / depth;
	const int32_t height = map_coords::FtoL(shown * static_cast<float>(highest));
	return height > highest ? highest : height;
}

float puzzle_totem::HoldOffset(int32_t height, int32_t highest, float depth)
{
	return -((1.0f - ShareOf(height, highest)) * depth);
}
