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
#include <limits>

#include "Common/X87.h"

/// The script natives that ask about the last miracle that a player cast: its time, its magic type and its place.
/// Pure rules, tested on their own.
///
/// The game keeps one record of the last cast for each player, but nothing in the game writes it. Each player starts
/// with the record at its start values, and the record keeps these values for all of the game (vanilla-bugs row 926).
namespace openblack::script::cast_history
{

/// The start values of the record of the last cast, which the record always keeps
struct LastCast
{
	/// The place on the map, in whole map units
	int32_t x {0};
	int32_t z {0};
	/// The height above the land, in metres
	float height {0.0f};
	/// The magic type, as the script reads it
	uint32_t type {0};
	/// The game turn of the cast, -1 before a cast
	int32_t turn {-1};
};

/// The value of a time in milliseconds as seconds: 0.001 as the game keeps it
inline constexpr float k_SecondsPerMillisecond = std::bit_cast<float>(0x3A83126Fu);
/// The time that a script reads for a player that is not there: the largest float
inline constexpr float k_NoPlayerTime = std::numeric_limits<float>::max();

/// The seconds from the last cast of a player to the game turn now. The game subtracts the turns as 32 bit numbers,
/// multiplies them by the length of a turn in milliseconds, and changes the product to seconds. Each step rounds to a
/// float.
[[nodiscard]] inline float SecondsSinceCast(uint32_t turnNow, const LastCast& cast, int32_t millisecondsPerTurn)
{
	const auto turns = static_cast<int32_t>(turnNow - static_cast<uint32_t>(cast.turn));
	const float milliseconds = x87::Mul(static_cast<double>(turns), static_cast<double>(millisecondsPerTurn));
	return milliseconds * k_SecondsPerMillisecond;
}

} // namespace openblack::script::cast_history
