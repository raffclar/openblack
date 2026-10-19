/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"
#include "Creature/LeashRules.h"

/// How the area that holds a creature refuses the walks of a new agenda. The area checks each walk as the agenda gets it.
namespace openblack::creature_agenda_area
{

/// Whether the area refuses a step of a new agenda. It refuses only a walk to a point, not a walk to a thing. A walk to a
/// place near the creature starts from the place of the creature when the agenda starts.
[[nodiscard]] bool RefusesStep(const creature_leash::AgendaArea& area, glm::vec2 creature, const creature_mind::Step& step);

/// Whether the area refuses a walk of an agenda that the action of a plan makes. Then the plan fails.
[[nodiscard]] bool RefusesAgenda(const creature_leash::AgendaArea& area, glm::vec2 creature,
                                 std::span<const creature_mind::Step> agenda);

/// The steps of an agenda that a forced plan gives itself, without the walks that the area refuses. The plan continues
/// with the other steps. When the area refuses the main step, the step before it becomes the main step.
[[nodiscard]] std::vector<creature_mind::Step> WithoutRefusedWalks(const creature_leash::AgendaArea& area, glm::vec2 creature,
                                                                   std::vector<creature_mind::Step> agenda);

} // namespace openblack::creature_agenda_area
