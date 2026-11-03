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

std::optional<uint32_t> DispenserTurns(float seconds)
{
	// A time before now gives so many turns that the dispenser never makes another bubble
	const auto turns = TurnsFor(seconds);
	if (turns == 0)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(turns);
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

void CountdownTimer::Start(float seconds)
{
	_running = true;
	_visible = true;
	// The turns round to a float, then they are cut towards zero. The low 32 bits of the whole number stay.
	const float turns = x87::Mul(static_cast<double>(k_TurnsPerSecond), seconds < 0.0f ? 0.0f : seconds);
	_turnsLeft = static_cast<uint32_t>(x87::ToInteger(turns));
}

void CountdownTimer::ProcessTurn()
{
	if (_running)
	{
		--_turnsLeft;
		if (_turnsLeft == 0)
		{
			_running = false;
		}
	}
}

void CountdownTimer::Reset()
{
	_running = false;
	_visible = false;
}

float CountdownTimer::GetSecondsLeft() const
{
	return static_cast<float>(_turnsLeft / static_cast<uint32_t>(k_TurnsPerSecond));
}

} // namespace openblack::script::timers
