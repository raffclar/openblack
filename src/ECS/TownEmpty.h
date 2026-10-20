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

namespace openblack::ecs::town_empty
{

/// When the last person leaves a town, the town waits this many of its turns before it becomes empty
inline constexpr uint32_t k_EmptyTownTurns = 50;

/// The countdown after a person leaves the town: it starts again when nobody is left, else it does not change
[[nodiscard]] constexpr uint32_t AfterLeaving(uint32_t countdown, uint32_t peopleLeft)
{
	return peopleLeft == 0 ? k_EmptyTownTurns : countdown;
}

/// The countdown at the town's turn. A running countdown goes down by one. When it gets to zero the town becomes
/// empty (the result is true). If it does not get to zero but the town has people again, it stops.
[[nodiscard]] constexpr bool Turn(uint32_t& countdown, uint32_t people)
{
	if (countdown == 0)
	{
		return false;
	}
	--countdown;
	if (countdown == 0)
	{
		return true;
	}
	if (people != 0)
	{
		countdown = 0;
	}
	return false;
}

} // namespace openblack::ecs::town_empty
