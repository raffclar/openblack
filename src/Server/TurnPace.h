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

#include <chrono>
#include <optional>

/// The pace of the frames of the game server (question MST-1, answer b). Each frame has a due time: the start of the pace
/// plus the number of frames times the frame time. After a slow frame, the server starts the next frame at once, until
/// it is on its due times again. Thus a slow turn does not lower the turn rate for good. The pace only changes the times
/// when the clients get the turns. It does not change the synced world. Pure, tested with a fake clock.
namespace openblack::server::turn_pace
{

using Clock = std::chrono::steady_clock;

/// The server makes up the time of this many frames at most. A longer stop moves the due time on, so that the server
/// does not send turns at once for a long time.
/// - A client does up to 10 turns of the server in one frame, thus it does the 5 turns of a catch-up in one frame.
/// - The time of 5 turns (500 ms) is less than the 2 s lag after which the clock of a client starts again at its turn.
/// - A stop longer than 500 ms is a pause of the game, for example a join or a snapshot. The players see the turns
///   play faster after a catch-up. Thus, the catch-up after a long stop must be short.
inline constexpr uint32_t k_MostCatchUpFrames = 5;

/// The due times of the frames of the server
class Deadline
{
public:
	/// Start the pace again: the frame that starts at this time is on its due time
	void Restart(Clock::time_point frameStart);

	/// At the end of a frame, the time when the next frame is due. A time before now tells the server to start the next
	/// frame at once. The due time is never more than k_MostCatchUpFrames frames before now.
	[[nodiscard]] Clock::time_point NextFrame(Clock::time_point now, Clock::duration frameTime);

	/// The due time of the frame that plays now, none before the start of the pace
	[[nodiscard]] std::optional<Clock::time_point> Due() const { return _due; }

private:
	std::optional<Clock::time_point> _due;
};

} // namespace openblack::server::turn_pace
