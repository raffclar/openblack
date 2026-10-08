/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Where a creature's body is drawn this frame: between where it was at the start of the turn and where it is at its
/// end. Its Transform moves only once a turn.
struct CreatureDrawPose
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	/// The scale it is drawn at when that is not its own (smaller in its temple's pen); its Transform keeps its own
	std::optional<glm::vec3> scale;
};

} // namespace openblack::ecs::components
