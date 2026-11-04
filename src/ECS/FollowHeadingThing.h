/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

#include "Camera/FollowHeadingCameraModel.h"

namespace openblack::ecs
{

/// The thing of the camera that follows a thing and turns with it, as the camera reads it in a frame: where it is,
/// raised by half its height, the way it faces, and whether a creature turns too fast. None once it is gone.
[[nodiscard]] FollowHeadingCameraModel::ThingLookup FollowHeadingThing(entt::entity thing);

} // namespace openblack::ecs
