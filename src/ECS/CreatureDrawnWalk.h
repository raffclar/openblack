/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"

/// Where a walking creature is drawn between two game turns. The turn moves the creature from one place to the next;
/// the frames draw it at the part of the way that matches the part of the turn that passed. These rules read the walk
/// and change nothing of it, so the frame rate cannot change the world.
namespace openblack::ecs::creature_drawn_walk
{

/// The place, the turn and the animations of the legs of a walking creature a part of the way through the turn, 0 to 1.
/// At 1 it is the place where the turn took it. Standing animations play at the point of the breath.
[[nodiscard]] components::CreatureAnimation::DrawnPlace PlaceAt(const components::CreatureLocomotion& walk, float breathPhase,
                                                                float fraction);

/// Whether a new drawn place differs from the one before it: the place, the turn or any animation of the legs
[[nodiscard]] bool Changed(const std::optional<components::CreatureAnimation::DrawnPlace>& before,
                           const components::CreatureAnimation::DrawnPlace& after);

} // namespace openblack::ecs::creature_drawn_walk
