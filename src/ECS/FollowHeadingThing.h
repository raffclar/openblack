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

#include "Camera/CreatureFollow.h"
#include "Camera/FollowHeadingCameraModel.h"

namespace openblack::ecs::components
{
struct Transform;
} // namespace openblack::ecs::components

namespace openblack::ecs
{
class Registry;

/// The height of a thing as a camera reads it: the body of a creature, or the height of the model of another thing
[[nodiscard]] float CameraThingHeight(const Registry& registry, entt::entity thing, const components::Transform& transform);
/// The game angle of a thing that walks (2048 to the circle), as a follow camera that turns with its thing reads it. A
/// creature always has the angle 0. None for a thing that does not walk.
[[nodiscard]] std::optional<uint16_t> CameraThingGameAngle(const Registry& registry, entt::entity thing);

/// The thing of a follow camera as the camera reads it in a frame. A thing with a place is where the frame draws it. A
/// flock is at the map place of its leader, or at its own place with no leader. None for a thing that is gone or has no
/// place.
[[nodiscard]] std::optional<creature_follow::Target> CameraTargetOf(const Registry& registry, entt::entity thing);

/// The thing of the camera that follows a thing and turns with it, as the camera reads it in a frame: where it is,
/// raised by half its height, the way it faces, and whether a creature turns too fast. None once it is gone.
[[nodiscard]] FollowHeadingCameraModel::ThingLookup FollowHeadingThing(entt::entity thing);

} // namespace openblack::ecs
