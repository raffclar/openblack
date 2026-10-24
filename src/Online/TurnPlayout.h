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

#include <deque>
#include <map>
#include <optional>

namespace openblack::online
{

/// The input buffer of a client (MI-1): when a client does each turn that the server sent. The server sends one turn
/// each 100 ms. The client does turn n at the time of the earliest line of arrivals plus n turns plus a buffer. The buffer
/// follows the jitter of the arrivals that the client measures, so a client on a steady network waits less than one on a
/// network that changes. A turn always waits until it arrived.
class TurnPlayout
{
public:
	/// The time between two turns of the server
	static constexpr int64_t k_TurnMicroseconds = 100'000;
	/// The buffer is this many times the measured jitter
	static constexpr int64_t k_JitterFactor = 4;
	/// The smallest and the largest buffer
	static constexpr int64_t k_LeastBufferMicroseconds = 0;
	static constexpr int64_t k_MostBufferMicroseconds = 1'000'000;
	/// The jitter takes this part of each new difference (1/16, as RTP's jitter)
	static constexpr int64_t k_JitterShare = 16;
	/// The earliest line comes from this many of the last arrivals
	static constexpr size_t k_LineArrivals = 64;

	/// A turn arrived at a time in microseconds of the client's clock
	void Arrived(uint32_t turn, int64_t now);
	/// True when the turn arrived and its time came
	[[nodiscard]] bool Due(uint32_t turn, int64_t now) const;
	/// The client did the turn: its arrival is no longer needed
	void Done(uint32_t turn);
	/// Forget everything, as for a new connection
	void Reset();

	[[nodiscard]] int64_t JitterMicroseconds() const { return _jitter; }
	[[nodiscard]] int64_t BufferMicroseconds() const;
	/// The time when the turn is due, once a turn arrived
	[[nodiscard]] std::optional<int64_t> DueTime(uint32_t turn) const;

private:
	/// The arrivals of the turns that the client did not do yet
	std::map<uint32_t, int64_t> _waiting;
	/// The arrival minus the turn's place on the line, of the last arrivals
	std::deque<int64_t> _offsets;
	std::optional<std::pair<uint32_t, int64_t>> _last;
	int64_t _jitter {0};
};

} // namespace openblack::online
