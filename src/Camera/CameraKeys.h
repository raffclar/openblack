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

/// The keys that move the world camera. Each key that the player holds adds or removes the same step, so two keys in
/// opposite directions cancel.
namespace openblack::camera_keys
{

/// The step of a key in one second
inline constexpr float k_StepPerSecond = std::bit_cast<float>(0x43C80000u);
/// The seconds in one millisecond, as the game keeps it
inline constexpr float k_SecondsPerMillisecond = std::bit_cast<float>(0x3A83126Fu);

/// The keys that the player holds in this frame
struct Held
{
	bool turnLeft {false};
	bool turnRight {false};
	bool tiltUp {false};
	bool tiltDown {false};
	bool forwards {false};
	bool backwards {false};
	bool left {false};
	bool right {false};
	/// The zoom key (Ctrl): the forwards and backwards keys zoom, and the left and right keys turn
	bool zoom {false};
	/// The rotate key (Shift): the forwards and backwards keys tilt, and the left and right keys turn
	bool rotate {false};
	/// The camera watches a fight: the arrow keys always tilt and turn, unless the player holds the zoom key
	bool fight {false};
};

/// What the keys ask the camera to do in this frame
struct Steps
{
	/// Positive turns to the right
	float turn {0.0f};
	/// Positive tilts up
	float tilt {0.0f};
	/// Positive zooms out
	float zoom {0.0f};
	/// Positive moves backwards
	float forwards {0.0f};
	/// Positive moves to the right
	float strafe {0.0f};
};

/// The seconds of a frame of the camera's clock, from its whole milliseconds
[[nodiscard]] float FrameSeconds(int32_t milliseconds);

/// Whether the arrow keys tilt and turn the camera: the zoom key stops it; a fight or the rotate key starts it
[[nodiscard]] bool ArrowsRotate(const Held& held);

/// The steps of the keys in a frame of these seconds, in the order that the game adds them
[[nodiscard]] Steps Step(const Held& held, float seconds);

} // namespace openblack::camera_keys
