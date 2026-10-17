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

#include <array>

#include <glm/vec3.hpp>

namespace openblack::audio
{

/// The world axes of a placed sound that its bank header locks. A locked axis keeps the value the header gives when
/// the sound starts, and later moves do not change it.
enum class LockedAxis : uint8_t
{
	None = 0,
	X = 1U << 0U,
	Y = 1U << 1U,
	Z = 1U << 2U,
};

/// The bits of the bank header's override flags that lock the x, y and z axes of the sound's position
constexpr uint32_t k_HeaderLockX = 0x4;
constexpr uint32_t k_HeaderLockY = 0x8;
constexpr uint32_t k_HeaderLockZ = 0x10;

/// The axes that a bank header's override flags lock, as a mask of LockedAxis bits
[[nodiscard]] constexpr uint8_t LockedAxesOf(uint32_t overrideFlags)
{
	uint8_t axes = 0;
	if ((overrideFlags & k_HeaderLockX) != 0)
	{
		axes |= static_cast<uint8_t>(LockedAxis::X);
	}
	if ((overrideFlags & k_HeaderLockY) != 0)
	{
		axes |= static_cast<uint8_t>(LockedAxis::Y);
	}
	if ((overrideFlags & k_HeaderLockZ) != 0)
	{
		axes |= static_cast<uint8_t>(LockedAxis::Z);
	}
	return axes;
}

/// Whether the locks make a sound placed when its caller plays it without a position. Only a lock of the x or the y
/// axis does this. A lock of only the z axis keeps such a sound centred on the listener.
[[nodiscard]] constexpr bool LocksPlaceSound(uint32_t overrideFlags)
{
	return (overrideFlags & (k_HeaderLockX | k_HeaderLockY)) != 0;
}

/// Where a sound starts: the caller's position, with each locked axis set to the header's value for it. A caller that
/// gives no position gives the origin.
[[nodiscard]] constexpr glm::vec3 LockedStartPosition(uint32_t overrideFlags, const std::array<float, 3>& headerPosition,
                                                      const glm::vec3& callerPosition)
{
	auto position = callerPosition;
	if ((overrideFlags & k_HeaderLockX) != 0)
	{
		position.x = headerPosition[0];
	}
	if ((overrideFlags & k_HeaderLockY) != 0)
	{
		position.y = headerPosition[1];
	}
	if ((overrideFlags & k_HeaderLockZ) != 0)
	{
		position.z = headerPosition[2];
	}
	return position;
}

/// Where a placed sound goes when it moves to target: each locked axis keeps its current value
[[nodiscard]] constexpr glm::vec3 MoveWithLockedAxes(uint8_t lockedAxes, const glm::vec3& current, const glm::vec3& target)
{
	auto position = target;
	if ((lockedAxes & static_cast<uint8_t>(LockedAxis::X)) != 0)
	{
		position.x = current.x;
	}
	if ((lockedAxes & static_cast<uint8_t>(LockedAxis::Y)) != 0)
	{
		position.y = current.y;
	}
	if ((lockedAxes & static_cast<uint8_t>(LockedAxis::Z)) != 0)
	{
		position.z = current.z;
	}
	return position;
}

} // namespace openblack::audio
