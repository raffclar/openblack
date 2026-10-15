/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}

/// The world's side of the actions with which a creature helps a town or plays with its people (see creature_town_help):
/// the things that the actions are done to, what the agendas need to know, and the totem and breeder steps
namespace openblack::ecs::systems::mind_town_help
{
/// Whether a thing suits one of the targets of the town help actions. The thing is in another creature's hand when
/// heldByOther is true.
[[nodiscard]] bool Accepts(const ecs::Registry& registry, creature_plan_actions::Target target, entt::entity thing,
                           entt::entity self, bool heldByOther);

/// Puts in the situation what a town help action needs: what the creature holds, where a worship site keeps its food,
/// and the home of a villager
void Situate(const ecs::Registry& registry, entt::entity creature, const creature_plan_actions::Executor& executor,
             std::optional<uint32_t> object, creature_plan_actions::Situation& situation);

/// The share of the town's people at worship, while the creature's step moves the town's totem; none when the step does
/// not move a totem, or the totem has no town that can worship
[[nodiscard]] std::optional<float> TotemShareOf(const ecs::Registry& registry, const creature_mind::IdleMind& idle);

/// Carries out the totem and breeder orders of a turn
void Apply(ecs::Registry& registry, const creature_mind::Commands& commands);

} // namespace openblack::ecs::systems::mind_town_help
