/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>

// The total health of a town and its people, as a script reads it to measure the harm that an attack did to a village.
// Pure rules, tested without the game.

namespace openblack::ecs::town_health
{

/// The lives of one building of a town: the lives of the people who live in it, in the order of the building, and the
/// life of the building
struct AbodeLives
{
	std::span<const float> residents;
	float life {0.0f};
};

/// The sum of the lives of the people of a building, in their order
[[nodiscard]] constexpr float ResidentsTotal(std::span<const float> residents) noexcept
{
	float total = 0.0f;
	for (const float life : residents)
	{
		total = life + total;
	}
	return total;
}

/// The total health of a town. The rules are:
/// - For each building in the order of the town, add the sum of the lives of its people, then the life of the building.
/// - Then add the life of each person without a home, in the order of the town.
/// Each step adds to the total in floats.
[[nodiscard]] constexpr float Total(std::span<const AbodeLives> abodes, std::span<const float> homeless) noexcept
{
	float total = 0.0f;
	for (const auto& abode : abodes)
	{
		total = ResidentsTotal(abode.residents) + total;
		total = abode.life + total;
	}
	for (const float life : homeless)
	{
		total = life + total;
	}
	return total;
}

} // namespace openblack::ecs::town_health
