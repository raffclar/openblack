/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandColourStamps.h"

#include <cmath>

#include <algorithm>

namespace openblack::land_colour_stamps
{

Placement Place(glm::vec2 xz)
{
	// A cell is 10 units; each axis gives its cell, cut towards zero and then down, and the weight of the next texel,
	// cut towards zero. Each step is a float.
	const auto axis = [](float position, int32_t& cell, int32_t& weight) {
		const float cells = position * 0.1f;
		cell = static_cast<int32_t>(cells);
		float next = 0.0f;
		if (cells < 0.0f)
		{
			next = (static_cast<float>(cell) - cells) * 255.0f;
			--cell;
		}
		else
		{
			next = 255.0f - ((cells - static_cast<float>(cell)) * 255.0f);
		}
		weight = static_cast<int32_t>(next) & 0xFF;
	};
	Placement placement {};
	axis(xz.x, placement.cell.x, placement.weight.x);
	axis(xz.y, placement.cell.y, placement.weight.y);
	return placement;
}

glm::vec2 CentredCorner(const glm::vec3& centre, int32_t side)
{
	const float half = static_cast<float>(side - 1) * 5.0f;
	return {centre.x - half, centre.z - half};
}

uint8_t Strength(float strength)
{
	return static_cast<uint8_t>(static_cast<int32_t>(std::clamp(strength * 255.0f, 0.0f, 255.0f)));
}

std::vector<uint8_t> LightningImage()
{
	constexpr int32_t k_Half = k_LightningSide / 2;
	constexpr float k_Radius = 32.0f;
	constexpr float k_Falloff = 9.0f;
	std::vector<uint8_t> texels(static_cast<size_t>(k_LightningSide) * k_LightningSide * 3, 0);
	for (int32_t row = -k_Half; row < k_Half; ++row)
	{
		for (int32_t column = -k_Half; column < k_Half; ++column)
		{
			const auto distance = std::sqrt(static_cast<float>((row * row) + (column * column)));
			if (!(distance < k_Radius))
			{
				continue;
			}
			const auto value = static_cast<uint8_t>(std::min(255, static_cast<int32_t>((k_Radius - distance) * k_Falloff)));
			const auto at = (static_cast<size_t>(row + k_Half) * k_LightningSide + static_cast<size_t>(column + k_Half)) * 3;
			texels[at] = value;
			texels[at + 1] = value;
			texels[at + 2] = value;
		}
	}
	return texels;
}

} // namespace openblack::land_colour_stamps
