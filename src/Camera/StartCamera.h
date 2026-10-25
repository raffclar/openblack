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

#include <glm/gtc/constants.hpp>
#include <glm/gtx/polar_coordinates.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "LandChange.h"

// Where the camera is when a game starts on a land. The land's script can give a start place for the camera. A new
// game or a skirmish on a land whose script gives no place, or the place at the origin of the map, starts with the
// camera at the player's temple. The camera then flies to the temple as the temple key makes it fly. A network game
// always starts that way, whatever place the script gives. Without a temple, the camera stays at the place of the
// script.
//
// Pure, tested on made-up starts.

namespace openblack::start_camera
{

/// The camera that a land's start makes looks at a point from this far, at these angles
inline constexpr float k_OriginDistance = 120.0f;
inline constexpr glm::vec2 k_OriginAngles {glm::radians(12.8571f), glm::radians(157.51f)};

/// Where the camera that a land's start makes is, when it looks at a point
[[nodiscard]] inline glm::vec3 OriginOver(glm::vec3 focus)
{
	return focus + (k_OriginDistance * glm::euclidean(k_OriginAngles));
}

/// How a land starts
struct LandStart
{
	/// Why the land loads
	land_change::Kind change;
	/// The land starts a network game
	bool networkGame;
	/// The start place of the camera that the script of the land gave, in map units. None when the script gave no place.
	std::optional<glm::ivec2> scriptPlace;
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
	return !start.scriptPlace.has_value() || (start.scriptPlace->x == 0 && start.scriptPlace->y == 0);
}

} // namespace openblack::start_camera
