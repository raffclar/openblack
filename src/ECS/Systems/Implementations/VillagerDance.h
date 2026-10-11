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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager dancing in a dance a script put it in
namespace openblack::ecs::villager_dance
{

/// The sexes a dance's group may take a villager as: a man, or a woman
[[nodiscard]] uint32_t DanceSex(entt::entity villager);

/// Each turn in a dance: it faces as its group's move has it, and walks to its place in its group's shape when it isn't
/// there, to dance on there
uint32_t InDance(components::LivingAction& action);
/// Where a dancer's place in its group's shape is now, as walks reach it; none for one in no group
[[nodiscard]] std::optional<glm::vec2> SlotOf(entt::entity villager);
/// The same in any dance state: walking off, it comes back to that state at its place
uint32_t PerformDance(components::LivingAction& action, VillagerStates danceState);
/// Walking to its place in a dance: a place that moves this turn is walked to where it is now
uint32_t MoveToDancePos(components::LivingAction& action);
/// The clip a villager plays in its place in a dance, `walkClip` when its group's move has it walk
[[nodiscard]] int32_t DanceClip(entt::entity villager, int32_t walkClip);
/// A dancer whose group starts a move plays its clip again
void PlayClipAgain(entt::entity villager);
/// Leaving the dance for a state of its own: it leaves the dance, and may go only into a state that comes and goes
/// without changing what it was doing. Into another script state it stays in the dance and may go. True refuses.
bool ExitInDance(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_dance
