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

/// On the hand of another player of a network game: how the frames of this computer move what that hand holds. They
/// move it in a straight line from the place of the hand before its last report to the reported place, in as much
/// game time as passed between the last two reports. Only the draw of this computer reads it.
struct HeldDrawTrack
{
	/// The place of the hand before the last report
	glm::vec3 from {0.0f};
	/// The game milliseconds of the frames since the last report
	int32_t elapsedMs {0};
	/// The game milliseconds of the frames between the two last reports, 0 before the second report
	int32_t intervalMs {0};
};

} // namespace openblack::ecs::components
