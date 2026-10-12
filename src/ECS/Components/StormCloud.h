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

#include <entt/entity/entity.hpp>

#include "3D/StormClouds.h"

namespace openblack::ecs::components
{

/// One of a storm's own clouds (see storm_clouds), a puff of mist (components::Mist) wandering over the storm. Its Mist's
/// alpha is how opaque the storm makes it; its colour is the land's, tinted by the storm's shade as it is drawn.
struct StormCloud
{
	/// The storm it belongs to; it goes when the storm does
	entt::entity storm {entt::null};
	/// Which of its storm's clouds it is, in the order they gathered
	uint8_t index {0};
	storm_clouds::Drift drift;
	/// Its size as a share of the storm's reach
	float sizeFactor {0.0f};
	/// The storm's shade, 0 to 1 and beyond: how much darker than the land's colour it is
	float shade {0.0f};
	/// How brightly the storm's last flash of lightning lights it, 0 to 255
	uint8_t flashGrey {0};
};

} // namespace openblack::ecs::components
