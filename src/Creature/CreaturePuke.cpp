/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePuke.h"

namespace openblack::creature_puke
{

float DropSize(float shownSize)
{
	return shownSize * k_SizePerSize;
}

uint32_t ShadeChannel(uint32_t channel, int32_t factor)
{
	return static_cast<uint32_t>((static_cast<int32_t>(channel) * factor) / 255) & 0xFFu;
}

std::array<Drop, k_DropsPerTurn> DropsOf(float shownSize, uint32_t light, const LocalRandom& random)
{
	const float step = shownSize * k_SpeedStep;
	const float least = shownSize * k_SpeedLeast;
	const auto draw = [&random](int32_t range) { return static_cast<float>(random(range)); };
	const auto factor = [&random](const Shade& shade) { return static_cast<int32_t>(random(shade.range)) + shade.least; };
	std::array<Drop, k_DropsPerTurn> drops {};
	for (auto& drop : drops)
	{
		const float z = (draw(k_SpeedDraws) * step) - least;
		const float y = -(draw(k_SpeedDraws) * step);
		const float x = (draw(k_SpeedDraws) * step) - least;
		const auto red = factor(k_Red);
		const auto green = factor(k_Green);
		const auto blue = factor(k_Blue);
		drop.velocity = glm::vec3(x, y, z);
		drop.argb = (light & 0xFF000000u) | (ShadeChannel((light >> 16u) & 0xFFu, red) << 16u) |
		            (ShadeChannel((light >> 8u) & 0xFFu, green) << 8u) | ShadeChannel(light & 0xFFu, blue);
	}
	return drops;
}

} // namespace openblack::creature_puke
