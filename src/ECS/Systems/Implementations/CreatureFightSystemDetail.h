/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandAvoid.h"
#include "Common/GameHeading.h"
#include "Creature/CreatureArena.h"
#include "Creature/CreatureEyes.h"
#include "Creature/FightTranscript.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}
namespace openblack::graphics
{
class L3DMesh;
}

/// What the files of the creature fight system share: the length of a game turn, the helpers that more than one file
/// uses and the arena helpers that the fight stages use. Not for use elsewhere.
namespace openblack::ecs::systems::fight_detail
{
using components::CreatureAnimation;
using components::CreatureFighting;
using components::CreatureKnockedOut;
using components::CreatureLocomotion;

glm::vec2 Flat(const glm::vec3& point);

float GroundAt(glm::vec2 point);

bool Has(entt::entity creature, size_t animation);

/// A direction in the world as seen by a creature facing a heading: x to its right, y ahead
glm::vec2 WorldToLocal(glm::vec2 world, GameHeading heading);

namespace arena = openblack::creature_arena;

/// Where creatures may go on the land, worked out the first time it is asked for on a land
const land_avoid::Map& LandAvoidOf(ecs::Registry& registry);

/// The creature that took an arena leaves its fight: an arena made for the fight goes, any other is free again
void ReleaseArena(ecs::Registry& registry, entt::entity entity);

/// The creature that started a fight takes its arena after the face-off: the arena appears for both fighters. Without
/// an arena near enough, or room for one, it takes none.
bool DrawArenaFor(ecs::Registry& registry, entt::entity creature, entt::entity opponent);

/// When either fighter is not inside the range of the arena, the fighter makes no moves. In its stance, it steps back to
/// the middle when it is farther from the middle than its body radius and its species has that step. A block stops.
/// Returns whether either fighter is not inside the range.
bool KeepInArena(ecs::Registry& registry, entt::entity creature, CreatureFighting& fighting);
} // namespace openblack::ecs::systems::fight_detail
