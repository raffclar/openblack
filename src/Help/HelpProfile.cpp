/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpProfile.h"

#include <algorithm>

namespace openblack::help::profile
{

namespace
{
/// A kept time's slot, steps back from the next one to be written
constexpr uint32_t SlotBack(uint32_t head, uint32_t back)
{
	return (head + k_TimesKept - back) % k_TimesKept;
}
} // namespace

void EventCount::Trigger(uint32_t clock)
{
	if (_triggeredThisTurn)
	{
		return;
	}
	++_total;
	_triggeredThisTurn = true;
	_times.at(_head) = clock;
	_head = (_head + 1) % k_TimesKept;
	_kept = std::min(_kept + 1, k_TimesKept);
}

void EventCount::EndTurn()
{
	const float happened = _triggeredThisTurn ? 1.0f : 0.0f;
	_triggeredThisTurn = false;
	_smoothedRate += (happened - _smoothedRate) * k_RateFollow;
}

void EventCount::PullTimesBackTo(uint32_t clock)
{
	std::ranges::for_each(_times, [clock](uint32_t& time) { time = std::min(time, clock); });
}

float EventCount::SecondsSince(uint32_t clock)
{
	if (_kept == 0)
	{
		return 0.0f;
	}
	const auto since = static_cast<int32_t>(clock - _times.at(SlotBack(_head, 1)));
	if (since < 0)
	{
		PullTimesBackTo(clock);
		return 0.0f;
	}
	return static_cast<float>(since) * 0.001f;
}

float EventCount::PerSecond(uint32_t clock)
{
	if (_kept < 2)
	{
		return 0.0f;
	}
	const auto since = static_cast<int32_t>(clock - _times.at(SlotBack(_head, _kept)));
	if (since < 0)
	{
		PullTimesBackTo(clock);
		return 0.0f;
	}
	if (since == 0)
	{
		return 0.0f;
	}
	return static_cast<float>((_kept - 1) * 1000) / static_cast<float>(since);
}

void HelpProfile::Trigger(uint32_t event)
{
	if (event >= k_EventCount)
	{
		return;
	}
	_counts.at(event).Trigger(_clock);
	if (event > 13 && event < 24)
	{
		_counts.at(24).Trigger(_clock);
	}
	if (event > 0 && event < 43)
	{
		_counts.at(43).Trigger(_clock);
	}
	if (event > 8 && event < 12)
	{
		_counts.at(11).Trigger(_clock);
	}
	_counts.at(k_AnyEvent).Trigger(_clock);
}

void HelpProfile::EndTurn()
{
	std::ranges::for_each(_counts, &EventCount::EndTurn);
	_clock += k_MillisecondsPerTurn;
}

} // namespace openblack::help::profile
