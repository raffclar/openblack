/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/GameHeading.h"
#include "Creature/CreatureDesires.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

namespace openblack::ecs::components
{
struct CreatureMindState;
} // namespace openblack::ecs::components

/// What a creature sees, and what a player's creature sees of what its player does
namespace openblack::ecs::creature_sight
{

/// The heading that the body of a creature has. Before its walk starts, it is the way that its model faces.
[[nodiscard]] GameHeading FacingHeading(const Registry& registry, entt::entity creature);

/// The heading along which a creature looks. When its head looks at a point, it is the way to that point on the land.
/// When its head looks at nothing, or at the position of the creature, it is the heading of its body.
[[nodiscard]] GameHeading LookHeading(const Registry& registry, entt::entity creature);

/// Whether a creature can see a point on the land, in metres. The point must be within two thirds of a half turn of
/// the way that the creature looks, or in the map cell of the creature.
[[nodiscard]] bool CanSeePos(const Registry& registry, entt::entity creature, glm::vec2 point);

/// The mind of the player's creature when it can see the point, from where it looks: none when the player has no
/// creature or it cannot see there
[[nodiscard]] components::CreatureMindState* MindSeeing(PlayerNames player, glm::vec3 point);

/// The player's creature, if it can see where the player did something, feels with the player: it takes it the player
/// wants more of one of its desires, by an amount, kept between none and all
void EmpathiseWithPlayer(PlayerNames player, CreatureDesires desire, float amount, glm::vec3 point);

/// The player's hand does something that shows a desire. The player's creature sees the hand when the player's camera
/// looks at the creature and the creature looks towards the hand. Then the desire grows faster for all time.
void SeePlayerAction(PlayerNames player, creature_desires::Desire desire);

} // namespace openblack::ecs::creature_sight
