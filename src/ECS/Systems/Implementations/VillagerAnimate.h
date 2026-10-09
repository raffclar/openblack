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

#include "3D/AllMeshes.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager's clips as its states change: the clip of the state it goes into, the clips that take it into and out of
/// states, and the wait while those play. The rules are villager_animation's; this applies them to the villager. Part of
/// the living action system.
namespace openblack::ecs::villager_animate
{

/// The clip its top state plays now (choosing it, with any random draws, and what it carries): negative to keep the one
/// it has, the hidden clip to be out of sight
[[nodiscard]] int32_t StateClip(entt::entity villager);
/// Plays its top state's clip, from the start when it is a new one
void SetStateAnim(entt::entity villager);
/// Plays a clip, from the start when it is a new one or when asked to, unless it dances in time with others
void SetAnim(entt::entity villager, int32_t clip, bool restart);
/// The clip it plays now
[[nodiscard]] AnimId CurrentClip(entt::entity villager);

/// The clip taking it out of its top state into another, if any; marks the clip into the next one as due
[[nodiscard]] std::optional<AnimId> OutOfClip(components::LivingAction& action, VillagerStates next);
/// The clip taking it into the state it is now in, if any; marks it as playing
[[nodiscard]] std::optional<AnimId> IntoClip(components::LivingAction& action, VillagerStates state);

/// Whether its clip has played through so many times since its state changed
[[nodiscard]] bool IsReadyForNewAnimation(const components::LivingAction& action, uint32_t times = 1);
/// A clip into or out of a state has ended: the next one plays, or the state's own
void FinishedIntoOutOfAnimation(components::LivingAction& action);

/// Whether its top state keeps it out of sight
[[nodiscard]] bool IsHiddenByState(VillagerStates state);

} // namespace openblack::ecs::villager_animate
