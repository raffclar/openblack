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

#include <glm/mat3x3.hpp>
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
	/// The way the frame draws it facing, radians from +x towards +z: the way it last moved. Only the draw reads it.
	float heading {0.0f};
	/// How far into its swimming clip the frame draws it, in milliseconds. Only the draw reads it.
	uint32_t clipPlace {0};
	/// Where the frame draws it between the start of its turn and where it is. Only the draw reads it. The game turns go by
	/// the place above and by its Transform. Empty before its first frame.
	struct DrawnPlace
	{
		glm::vec3 position;
		glm::mat3 rotation;
	};
	std::optional<DrawnPlace> drawn;
};

} // namespace openblack::ecs::components
