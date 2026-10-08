/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RampedStops.h"

#include <algorithm>
#include <utility>

using namespace openblack::audio;

void RampedStops::Begin(size_t channel, SourceId source, float gain, Clock::time_point now, Target& target)
{
	target.SetGain(source, StepGain(gain, 1));
	_fades.push_back({.channel = channel, .source = source, .gain = gain, .start = now, .step = 1});
}

std::optional<RampedStops::Clock::time_point> RampedStops::Advance(Clock::time_point now, Target& target)
{
	std::optional<Clock::time_point> next;
	std::erase_if(_fades, [&](Fade& fade) {
		const auto elapsed = now - fade.start;
		if (elapsed >= k_Ramp)
		{
			target.Flush(fade.source);
			_spares.push_back(fade.source);
			return true;
		}
		const int due = 1 + static_cast<int>(elapsed / k_StepLength);
		if (due > fade.step)
		{
			fade.step = due;
			target.SetGain(fade.source, StepGain(fade.gain, due));
		}
		// the next step, or the flush after the last one
		const auto at = fade.start + k_StepLength * fade.step;
		next = next ? std::min(*next, at) : at;
		return false;
	});
	return next;
}

void RampedStops::Cut(size_t channel, Target& target)
{
	std::erase_if(_fades, [&](const Fade& fade) {
		if (fade.channel != channel)
		{
			return false;
		}
		target.Flush(fade.source);
		_spares.push_back(fade.source);
		return true;
	});
}

std::optional<RampedStops::SourceId> RampedStops::TakeSpare()
{
	if (_spares.empty())
	{
		return std::nullopt;
	}
	const auto source = _spares.back();
	_spares.pop_back();
	return source;
}

std::vector<RampedStops::SourceId> RampedStops::TakeAll()
{
	auto sources = std::exchange(_spares, {});
	for (const auto& fade : _fades)
	{
		sources.push_back(fade.source);
	}
	_fades.clear();
	return sources;
}
