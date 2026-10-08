/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandWaterGlow.h"

#include <cmath>

#include <algorithm>

#include "3D/LandIslandInterface.h"
#include "ECS/SeaCells.h"

using namespace openblack;

float hand_water_glow::Intensity(const glm::vec3& baseColour)
{
	// the same reading as night_lights::Update
	const float mean = (std::floor(baseColour.r * 255.0f + 0.5f) + std::floor(baseColour.g * 255.0f + 0.5f) +
	                    std::floor(baseColour.b * 255.0f + 0.5f)) /
	                   3.0f;
	if (mean >= 120.0f)
	{
		return 0.0f;
	}
	return std::clamp((120.0f - mean) / 15.0f, 0.0f, 1.0f);
}

uint32_t hand_water_glow::Colour(uint32_t warmRampTop, float intensity)
{
	// R + (255 - R) / 4, G + (128 - G) / 4 and B + (64 - B) / 4 with exact integer shifts, so each is
	// floor((3 c + target) / 4)
	const auto channel = [warmRampTop](uint32_t shift, uint32_t target) {
		const uint32_t c = (warmRampTop >> shift) & 0xFFu;
		return static_cast<uint32_t>((3 * static_cast<int32_t>(c) + static_cast<int32_t>(target)) >> 2) & 0xFFu;
	};
	const int alpha = std::clamp(static_cast<int>(intensity * static_cast<float>(k_MaxAlpha)), 0,
	                             static_cast<int>(k_MaxAlpha)); // truncates
	return static_cast<uint32_t>(alpha) << 24 | channel(16, 255) << 16 | channel(8, 128) << 8 | channel(0, 64);
}

bool hand_water_glow::NearLowLand(const LandIslandInterface& island, glm::vec2 xz)
{
	// truncate, then a signed division by 10: both truncate towards 0
	const auto cellOf = [](float v) { return static_cast<int>(v) / 10; };
	const int last = std::max(0, static_cast<int>(island.GetCellsPerSide()) - 1); // 511 in the original
	const int x0 = std::clamp(cellOf(xz.x - k_CellReach), 0, last);
	const int z0 = std::clamp(cellOf(xz.y - k_CellReach), 0, last);
	const int x1 = cellOf(xz.x + k_CellReach);
	const int z1 = cellOf(xz.y + k_CellReach);
	for (int z = z0; z <= z1; ++z)
	{
		for (int x = x0; x <= x1; ++x)
		{
			const auto* cell = ecs::sea_cells::CellAt(island, glm::ivec2(x, z));
			if (cell == nullptr || island.GetCellAltitude(*cell) < k_LowAltitude)
			{
				return true;
			}
		}
	}
	return false;
}

std::optional<hand_water_glow::Quad> hand_water_glow::Compute(const LandIslandInterface& island, const glm::vec3& baseColour,
                                                              uint32_t warmRampTop, const glm::vec3& handPosition)
{
	const float intensity = Intensity(baseColour);
	if (intensity <= 0.01f || !NearLowLand(island, glm::vec2(handPosition.x, handPosition.z)))
	{
		return std::nullopt;
	}
	return Quad {glm::vec2(handPosition.x, handPosition.z), Colour(warmRampTop, intensity)};
}
