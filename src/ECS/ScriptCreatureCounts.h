/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include "ECS/Components/Creature.h"

/// The counts of actions that the scripts read and clear for a creature (CREATURE_GET_NUM_TIMES_ACTION_PERFORMED and
/// CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION). The game keeps three counts, by number:
/// - 0: the times that the creature started to eat a thing that its hand held.
/// - 1: the objects that its miracles destroyed.
/// - 2: a count that nothing adds to, so it is always 0.
/// A number above 2 is not a count. The game then reads and writes other values of the creature. openblack gives 0
/// and writes nothing.
namespace openblack::ecs::script_creature_counts
{

/// The count that has this number, none for a number that is not a count
[[nodiscard]] constexpr std::optional<float> Count(const components::Creature& creature, uint32_t number)
{
	switch (number)
	{
	case 0:
		return creature.timesEaten;
	case 1:
		return creature.objectsDestroyed;
	case 2:
		return 0.0f;
	default:
		return std::nullopt;
	}
}

/// Sets the count that has this number to 0. False for a number that is not a count.
constexpr bool Clear(components::Creature& creature, uint32_t number)
{
	switch (number)
	{
	case 0:
		creature.timesEaten = 0.0f;
		return true;
	case 1:
		creature.objectsDestroyed = 0.0f;
		return true;
	case 2:
		return true;
	default:
		return false;
	}
}

} // namespace openblack::ecs::script_creature_counts
