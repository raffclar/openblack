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

/// The scripts' timers: a timer counts game turns from when it was set and holds how many it was set for
namespace openblack::script::timers
{

/// The game's turns are this long, in milliseconds
constexpr uint32_t k_MillisecondsPerTurn = 100;

/// A timer as it was last set: the turn it was set on and how many turns it was set for
struct Timer
{
	uint32_t setTurn {0};
	int32_t turns {0};
};

/// The turns a timer of some seconds runs for: the seconds times the turns in a second, cut down to whole turns
[[nodiscard]] int32_t TurnsFor(float seconds);
/// A timer set now for some seconds
[[nodiscard]] Timer Set(uint32_t turn, float seconds);
/// The seconds a timer has left, none once it has run out
[[nodiscard]] float SecondsRemaining(const Timer& timer, uint32_t turn);
/// The seconds since a timer was set
[[nodiscard]] float SecondsSinceSet(const Timer& timer, uint32_t turn);

} // namespace openblack::script::timers
