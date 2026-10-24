/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TurnPlayout.h"

#include <cstdlib>

#include <algorithm>

namespace openblack::online
{

void TurnPlayout::Arrived(uint32_t turn, int64_t now)
{
	_waiting[turn] = now;
	if (_last.has_value() && turn > _last->first)
	{
		// The difference from the time that the server's steady pace gives
		const auto expected = static_cast<int64_t>(turn - _last->first) * k_TurnMicroseconds;
		const auto difference = std::llabs((now - _last->second) - expected);
		_jitter += (difference - _jitter) / k_JitterShare;
	}
	if (!_last.has_value() || turn > _last->first)
	{
		_last = std::pair {turn, now};
	}
	_offsets.push_back(now - static_cast<int64_t>(turn) * k_TurnMicroseconds);
	while (_offsets.size() > k_LineArrivals)
	{
		_offsets.pop_front();
	}
}

int64_t TurnPlayout::BufferMicroseconds() const
{
	return std::clamp(_jitter * k_JitterFactor, k_LeastBufferMicroseconds, k_MostBufferMicroseconds);
}

std::optional<int64_t> TurnPlayout::DueTime(uint32_t turn) const
{
	if (_offsets.empty())
	{
		return std::nullopt;
	}
	const auto line = *std::ranges::min_element(_offsets);
	return line + static_cast<int64_t>(turn) * k_TurnMicroseconds + BufferMicroseconds();
}

bool TurnPlayout::Due(uint32_t turn, int64_t now) const
{
	if (!_waiting.contains(turn))
	{
		return false;
	}
	const auto due = DueTime(turn);
	return due.has_value() && now >= *due;
}

void TurnPlayout::Done(uint32_t turn)
{
	_waiting.erase(_waiting.begin(), _waiting.upper_bound(turn));
}

void TurnPlayout::Reset()
{
	_waiting.clear();
	_offsets.clear();
	_last.reset();
	_jitter = 0;
}

} // namespace openblack::online
