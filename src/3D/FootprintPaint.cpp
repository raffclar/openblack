/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootprintPaint.h"

#include <cmath>

#include <algorithm>

namespace openblack::footprint_paint
{

glm::vec2 SnapToTexelCorner(glm::vec2 world)
{
	return {std::floor(world.x * k_TexelsPerUnit + 0.5f), std::floor(world.y * k_TexelsPerUnit + 0.5f)};
}

int FootprintTexel(float coordinate, int size)
{
	return std::clamp(static_cast<int>(std::floor(coordinate + 0.5f)), 0, size - 1);
}

uint16_t Paint(uint16_t land, uint16_t footprint)
{
	const auto alpha = static_cast<uint8_t>(footprint >> 12);
	const auto channel = [&](int shift) {
		const auto mixed =
		    MixChannel(static_cast<uint8_t>((land >> shift) & 0xF), static_cast<uint8_t>((footprint >> shift) & 0xF), alpha);
		return static_cast<uint16_t>(mixed << shift);
	};
	return static_cast<uint16_t>((land & 0xF000) | channel(8) | channel(4) | channel(0));
}

uint8_t MixChannelAsDrawn(uint8_t land4, float premultiplied, float coverage)
{
	// The mix's exact result is a whole number of 255ths of a level, so a thousandth of a level absorbs the rounding of
	// the stored layer without lifting any result to the next level
	constexpr float k_RoundingAllowance = 0.001f;
	const float land = static_cast<float>(land4) / 15.0f;
	const float mixed = land * (1.0f - coverage) + premultiplied;
	return static_cast<uint8_t>(std::floor(mixed * 15.0f + k_RoundingAllowance));
}

} // namespace openblack::footprint_paint
