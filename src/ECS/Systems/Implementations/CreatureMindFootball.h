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

#include "Creature/CreaturePlanActions.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}

/// The world's side of the football actions of a creature (see creature_football): the villagers that the actions are
/// done to, and what the agendas need to know
namespace openblack::ecs::systems::mind_football
{
/// Whether a villager plays football, as the creature sees it
[[nodiscard]] bool IsPlaying(const ecs::Registry& registry, entt::entity thing);

/// Whether a thing suits one of the targets of the football actions
[[nodiscard]] bool Accepts(const ecs::Registry& registry, creature_plan_actions::Target target, entt::entity thing,
                           entt::entity self);

/// Puts in the situation what a football action needs: the ball of the villager's pitch, the goal that the creature's
/// side attacks, and the first player of the other team
void Situate(const ecs::Registry& registry, entt::entity creature, const creature_plan_actions::Executor& executor,
             std::optional<uint32_t> object, creature_plan_actions::Situation& situation);

} // namespace openblack::ecs::systems::mind_football
