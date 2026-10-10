/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DanceRules.h"

using namespace openblack::ecs;
using openblack::ecs::components::Dance;

namespace
{
/// The fastest rate, at a speed of 1
constexpr float k_MostRate = 4.0f;
/// A loop length is this many half seconds long
constexpr uint32_t k_HalfSecondsPerLoopLength = 120;
} // namespace

float dance_rules::RateForSpeed(float speed)
{
	// Rounded towards zero to tenths
	const auto tenths = static_cast<int32_t>(speed * 10.0f);
	return static_cast<float>(tenths) * 0.1f * k_MostRate;
}

void dance_rules::SetSpeed(Dance& dance, float speed)
{
	dance.rate = RateForSpeed(speed);
	if (dance.state == Dance::State::Dancing && dance.rate != dance.dancingRate)
	{
		dance.dancingRate = dance.rate;
		dance.clock = 0.0f;
	}
	dance.speed = speed;
}

void dance_rules::SetWorshipSpeed(Dance& dance, float intensity)
{
	if (intensity > 0.0f && dance.state == Dance::State::Stopped)
	{
		dance.state = Dance::State::Dancing;
	}
	else if (intensity <= 0.0f && dance.state == Dance::State::Dancing)
	{
		dance.state = Dance::State::Stopped;
	}
	SetSpeed(dance, intensity);
}

bool dance_rules::HasProperlyStarted(Dance& dance, uint32_t turn)
{
	if (dance.dancers == 0)
	{
		return false;
	}
	if (!dance.firstDancerTurn.has_value())
	{
		dance.firstDancerTurn = turn;
	}
	return dance.dancers > dance.onTheirWay / 2 || (turn - *dance.firstDancerTurn) / k_TurnsPerSecond >= k_LongestWaitSeconds;
}

uint32_t dance_rules::LoopTurns(const Dance& dance)
{
	return k_TurnsPerSecond / 2 * dance.loopLength * k_HalfSecondsPerLoopLength;
}

void dance_rules::ProcessTurn(Dance& dance, bool startsAutomatically, uint32_t turn)
{
	if (dance.state == Dance::State::Stopped && startsAutomatically && HasProperlyStarted(dance, turn))
	{
		dance.state = Dance::State::Dancing;
		dance.startTurn = turn;
	}
	else if (dance.duration > 0 && turn - dance.startTurn > dance.duration)
	{
		dance.state = Dance::State::Stopped;
	}
	if (dance.state != Dance::State::Dancing || dance.dancers == 0)
	{
		return;
	}
	// The groups move as the dance file has them at this time on the clock (not followed yet)
	dance.clock += 1.0f;
	if (static_cast<uint32_t>(dance.clock) >= LoopTurns(dance))
	{
		dance.clock = 0.0f;
	}
}
