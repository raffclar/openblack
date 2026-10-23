/*******************************************************************************
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

/// Where a player's hand and camera are, as the last report that a turn did tells it (player_commands::HandReport). The
/// world reads the hand and the camera of a player only from here, never from the hand that the frames draw. All the
/// values are zero until the first report.
struct ReportedHand
{
	/// The point under the cursor that the interface picks
	glm::vec3 collidePoint {0.0f};
	/// The place of the hand
	glm::vec3 handPoint {0.0f};
	/// The place of the camera
	glm::vec3 camera {0.0f};
	/// The point that the camera looks at
	glm::vec3 focus {0.0f};
};

} // namespace openblack::ecs::components
