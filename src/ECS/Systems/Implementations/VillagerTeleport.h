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

#include <array>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager turning aside into a teleport stone: it walks (or runs, when it was already going faster than its walk)
/// to the stone, jumps to the stone that leaves it closest to where it was going, then takes up again what it was doing
/// as its state table says. The state functions are the living action system's.
namespace openblack::ecs::villager_teleport
{

/// Heading for the stone, walking or running
uint32_t GoTowardsTeleportReaction(components::LivingAction& action);
/// At the stone: the jump, then on its way again
uint32_t TeleportReaction(components::LivingAction& action);

/// The states of a villager that turns aside into a teleport stone: the walk to it, the run to it and the jump
inline constexpr std::array k_TeleportStates {VillagerStates::GoTowardsTeleportReaction, VillagerStates::TeleportReaction,
                                              VillagerStates::GoTowardsTeleportReactionQuickly};

/// Whether a villager that leaves its stone also stops on its way to worship: it stops only when the next state is not
/// one of the stone's states and is a state that the villager works towards
[[nodiscard]] bool LeavesTheWayToWorship(VillagerStates next, bool nextIsFinal);
/// The villager leaves a state of the stone: it stops on its way to worship as the rule above says, then it lets go of
/// its reaction as every reaction does
bool ExitReactToTeleport(components::LivingAction& action, VillagerStates next);

/// The villager stops reacting to its stone and goes back to what it was doing, as every reaction ends
void StopReacting(components::LivingAction& action, entt::entity villager);

} // namespace openblack::ecs::villager_teleport
