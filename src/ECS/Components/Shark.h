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

/// A shark (the opening's sharks): it moves a turn at a time, as a script walks it, and is drawn swimming between where
/// it was at the start of the turn and where it is, facing the way it moves, its wake spreading on the water behind it
struct Shark
{
	/// Where it is this turn, on the land's grid
	glm::vec3 position {0.0f};
	/// Where it was at the start of the turn
	glm::vec3 turnStart {0.0f};
	/// The way it faces, radians from +x towards +z: the way it last moved
	float heading {0.0f};
	/// How far into its swimming clip it is, in milliseconds
	uint32_t clipPlace {0};
};

} // namespace openblack::ecs::components
