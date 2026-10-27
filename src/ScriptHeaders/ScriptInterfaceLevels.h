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

#include "ECS/Components/InterfaceLimits.h"

namespace openblack::ecs
{
class Registry;
}

/// The limits that the tutorial's interface levels put on the Action button of the player
namespace openblack::script::interface_levels
{

/// The limits after a script sets the interface level. The normal level and the two levels that let the player rotate
/// and interact remove all the limits. The level that lets the hand interact adds only the limit on holding. Each other
/// level that the game knows adds both limits. A level adds to the limits that are there and does not remove them. A
/// level that the game does not know keeps the limits.
[[nodiscard]] ecs::components::InterfaceLimits AfterLevel(ecs::components::InterfaceLimits limits, int32_t level);

/// Whether the Action button can take hold of a creature or a town's totem
[[nodiscard]] bool MayHold(const ecs::components::InterfaceLimits& limits);

/// Whether the Action button of an empty hand can act on the thing under the hand: pick it up, tap it or tap the land
[[nodiscard]] bool MayPress(const ecs::components::InterfaceLimits& limits, bool highlightUnderHand);

/// Gives the player the limits after a script sets the interface level. A player with no limits yet starts with none.
void Apply(ecs::Registry& registry, entt::entity player, int32_t level);

/// The limits of the player. A player with no limits yet has none. A player who is not in the game yet, as before the
/// first turn of a network game, also has none.
[[nodiscard]] ecs::components::InterfaceLimits LimitsOf(const ecs::Registry& registry, std::optional<entt::entity> player);

} // namespace openblack::script::interface_levels
