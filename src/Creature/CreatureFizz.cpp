/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFizz.h"

namespace openblack::creature_fizz
{

namespace
{
/// Milliseconds to seconds, as the game's single-precision constant
constexpr float k_SecondsPerMillisecond = 0.001f;
} // namespace

Set SetFizz(const Fizz& fizz, float target, float seconds, bool goesForGood)
{
	if (fizz.goesForGood)
	{
		return {.fizz = fizz, .sound = false};
	}
	// Only a target of exactly right out of sight sounds, before it is kept within bounds
	const bool sound = target == 1.0f;
	auto next = fizz;
	next.goesForGood = goesForGood;
	next.target = target <= 0.0f ? 0.0f : (target >= 1.0f ? 1.0f : target);
	if (seconds != 0.0f && next.target != fizz.now)
	{
		const float rate = 1.0f / seconds;
		next.perSecond = next.target - fizz.now > 0.0f ? rate : -rate;
	}
	else
	{
		next.perSecond = 0.0f;
		next.now = next.target;
	}
	return {.fizz = next, .sound = sound};
}

Fizz Step(const Fizz& fizz, uint32_t turnMilliseconds)
{
	if (fizz.perSecond == 0.0f)
	{
		return fizz;
	}
	auto next = fizz;
	// Worked in double precision and kept as a float, as the game does
	next.now =
	    static_cast<float>((static_cast<double>(turnMilliseconds) * fizz.perSecond * k_SecondsPerMillisecond) + fizz.now);
	// It stops at the target only once past it
	const bool past = fizz.perSecond > 0.0f ? next.now > fizz.target : next.now < fizz.target;
	if (past)
	{
		next.now = fizz.target;
		next.perSecond = 0.0f;
	}
	return next;
}

bool Gone(const Fizz& fizz)
{
	return fizz.goesForGood && fizz.now == 1.0f;
}

bool Settled(const Fizz& fizz)
{
	return !fizz.goesForGood && fizz.now == 0.0f && fizz.perSecond == 0.0f;
}

uint16_t TurnsOf(float seconds, uint32_t turnMilliseconds)
{
	if (turnMilliseconds == 0)
	{
		return 0;
	}
	const auto turnsPerSecond = 1000 / turnMilliseconds;
	return static_cast<uint16_t>(static_cast<int64_t>(static_cast<double>(turnsPerSecond) * seconds));
}

} // namespace openblack::creature_fizz
