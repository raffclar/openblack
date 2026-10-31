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
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;
}

// How big the things of the world are, as creatures measure them to walk up to them and cast at them (see
// creature_cast_moves): a thing's radius on the ground and height from its model and scale, the radius of a field and a fish
// farm always 5, a creature's from its size and how far its bones reach.
namespace openblack::ecs::systems::object_measures
{

/// The part of a thing whose model gives its radius on the ground and its height. A town centre's totem measures as the
/// icon on its plinth; every other thing measures as itself.
[[nodiscard]] entt::entity MeasuredPart(const Registry& registry, entt::entity entity);
/// A thing's radius on the ground, 0 for one without a model. Fields and teleport stones have fixed radii.
[[nodiscard]] float TwoDRadius(const Registry& registry, entt::entity entity);
/// The radius that a creature keeps clear of things by on its walks. In a temple's pen it is the radius of the creature's
/// own size, not of its shown size. While the creature clears its way, it is the radius of its shown size.
[[nodiscard]] float NavRadius(const Registry& registry, entt::entity creature);
/// A thing's height, 0 for one without a model
[[nodiscard]] float Height(const Registry& registry, entt::entity entity);
/// What a creature walking up to the thing keeps clear of: the thing's radius less a share of the creature's radius on
/// its walks (NavRadius)
[[nodiscard]] float RoutePlanRadius(const Registry& registry, entt::entity entity, entt::entity creature);
/// A building or anything else of several map cells fixed to the land, or a tree: a creature walking up to one stops
/// where its walk takes it
[[nodiscard]] bool WalkedUpToItself(const Registry& registry, entt::entity entity);
/// The distance from the edge of a thing to the edge of another thing, across the land. A worship site measures from
/// its middle with its fixed reach. Every other thing measures from where it stands with its radius on the ground.
[[nodiscard]] float DistanceFromObject(const Registry& registry, entt::entity entity, entt::entity other);
/// Where a thing stands, none once it has gone
[[nodiscard]] std::optional<glm::vec3> PositionOf(const Registry& registry, entt::entity entity);

} // namespace openblack::ecs::systems::object_measures
