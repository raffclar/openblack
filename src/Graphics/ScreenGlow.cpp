/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenGlow.h"

#include <cmath>

#include <algorithm>

namespace openblack::graphics::screen_glow
{

glm::ivec2 SamplePoint(glm::ivec2 screen, glm::ivec2 point)
{
	// The columns are a sixteenth of the width apart, from the middle of the first sixteenth
	const int across = screen.x >> 4;
	const int x = (across / 2) + (point.x * across);
	// The rows are at a sixteenth of the height each, moved down by a thirty-second
	const int y = ((point.y * screen.y) / k_Side) + ((screen.y / k_Side) / 2);
	return {x, y};
}

int Step(int value, uint8_t screenChannel)
{
	const int target = static_cast<int>(screenChannel) << k_ChannelShift;
	// The division of a whole number cuts towards zero
	const int stepped = value + ((target - value) / 8);
	return std::clamp(stepped, 0, k_ChannelMost);
}

glm::ivec3 Levels(glm::ivec3 value)
{
	return {
	    (value.r + 0x80) >> 16,
	    (value.g + 0x800) >> 16,
	    (value.b + 0x8000) >> 16,
	};
}

glm::vec2 PointsAt(glm::vec2 pixel, glm::vec2 screen)
{
	return pixel * static_cast<float>(k_Side - 1) / screen;
}

uint8_t Shown(uint8_t frameChannel, float level, uint8_t strength)
{
	const float glow = (level / static_cast<float>(k_TopLevel)) * (static_cast<float>(strength) / 255.0f);
	const float shown = std::min(1.0f, (static_cast<float>(frameChannel) / 255.0f) + glow);
	return static_cast<uint8_t>(std::floor((shown * 255.0f) + 0.5f));
}

uint8_t Strength(uint8_t darkness, bool weather)
{
	return weather ? darkness : 0;
}

} // namespace openblack::graphics::screen_glow
