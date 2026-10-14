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

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// How long the player's hand has stayed in one place, counted in game turns (see hand_stillness::Step)
struct HandStillness
{
	/// The game turns since the hand last moved far enough
	uint32_t stillTurns {0};
	/// How far the hand has gone since then, added up as one movement
	glm::vec3 drift {0.0f};
	/// Where the hand was on the last turn
	glm::vec3 last {0.0f};
};

} // namespace openblack::ecs::components
