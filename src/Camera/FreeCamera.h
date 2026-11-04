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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The maths of the free camera, free of the game so that it can be tested alone. While the hand grips the land, the
/// camera flies forwards faster and faster, and the place of the mouse on the screen turns it. When the hand lets go,
/// it slows down. The movement keys move it along the view and to the side; with Shift they turn and tilt it, and with
/// Ctrl they move it to the side and up and down. The camera goes to its new place at once in each frame, also below
/// the land. Only a saved game of the original game gives this camera: no part of play makes it.
namespace openblack::free_camera
{

/// Each second, the speed falls to e to the power of this
inline constexpr float k_SpeedFall = -5.0f;
/// The speed moves the camera five times as far
inline constexpr float k_SpeedDistance = 5.0f;
/// The mouse turns and tilts the camera by this share of its distance from the middle of the screen in each second
inline constexpr float k_MouseTurn = 1.1f;
/// The keys move the camera by this share of their count
inline constexpr float k_KeyMove = 0.05f;
/// A key count tilts the camera by this angle
inline constexpr float k_KeyTilt = 0.002f;
/// Shift and the keys tilt the camera between these pitches
inline constexpr float k_LowestPitch = -3.0f;
inline constexpr float k_HighestPitch = 3.0f;
/// The keys count this many in each second
inline constexpr float k_KeyCountPerSecond = 400.0f;

/// What the camera keeps from frame to frame. A saved game keeps these values.
struct State
{
	float speed {0.0f};
	/// The counts of the keys of the last frame: to the side (right more than zero) and along the view (back more than
	/// zero)
	int32_t across {0};
	int32_t along {0};
	/// The keys that change what the movement keys do
	bool shift {false};
	bool ctrl {false};
};

/// What the camera reads in a frame
struct Input
{
	/// The time of the frame, in whole milliseconds
	int32_t milliseconds {0};
	/// The hand grips the land
	bool gripsLand {false};
	/// The place of the mouse on the screen, and the size of the screen, in pixels
	glm::ivec2 mouse {0};
	glm::ivec2 screen {1};
};

/// Where the camera stands and looks
struct View
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
};

/// The movement keys held in a frame
struct Keys
{
	bool left {false};
	bool right {false};
	bool forwards {false};
	bool backwards {false};
	bool shift {false};
	bool ctrl {false};
};

/// Keeps the counts of the movement keys for a frame of some milliseconds. Left wins over right, and forwards wins over
/// backwards.
void ReadKeys(State& state, const Keys& keys, int32_t milliseconds);

/// One frame of the camera from where it is now. The counts of the keys go back to zero.
[[nodiscard]] View Step(const View& now, State& state, const Input& input);

} // namespace openblack::free_camera
