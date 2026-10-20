/*******************************************************************************
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
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A bookmark of the camera, kept by a number key, and its marker on the land
struct CameraBookmark
{
	/// The slot of the bookmark: 0 for the key 1, up to 6 for the key 7
	uint8_t number;
	/// Whether the player set the bookmark. The marker shows only while it is set.
	bool set {false};
	/// The place of the bookmark: its place on the land, or the last place of the thing that it is on
	glm::vec3 point {0.0f};
	/// The thing that the bookmark is on, none for a bookmark on the land. When the thing goes, the bookmark stays at
	/// its last place.
	entt::entity thing {entt::null};
	/// The camera's position less the point, when the player set the bookmark. A jump puts the camera there again.
	glm::vec3 cameraOffset {0.0f};
};

} // namespace openblack::ecs::components
