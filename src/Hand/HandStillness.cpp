/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandStillness.h"

#include <glm/geometric.hpp>

void openblack::hand_stillness::Step(ecs::components::HandStillness& stillness, const glm::vec3& position) noexcept
{
	const auto moved = position - stillness.last;
	stillness.drift += moved;
	stillness.last = position;
	stillness.velocity += (moved * k_TurnsPerSecond - stillness.velocity) * k_VelocityFollow;
	if (glm::length(stillness.drift) >= k_MoveDistance)
	{
		stillness.stillTurns = 0;
		stillness.drift = glm::vec3(0.0f);
	}
	else
	{
		++stillness.stillTurns;
	}
}
