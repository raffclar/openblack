/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptTimers.h"

#include "Common/X87.h"

namespace openblack::script::timers
{

namespace
{
/// A second's turns, as a whole number, and a millisecond in seconds
constexpr int64_t k_TurnsPerSecond = 1000 / k_MillisecondsPerTurn;
constexpr float k_SecondsPerMillisecond = 0.001f;

int32_t TurnsSince(const Timer& timer, uint32_t turn)
{
	return static_cast<int32_t>(turn - timer.setTurn);
}

float TurnsToSeconds(int32_t turns)
{
	// The milliseconds round to a float, then the seconds do
	const float milliseconds = x87::Mul(static_cast<double>(turns), static_cast<double>(k_MillisecondsPerTurn));
	return milliseconds * k_SecondsPerMillisecond;
}
} // namespace

int32_t TurnsFor(float seconds)
{
	// The turns round to a float, then they are cut towards zero
	return static_cast<int32_t>(static_cast<int64_t>(x87::Mul(static_cast<double>(k_TurnsPerSecond), seconds)));
}

Timer Set(uint32_t turn, float seconds)
{
	return {.setTurn = turn, .turns = TurnsFor(seconds)};
}

float SecondsRemaining(const Timer& timer, uint32_t turn)
{
	const int32_t left = timer.turns - TurnsSince(timer, turn);
	return TurnsToSeconds(left < 0 ? 0 : left);
}

float SecondsSinceSet(const Timer& timer, uint32_t turn)
{
	return TurnsToSeconds(TurnsSince(timer, turn));
}

} // namespace openblack::script::timers
