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

/// On the hand of another player: how the frames of this computer move what that hand holds. They move it in a straight
/// line from the place of the hand before its last report to the place of the turn, in as much game time as the frames
/// had between the last two reports. A person of a network game reports each turn. A computer mind reports after each
/// step, and the movement starts at the hand before the step. Only the draw of this computer reads it.
struct HeldDrawTrack
{
	/// The place of the hand before the last report (for a computer mind, before the last step)
	glm::vec3 from {0.0f};
	/// The game milliseconds of the frames since the last report
	int32_t elapsedMs {0};
	/// The game milliseconds of the frames between the two last reports, 0 before the second report
	int32_t intervalMs {0};
};

} // namespace openblack::ecs::components
