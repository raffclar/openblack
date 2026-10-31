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

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

/// Where the hands and the camera of a player start. When a land script makes the citadel of a player, the reported
/// hand and the camera focus of the player go to the citadel. The camera goes 20 m above it and 20 m along z. This
/// occurs before the first report of the player.
namespace openblack::hand_start
{

/// The camera goes this distance above the citadel, in metres
constexpr float k_CameraRise = 20.0f;
/// The camera goes this distance along z from the citadel, in metres
constexpr float k_CameraBack = 20.0f;

/// The point of a citadel: its map position in metres, at the height of the land plus the altitude of the position.
/// The ground is the height of the land at the map position.
[[nodiscard]] inline glm::vec3 AtCitadel(const map_coords::MapCoords& place, float ground) noexcept
{
	return {map_coords::ToMetres(place.x), ground + place.altitude, map_coords::ToMetres(place.z)};
}

/// The camera that looks at a start point
[[nodiscard]] inline glm::vec3 CameraOf(const glm::vec3& point) noexcept
{
	return {point.x, point.y + k_CameraRise, point.z + k_CameraBack};
}

/// The camera of a god that no person plays, as the turns read it: where it is and the point that it looks at
struct GodCamera
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
};

/// The camera of a god that no person plays. The citadel of the god gives the point that the camera looks at, and the
/// camera starts above the citadel (see CameraOf). A computer mind of the god moves the camera to its hand, and the
/// point stays at the citadel. A god with no citadel has no such camera.
[[nodiscard]] inline std::optional<GodCamera> GodCameraOf(const std::optional<glm::vec3>& mindCamera,
                                                          const std::optional<glm::vec3>& citadel) noexcept
{
	if (!citadel.has_value())
	{
		return std::nullopt;
	}
	return GodCamera {.origin = mindCamera.value_or(CameraOf(*citadel)), .focus = *citadel};
}

} // namespace openblack::hand_start
