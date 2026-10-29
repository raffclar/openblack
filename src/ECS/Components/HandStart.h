/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Where the hands and the camera of a player start (see hand_start::AtCitadel). The player gets it when a land script
/// makes its citadel. A hand or a computer mind that the game makes for the player after that starts there too.
struct HandStart
{
	/// The point of the citadel, in metres
	glm::vec3 point {0.0f};
};

} // namespace openblack::ecs::components
