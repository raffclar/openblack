/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DayNightClock.h"

#include <cmath>

#include <algorithm>

#include "GameClock.h"
#include "SkyType.h"

using namespace openblack;

namespace
{
// Game turn length in seconds, as the game turn passes it
constexpr float k_TurnSeconds = game_clock::k_TurnSeconds;

float WrapHours(float t)
{
	while (t < 0.0f)
	{
		t += 24.0f;
	}
	while (t >= 24.0f)
	{
		t -= 24.0f;
	}
	return t;
}

/// Piecewise-linear map of the half day [0, 12] with thresholds `from` onto `to`, mirrored at 12
float MapHours(float t, const std::array<float, 4>& from, const std::array<float, 4>& to)
{
	const bool mirrored = t > 12.0f;
	if (mirrored)
	{
		t = 24.0f - t;
	}
	float lo0 = 0.0f;
	float lo1 = 0.0f;
	float hi0 = 12.0f;
	float hi1 = 12.0f;
	size_t i = 0;
	while (i < from.size() && !(t < from[i]))
	{
		++i;
	}
	if (i > 0)
	{
		lo0 = from[i - 1];
		lo1 = to[i - 1];
	}
	if (i < from.size())
	{
		hi0 = from[i];
		hi1 = to[i];
	}
	float r = (t - lo0) / (hi0 - lo0) * (hi1 - lo1) + lo1;
	return mirrored ? 24.0f - r : r;
}
} // namespace

void DayNightClock::Reset()
{
	// as a land opens: scale 1, the default cycle, the time forced to 12
	_scale = 1.0f;
	_moveSeconds = 0.0f;
	SetCycle(k_DefaultDuration, k_DefaultNight, k_DefaultChange);
	SetScriptTime(12.0f);
}

void DayNightClock::SetCycle(float duration, float night, float change)
{
	// trunc(duration * 0.41666666f) (10 / 24 in tenths of a second). The product stays in the FPU register before the
	// truncation; with the FPU at single precision that is the float product taken here, with double precision
	// durations that are multiples of 2.4 would truncate one lower.
	const auto tenths = static_cast<int>(duration * 0.41666666f);
	// both rates = 10 / n (0 for n = 0), the day and night rates are the same
	_dayRate = tenths != 0 ? 10.0f / static_cast<float>(tenths) : 0.0f;
	_nightRate = _dayRate;

	// N = night 12, E = 12 change + N (kept as a float), c = (E - N) * 0.25 limited to N, the thresholds
	// (N - c, c + N, E - c, E + c)
	const float halfNight = night * 12.0f;
	const float end = change * 12.0f + halfNight;
	const float ramp = std::min((end - halfNight) * 0.25f, halfNight);
	_times = {halfNight - ramp, ramp + halfNight, end - ramp, end + ramp};
	sky_type::SetThresholds(_times[0], _times[1], _times[2], _times[3]);
}

void DayNightClock::SetCycleFromLand(float duration, float night, float change)
{
	night = std::min(night, 1.0f);
	change = std::min(change, 1.0f - night);
	SetCycle(duration, night, change);
}

void DayNightClock::SetScriptTime(float hour)
{
	_moveSeconds = 0.0f;
	SetTarget(ScriptToVisual(hour), 0.0f);
	_visualTime = _target;
	// the sky type is sampled at once and the dome rebuilt whole. The original's following call is not sky type and is
	// not done here.
	sky_type::Jump(_visualTime);
}

void DayNightClock::MoveScriptTime(float hour, float seconds)
{
	SetTarget(ScriptToVisual(hour), seconds);
}

void DayNightClock::SetTarget(float hour, float seconds)
{
	if (hour < -1000.0f || hour > 1000.0f)
	{
		hour = 0.0f;
	}
	_target = WrapHours(hour);
	if (seconds != 0.0f)
	{
		_moveSeconds = seconds;
		float diff = std::abs(_visualTime - _target);
		if (diff > 12.0f)
		{
			diff -= 24.0f;
		}
		_step = std::abs(diff) / seconds;
	}
}

void DayNightClock::ProcessTurn()
{
	const float gameSeconds = _scale * k_TurnSeconds;
	if (_moveSeconds != 0.0f)
	{
		if (_visualTime == _target)
		{
			_moveSeconds = 0.0f;
		}
	}
	else
	{
		_step = 2.5f;
		const float rate = (_nightRate - _dayRate) * SkyType() * 0.5f + _dayRate;
		SetTarget(_visualTime + rate * gameSeconds, 0.0f);
	}

	// Slide towards the target the short way round, at most _step hours per second of turn
	float target = _target;
	if (_visualTime - target > 12.0f)
	{
		target += 24.0f;
	}
	if (_visualTime - target < -12.0f)
	{
		target -= 24.0f;
	}
	const float step = _step * k_TurnSeconds;
	if (target < _visualTime)
	{
		_visualTime = std::max(_visualTime - step, target);
	}
	else if (target > _visualTime)
	{
		_visualTime = std::min(_visualTime + step, target);
	}
	_visualTime = WrapHours(_visualTime);
}

float DayNightClock::ScriptToVisual(float hour) const
{
	return MapHours(hour, k_ScriptTimes, _times);
}

float DayNightClock::VisualToScript(float hour) const
{
	return MapHours(hour, _times, k_ScriptTimes);
}

float DayNightClock::SkyType(float hour) const
{
	// on this clock's copy of the thresholds (the same as the sky's after SetCycle)
	return sky_type::At(hour, _times);
}

bool DayNightClock::IsVisualNight() const
{
	return sky_type::IsVisualNight(SkyType());
}
