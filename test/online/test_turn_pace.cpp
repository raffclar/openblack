/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <functional>
#include <vector>

#include <gtest/gtest.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "Common/PreciseWait.h"
#include "Online/TurnPlayout.h"
#include "Server/TurnPace.h"

using namespace std::chrono_literals;
using openblack::online::TurnPlayout;
using openblack::server::turn_pace::Clock;
using openblack::server::turn_pace::Deadline;
using openblack::server::turn_pace::k_MostCatchUpFrames;

namespace
{

constexpr Clock::duration k_Frame = 100ms;
const Clock::time_point k_Start {10s};

/// The frames of a fake server: when each frame started, with the work of each frame from a function
std::vector<Clock::time_point> PlayFrames(uint32_t frames, const std::function<Clock::duration(uint32_t)>& work)
{
	Deadline pace;
	pace.Restart(k_Start);
	std::vector<Clock::time_point> starts;
	auto now = k_Start;
	for (uint32_t frame = 0; frame < frames; ++frame)
	{
		starts.push_back(now);
		now += work(frame);
		// The server waits for the due time, or starts at once when it is late
		now = std::max(now, pace.NextFrame(now, k_Frame));
	}
	return starts;
}

} // namespace

TEST(TurnPace, ASteadyServerStartsEachFrameOnItsDueTime)
{
	const auto starts = PlayFrames(1000, [](uint32_t) { return 20ms; });
	for (uint32_t frame = 0; frame < starts.size(); ++frame)
	{
		EXPECT_EQ(starts[frame], k_Start + k_Frame * frame) << frame;
	}
}

TEST(TurnPace, TheWorkOfAFrameDoesNotMoveTheDueTimes)
{
	// The work changes from frame to frame, but it is always shorter than the frame
	const auto starts = PlayFrames(1000, [](uint32_t frame) { return std::chrono::milliseconds(frame % 97); });
	EXPECT_EQ(starts.back(), k_Start + k_Frame * 999);
}

TEST(TurnPace, TheServerMakesUpTheTimeOfASlowFrame)
{
	// Frame 50 takes 320 ms: the frames after it start at once until they are on their due times again
	const auto starts = PlayFrames(100, [](uint32_t frame) { return frame == 50 ? 320ms : 20ms; });
	EXPECT_EQ(starts[51], k_Start + 5000ms + 320ms);
	EXPECT_EQ(starts[52], starts[51] + 20ms);
	EXPECT_EQ(starts[53], starts[52] + 20ms);
	// Frame 54 is on its due time again: the server lost no time
	EXPECT_EQ(starts[54], k_Start + k_Frame * 54);
	EXPECT_EQ(starts[55], k_Start + k_Frame * 55);
	EXPECT_EQ(starts[99], k_Start + k_Frame * 99);
}

TEST(TurnPace, AServerSlowerThanItsPaceStartsEachFrameAtOnce)
{
	const auto starts = PlayFrames(100, [](uint32_t) { return 150ms; });
	for (uint32_t frame = 1; frame < starts.size(); ++frame)
	{
		EXPECT_EQ(starts[frame] - starts[frame - 1], 150ms) << frame;
	}
}

TEST(TurnPace, TheServerMakesUpTheTimeOfAFewFramesOnly)
{
	// A stop of 2 s in frame 50: the server makes up 5 frames and then keeps the pace from there
	const auto starts = PlayFrames(100, [](uint32_t frame) { return frame == 50 ? 2000ms : 20ms; });
	const auto afterStop = k_Start + 5000ms + 2000ms;
	EXPECT_EQ(starts[51], afterStop);
	// The due time of frame 51 is 5 frames before the end of the stop
	const auto due51 = afterStop - k_Frame * k_MostCatchUpFrames;
	// Each frame of 20 ms makes up 80 ms, so the 500 ms take 7 frames
	for (uint32_t frame = 52; frame < 58; ++frame)
	{
		EXPECT_EQ(starts[frame], starts[frame - 1] + 20ms) << frame;
	}
	EXPECT_EQ(starts[58], due51 + k_Frame * 7);
	EXPECT_EQ(starts[99], due51 + k_Frame * 48);
}

TEST(TurnPace, ARestartPutsTheDueTimeAtTheFrameStart)
{
	Deadline pace;
	EXPECT_FALSE(pace.Due().has_value());
	// With no start, the frame that ends now was on its due time
	EXPECT_EQ(pace.NextFrame(k_Start, k_Frame), k_Start + k_Frame);
	pace.Restart(k_Start + 5s);
	EXPECT_EQ(pace.Due(), k_Start + 5s);
	EXPECT_EQ(pace.NextFrame(k_Start + 5s + 30ms, k_Frame), k_Start + 5s + k_Frame);
}

TEST(TurnPace, AClientDoesTheTurnsOfACatchUpAndItsBufferComesBack)
{
	// The turns of a server with a slow frame of 320 ms arrive at a client after 30 ms
	const auto starts = PlayFrames(200, [](uint32_t frame) { return frame == 50 ? 320ms : 20ms; });
	const auto micros = [](Clock::time_point time) {
		return std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count();
	};
	TurnPlayout playout;
	int64_t mostBuffer = 0;
	for (uint32_t turn = 0; turn < starts.size(); ++turn)
	{
		const auto arrival = micros(starts[turn] + 30ms);
		playout.Arrived(turn, arrival);
		mostBuffer = std::max(mostBuffer, playout.BufferMicroseconds());
		// A turn on its due time is due when it arrives. The late turns of the catch-up are due when they arrive too,
		// because their due time on the steady line is before their arrival.
		if (turn < 51 || turn > 53)
		{
			continue;
		}
		const auto due = playout.DueTime(turn);
		ASSERT_TRUE(due.has_value());
		EXPECT_LT(*due, arrival + playout.BufferMicroseconds()) << turn;
		EXPECT_TRUE(playout.Due(turn, arrival + playout.BufferMicroseconds())) << turn;
		playout.Done(turn);
	}
	// The buffer that the late frame and the catch-up give is short, and it goes back to zero
	EXPECT_LT(mostBuffer, 200'000);
	EXPECT_LT(playout.BufferMicroseconds(), 1'000);
}

TEST(PreciseWait, TheWaitEndsAtItsTimeAndNotBefore)
{
#if defined(_WIN32)
	// A thread of normal priority waits for a time slice when every core is busy, and the wait goes past its time by that
	// slice. The test thread has a high priority for the test, so that a machine with a high load does not change the
	// result. The priority goes back at the end.
	struct PriorityScope
	{
		int previous {GetThreadPriority(GetCurrentThread())};
		PriorityScope() { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL); }
		~PriorityScope() { SetThreadPriority(GetCurrentThread(), previous); }
	} priority;
#endif
	openblack::precise_wait::Waiter waiter;
	// A sleep of the thread on Windows goes past its time by up to the period of the system timer. The waiter must not.
	// A machine with a high load can take the thread away for a whole time slice, so a round with a high median is not a
	// fault of the waiter. A fault makes every round late. Thus the test passes when one round of a few has a median
	// under 1 ms. No wait ends before its time in any round.
	constexpr int k_MostRounds = 5;
	constexpr int k_WaitsInRound = 21;
	Clock::duration bestMedian = Clock::duration::max();
	for (int round = 0; round < k_MostRounds && bestMedian >= 1ms; ++round)
	{
		std::vector<Clock::duration> late;
		for (int wait = 0; wait < k_WaitsInRound; ++wait)
		{
			const auto due = Clock::now() + 7ms;
			waiter.WaitUntil(due);
			const auto end = Clock::now();
			EXPECT_GE(end, due);
			late.push_back(end - due);
		}
		std::ranges::nth_element(late, late.begin() + k_WaitsInRound / 2);
		bestMedian = std::min(bestMedian, late[k_WaitsInRound / 2]);
	}
	EXPECT_LT(bestMedian, 1ms);
	// A time in the past gives no wait
	const auto before = Clock::now();
	waiter.WaitUntil(before - 1s);
	EXPECT_LT(Clock::now() - before, 1ms);
}
