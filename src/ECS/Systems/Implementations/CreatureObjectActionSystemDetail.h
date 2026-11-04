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

#include <array>
#include <optional>
#include <span>
#include <string>

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "CreatureObjectActionSystem.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

/// What the files of the creature object action system share: the measures of the body and the hand, the reach, the
/// meal, the catch and the kick helpers that more than one file uses. Not for use elsewhere.
namespace openblack::ecs::systems::object_action_detail
{
using components::Creature;
using components::CreatureAnimation;
using components::CreatureHeldObject;
using components::CreatureObjectAction;
using components::Transform;
using creature::CreatureRig;

const CreatureRig::ActionPoints* PointsOf(const Creature& creature);

glm::mat4 PlacementOf(const Transform& transform);

/// Something eaten is gone: out of its home and its town first
void Consume(ecs::Registry& registry, entt::entity creature, entt::entity food);

/// The body takes in the food of a thing that it eats, and answers the energy that the food gave it
float TakeInFood(entt::entity creature, std::optional<float> value);

/// The creature swallows what it ate. A thing it can't swallow it brings back up. Otherwise the meal is counted, and
/// the creature keeps it down or brings it up.
void Swallow(entt::entity creature, CreatureObjectAction& action, std::optional<float> value);

/// The foot meets what the creature kicks. The order of the rules is:
/// 1. A building breaks with two blows when it is fully built and not on fire.
/// 2. A thing that can fly flies off ahead of the creature. A thing in the air takes the new speed. A thing on the land
///    goes into the air from where it lies, from the player of the creature.
/// 3. Another creature takes a tenth of a hit, and more as the kicker is tall for it.
void KickAway(entt::entity creature, const Creature& body, const Transform& transform, const CreatureAnimation& animation,
              entt::entity target);

void Fail(CreatureObjectAction& action, std::string reason,
          creature_object_actions::Failure kind = creature_object_actions::Failure::Other);

/// Whether a thing lies on the map, where a creature can stomp on it: not held, flying or carried off.
/// A piece that broke off a building goes into the cells of the map when it comes to rest, as a rock does.
bool OnTheMap(const ecs::Registry& registry, entt::entity thing);

/// One of the three creaks of a tree a creature pulls from the ground, drawn from the local random numbers
entt::id_type TreeCreak();

/// The spheres round the body of each creature come on and go off as its body actions start. While they are on, each
/// part that moved far enough since its last check knocks about what it reaches.
void Trample(ecs::Registry& registry, entt::entity only);
} // namespace openblack::ecs::systems::object_action_detail
