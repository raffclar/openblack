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
#include <vector>

/// The life of a town added up for the scripts, as a cut scene compares a town's life before and after it
namespace openblack::script::town_health
{

/// A building of a town: the life of each of its people in their order, and its own life
struct AbodeLives
{
	std::vector<float> people;
	float life {0.0f};
};

/// The life of a town's buildings and of all its people added up, in this order: for each building, its people one by
/// one, then the building itself; then each homeless person. Each step is a float add, as the game adds them.
[[nodiscard]] inline float Total(std::span<const AbodeLives> abodes, std::span<const float> homeless)
{
	float total = 0.0f;
	for (const auto& abode : abodes)
	{
		float people = 0.0f;
		for (const auto life : abode.people)
		{
			people = life + people;
		}
		total += people;
		total += abode.life;
	}
	for (const auto life : homeless)
	{
		total += life;
	}
	return total;
}

} // namespace openblack::script::town_health
