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

#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/CreatureAttitudeRules.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// Villagers and the creatures they see (see creature_attitude for the rules). A creature's reaction follows it; a
/// villager that takes it up makes its town watch the creature. Curious, it looks at the creature for a while, then steps
/// closer, again and again; frightened, it runs; respecting it, it goes up to it, faces it and bows. Every turn it
/// follows its town's feeling: indifferent it goes back to what it was doing, and it leaves the creature for its town's
/// worship or when the creature goes into a fight. The state functions are the living action system's.
namespace openblack::ecs::villager_creature
{

/// A town's feeling about a creature: curious when it doesn't watch the creature
[[nodiscard]] creature_attitude::Attitude AttitudeOf(entt::entity town, entt::entity creature);
/// The town starts to watch a creature, curious about it, unless it watches it already
void StartWatching(entt::entity town, entt::entity creature, uint32_t turn);
/// The town feels so about a creature from now, as if the creature had just shown it (for the testbed)
void SetFeeling(entt::entity town, entt::entity creature, creature_attitude::Attitude attitude, uint32_t turn);
/// A town's turn of watching the creatures it watches: it forgets the ones that went, and judges each of the others by
/// what it last finished doing to what it means to act on next
void TownTurn(entt::entity town, uint32_t turn);

/// How urgently a villager heeds a creature's reaction, before nearness counts
[[nodiscard]] uint8_t Priority(entt::entity villager, entt::entity creature);
/// How many turns a living (a villager, a creature or an animal) reacts to a creature, and waits before reacting to it
/// again, at a distance from it; one without a town has no feeling about the creature to weigh
[[nodiscard]] uint32_t TurnsToReact(entt::entity villager, entt::entity creature, float distance);
[[nodiscard]] uint32_t TurnsBeforeReactingAgain(entt::entity villager, entt::entity creature, float distance);
/// The state a villager takes a creature's reaction up in: inspecting it, or running from a creature its town fears;
/// none when its town is indifferent to the creature or the creature has gone. Its town starts to watch the creature.
[[nodiscard]] std::optional<VillagerStates> Setup(entt::entity villager, entt::entity creature);

uint32_t InspectCreatureReaction(components::LivingAction& action);
uint32_t PerformInspectCreatureReaction(components::LivingAction& action);
uint32_t ApproachCreatureReaction(components::LivingAction& action);
uint32_t InitialiseRespectCreatureReaction(components::LivingAction& action);
uint32_t TurnToFaceCreatureReaction(components::LivingAction& action);
uint32_t PerformRespectCreatureReaction(components::LivingAction& action);
uint32_t FinishRespectCreatureReaction(components::LivingAction& action);
uint32_t FleeingFromCreatureReaction(components::LivingAction& action);
uint32_t MoveTowardsCreatureReaction(components::LivingAction& action);

} // namespace openblack::ecs::villager_creature
