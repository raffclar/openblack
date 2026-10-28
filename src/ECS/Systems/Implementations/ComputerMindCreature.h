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

#include "Creature/CreatureDesires.h"
#include "ECS/ComputerMind.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

/// What the parts of the mind of a computer god that look after its creature and play with it read of the creature
namespace openblack::ecs::systems::computer_world
{

/// The creature of the god, none when it has none or a script controls it
[[nodiscard]] entt::entity CreatureOf(PlayerNames player);
/// The desire of the plan that the creature made last, if any
[[nodiscard]] std::optional<creature_desires::Desire> PlanDesireOf(entt::entity creature);
/// The player holds the creature on the leash of the player
[[nodiscard]] bool LeashedBy(PlayerNames player, entt::entity creature);
/// How free the creature is for the god. On the god's leash, the creature is not free for a time after the leash is tied.
[[nodiscard]] float FreedomOf(PlayerNames player, entt::entity creature);

/// The object of the node, or of the first node above it that has one
[[nodiscard]] entt::entity ObjectOf(const computer_mind::Mind& mind, uint16_t node);
/// The object of the first node above the node that has one
[[nodiscard]] entt::entity ObjectAboveOf(const computer_mind::Mind& mind, uint16_t node);
/// The object of the leaf that the node chose, or of the first node above that leaf that has one
[[nodiscard]] entt::entity LeafObjectOf(const computer_mind::Mind& mind, uint16_t node);

/// Each turn, the god takes off the evil leash of its creature from another creature that is out of the influence of the
/// god, or while its own creature tries to get its health back
void CheckCreatureLeash(PlayerNames player);

} // namespace openblack::ecs::systems::computer_world
