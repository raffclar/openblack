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

#include <bit>
#include <optional>

#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Camera/ScriptCameraModel.h"
#include "LandChange.h"

// Where the camera is when a game starts on a land. The land's script can give a start place for the camera. A new
// game or a skirmish on a land whose script gives no place, or the place at the origin of the map, starts with the
// camera at the player's temple. The camera then flies to the temple as the temple key makes it fly. A network game
// always starts that way, whatever place the script gives. Without a temple, the camera stays at the place of the
// script.
//
// The script's line only keeps the place. The game puts the camera there after the whole script has run, so the line
// can come before the line that loads the landscape.
//
// Pure, tested on made-up starts.

namespace openblack::start_camera
{

/// The camera that a land's start makes looks at a point from this far, with this heading and this pitch. The game
/// keeps the heading and the pitch as these floats.
inline constexpr float k_OriginDistance = 120.0f;
inline constexpr float k_OriginHeading = std::bit_cast<float>(uint32_t {0x402FEF9Eu});
inline constexpr float k_OriginPitch = std::bit_cast<float>(uint32_t {0x3E65C8FAu});

/// Where the camera that a land's start makes is, when it looks at a point
[[nodiscard]] inline glm::vec3 OriginOver(glm::vec3 focus)
{
	return script_camera::PointFromDistanceHeadingAndPitch(focus, k_OriginDistance, k_OriginHeading, k_OriginPitch);
}

/// The point that the camera looks at from the script's place (in metres along the two map axes). The point is above the
/// land of the given height there by the altitude of the script's place.
[[nodiscard]] constexpr glm::vec3 FocusOnLand(glm::vec2 scriptFocus, float landHeight, float altitude)
{
	return {scriptFocus.x, landHeight + altitude, scriptFocus.y};
}

/// How a land starts
struct LandStart
{
	/// Why the land loads
	land_change::Kind change;
	/// The land starts a network game
	bool networkGame;
	/// The start place of the camera that the script of the land gave, in map units with its height above the land. None
	/// when the script gave no place.
	std::optional<map_coords::MapCoords> scriptPlace;
};

/// Whether the camera starts at the temple of the player and flies to it, when the player has a temple
[[nodiscard]] constexpr bool StartsAtTemple(const LandStart& start)
{
	// Only a game that starts on the land makes a new camera: not a land of the story, a land of the land menu, a saved
	// game or the testbed
	if (start.change != land_change::Kind::NewGame && start.change != land_change::Kind::Skirmish)
	{
		return false;
	}
	if (start.networkGame)
	{
		return true;
	}
	// Only the place at the origin of the map with no height above the land is the same as no place
	return !start.scriptPlace.has_value() || *start.scriptPlace == map_coords::MapCoords {};
}

} // namespace openblack::start_camera
