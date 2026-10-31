/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <bitset>
#include <string_view>

#include <glm/vec3.hpp>

#include "ECS/Components/Player.h"

/// The script natives that the lessons of the Gods' Playground ask: the miracles a player has had, the keys held, the
/// mouse's wheel, the camera icons by the hand, and whether a thing faces the camera. Pure rules, tested on their own.
namespace openblack::script::lesson_natives
{

/// The script error for a magic type out of the game's range
inline constexpr std::string_view k_InvalidMagic = "Invalid magic";
/// The script error for a thing that is not there
inline constexpr std::string_view k_ThingNotValid = "Thing not valid!";

/// The magic types that a script can name: 1 to 41. None (0) is not one of them.
inline constexpr int32_t k_FirstMagic = 1;
inline constexpr int32_t k_LastMagic = 41;

/// Whether a script names a magic type of the game
[[nodiscard]] constexpr bool ValidMagic(int32_t magic) noexcept
{
	return magic >= k_FirstMagic && magic <= k_LastMagic;
}

/// Whether a player has ever had a magic type enabled. A magic type out of the range gives false. A player that is not
/// there gives true, as in the game.
[[nodiscard]] inline bool HasPlayerMagic(int32_t magic, const ecs::components::Player::Miracles* miracles)
{
	if (!ValidMagic(magic))
	{
		return false;
	}
	if (miracles == nullptr)
	{
		return true;
	}
	return miracles->everEnabled.at(static_cast<size_t>(magic));
}

/// The game keeps a key's state for each of its key numbers 0 to 254: the keyboard's scan code, with 0x80 added for the
/// extended keys
inline constexpr size_t k_KeyCount = 255;
using KeysHeld = std::bitset<k_KeyCount>;

/// Whether a key that a script names by the game's key number is held. A number past the table gives false (the game
/// reads past its table).
[[nodiscard]] inline bool KeyDown(int32_t key, const KeysHeld& held)
{
	if (key < 0 || static_cast<size_t>(key) >= k_KeyCount)
	{
		return false;
	}
	return held.test(static_cast<size_t>(key));
}

/// Whether the mouse has a wheel: the computer's system says so, or the wheel has turned since the game started
[[nodiscard]] constexpr bool MouseHasWheel(bool systemSaysWheel, bool wheelTurned) noexcept
{
	return systemSaysWheel || wheelTurned;
}

/// Whether the cursor is within the turn of the world camera: any camera icon by the hand shows or is offered. The icons
/// are those of the world camera's last frame, after the draw took away the camera icons that the hand does not show.
[[nodiscard]] constexpr bool WithinRotation(uint32_t cameraIcons) noexcept
{
	return cameraIcons != 0;
}

/// The half angle of a view cone in radians, from its full angle in degrees, as the game rounds it
[[nodiscard]] float HalfViewAngle(float degrees);

/// Whether a point is inside the view cone of a thing that is not a creature. The thing is at `position` (its map
/// position in metres), faces the angle `yAngle` about the upright (the direction cos, sin across x and z), and sees
/// `halfAngle` to each side. The point is in the cone when it is between the two edges of the cone, or on an edge. The
/// game does the test in float, in its own order.
[[nodiscard]] bool InViewCone(glm::vec3 position, float yAngle, float halfAngle, glm::vec3 point);

} // namespace openblack::script::lesson_natives
