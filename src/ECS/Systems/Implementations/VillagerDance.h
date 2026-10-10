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

#include <entt/entity/fwd.hpp>

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

/// Each turn in a dance: it dances its group's part
uint32_t InDance(components::LivingAction& action);
/// Leaving the dance for a state of its own: it leaves the dance. Into another script state it stays in it. True
/// refuses.
bool ExitInDance(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_dance
