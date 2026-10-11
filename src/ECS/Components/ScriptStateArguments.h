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

#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

/// What a script last gave a villager for the script states it puts it in: a place, a number and a pair of whole
/// numbers. Each state reads them its own way: wandering round a place takes the place, the number as how far from it
/// to wander, and the pair as the fewest and most turns to stand at each stop.
struct ScriptStateArguments
{
	/// On the ground, in metres
	glm::vec2 place {0.0f};
	float number {0.0f};
	uint32_t first {0};
	uint32_t second {0};
};

} // namespace openblack::ecs::components
