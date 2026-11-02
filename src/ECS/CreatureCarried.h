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

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
}

/// What a creature carries, as its mind, the scripts, the towns and the carried thing see it. This is the thing in its
/// hand, except while it prepares a throw: then the hand keeps the thing until the release, but the creature carries
/// nothing.
namespace openblack::ecs::creature_carried
{

/// The thing that the creature carries, none when it carries nothing
[[nodiscard]] std::optional<entt::entity> CarriedBy(const Registry& registry, entt::entity creature);

/// The creature that carries the thing, none when no creature carries it
[[nodiscard]] std::optional<entt::entity> CarrierOf(const Registry& registry, entt::entity thing);

/// The thing in the hand of the creature, none when the hand is empty. While the creature prepares a throw, the thing
/// is in its hand, but the creature does not carry it.
[[nodiscard]] std::optional<entt::entity> InHandOf(const Registry& registry, entt::entity creature);

/// Whether the creature carries the thing
[[nodiscard]] bool Carries(const Registry& registry, entt::entity creature, entt::entity thing);

/// At each turn of the creature, the thing that it carries takes the place of the creature in the world and on the map.
/// The thing keeps its own turn, and it stays out of the map's cells. Nothing changes when the creature carries
/// nothing.
void FollowCarrier(Registry& registry, entt::entity creature);

} // namespace openblack::ecs::creature_carried
