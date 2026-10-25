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

#include "ECS/Components/HeldDrawTrack.h"

/// Where the frames of this computer draw what the hand of another player of a network game holds. The hand of that
/// player is not drawn.
namespace openblack::hand_grab::other_held
{

/// A report of the hand came: the time since the last report becomes the time of the movement, and the movement starts
/// from the place of the hand before the report
void Reported(ecs::components::HeldDrawTrack& track, glm::vec3 placeBefore);

/// A frame of some game milliseconds: the point on the straight line from the place before the last report to the
/// reported place, as far along as the frames since the report are of the time between the two last reports. Before the
/// second report, the reported place.
[[nodiscard]] glm::vec3 Step(ecs::components::HeldDrawTrack& track, glm::vec3 reported, int32_t frameMs);

} // namespace openblack::hand_grab::other_held
