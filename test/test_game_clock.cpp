/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game clock (GameClock.h): the turn scheduler (IsTurnScheduled / TurnDue), StartTurn, the frame clock of the
// game loop, Pause, SetSpeed and TicksForSeconds, driven by a fake GetTickCount with frames of 33 ms; and the paused
// turn's schedule (PausedTurnTimer), the temple's own turn while the game is paused inside the citadel.

#include <vector>

#include <gtest/gtest.h>

#include "GameClock.h"

namespace gc = openblack::game_clock;

namespace
{
uint32_t g_Now = 0;

uint32_t FakeTicks()
{
	return g_Now;
}

class GameClockTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_Now = 5000;
		gc::SetTickSource(&FakeTicks);
		gc::Reset();
		gc::SetTurn(0);
		gc::OnLoad();
		gc::Start(false);
	}
	void TearDown() override
	{
		gc::SetTickSource(nullptr);
		gc::Reset();
	}

	/// One loop of the game loop: the turns, then the frame clock; true if a turn ran
	static bool Frame(uint32_t ms)
	{
		g_Now += ms;
		bool turned = false;
		while (gc::TurnDue())
		{
			gc::StartTurn();
			turned = true;
		}
		gc::UpdateFrameClock();
		return turned;
	}
};
} // namespace

TEST_F(GameClockTest, FirstTurnRightAwayThenEveryHundredMs)
{
	// local 0 >= turn 0 * 100: the first loop already plays turn 1, and it is counted at its start
	EXPECT_TRUE(Frame(0));
	EXPECT_EQ(gc::Turn(), 1u);
	// the frame clock jumps one turn ahead: visual = 1 * 100 + 0
	EXPECT_EQ(gc::VisualMs(), 100u);
	EXPECT_EQ(gc::FrameGameMs(), 100u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.0f);
}

TEST_F(GameClockTest, ThirtyFpsKeepsTheLeftover)
{
	// 33 ms frames: with the leftover kept, 3000 ms make 30 turns (not 22, as when each turn restarts the count)
	Frame(0);
	std::vector<uint32_t> turnFrames;
	for (uint32_t i = 1; i <= 91; ++i) // 91 * 33 = 3003 ms
	{
		if (Frame(33))
		{
			turnFrames.push_back(i);
		}
	}
	EXPECT_EQ(gc::Turn(), 31u);
	ASSERT_EQ(turnFrames.size(), 30u);
	// turn 2 is due at local 100: frame 4 (132 ms), turn 3 at 200: frame 7 (231 ms), turn 4 at 300: frame 10 (330)
	EXPECT_EQ(turnFrames[0], 4u);
	EXPECT_EQ(turnFrames[1], 7u);
	EXPECT_EQ(turnFrames[2], 10u);
	// turn 5 at 400: frame 13 (429), and so on: the gaps are 3 or 4 frames, 100 ms on average
	for (size_t i = 1; i < turnFrames.size(); ++i)
	{
		const auto gap = turnFrames[i] - turnFrames[i - 1];
		EXPECT_TRUE(gap == 3u || gap == 4u) << i;
	}
}

TEST_F(GameClockTest, RemainderFractionAndFrameMs)
{
	Frame(0); // turn 1, visual 100
	Frame(33);
	EXPECT_EQ(gc::FrameGameMs(), 33u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 33 * 0.01f);
	Frame(33);
	Frame(33); // local 99, still no turn 2: the remainder is 99
	EXPECT_EQ(gc::Turn(), 1u);
	EXPECT_EQ(gc::VisualMs(), 199u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 99 * 0.01f);
	Frame(33); // local 132: turn 2; remainder 99 + 33 - 100 = 32
	EXPECT_EQ(gc::Turn(), 2u);
	EXPECT_EQ(gc::VisualMs(), 232u);
	EXPECT_EQ(gc::FrameGameMs(), 33u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 32 * 0.01f);
}

TEST_F(GameClockTest, RemainderClampedAt99AndFrameMsAtMost199)
{
	Frame(0);
	// a long frame (250 ms): one turn only (1 a frame); the remainder is clamped at 99
	EXPECT_TRUE(Frame(250));
	EXPECT_EQ(gc::Turn(), 2u);
	EXPECT_EQ(gc::VisualMs(), 299u);
	EXPECT_EQ(gc::FrameGameMs(), 199u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.99f);
	// the next frame catches up one more turn
	EXPECT_TRUE(Frame(1));
	EXPECT_EQ(gc::Turn(), 3u);
}

TEST_F(GameClockTest, MoreThanTwoSecondsBehindDropsTheLag)
{
	Frame(0);
	Frame(5000); // local 5000, target 100: reset to turn * 100 from now; still one turn this frame
	EXPECT_EQ(gc::Turn(), 2u);
	// the reset set the timer to 1 * 100 (the turn when it was reset): turn 3 waits until it reaches 200
	EXPECT_FALSE(Frame(50));
	EXPECT_TRUE(Frame(50));
	EXPECT_EQ(gc::Turn(), 3u);
}

TEST_F(GameClockTest, PauseStopsTheTimerKeepsTheFractionAndNoExtraTurn)
{
	Frame(0);
	Frame(33);
	Frame(33); // remainder 66
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.66f);
	gc::Pause(true);
	for (int i = 0; i < 100; ++i) // 3.3 s paused
	{
		EXPECT_FALSE(Frame(33));
		EXPECT_EQ(gc::FrameGameMs(), 0u);
		EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.66f); // kept, not 0
	}
	EXPECT_EQ(gc::Turn(), 1u);
	gc::Pause(false);
	// the paused time does not count: the timer goes on from 66
	EXPECT_FALSE(Frame(0));
	EXPECT_FALSE(Frame(33)); // 99
	EXPECT_EQ(gc::FrameGameMs(), 33u);
	EXPECT_TRUE(Frame(33)); // 132: turn 2
	EXPECT_EQ(gc::Turn(), 2u);
}

TEST_F(GameClockTest, SpeedRebasesTheTimer)
{
	Frame(0);
	Frame(50); // local 50 at speed 1
	gc::SetSpeed(2.0f);
	EXPECT_FLOAT_EQ(gc::Speed(), 2.0f);
	// the 50 ms gone keep their speed: 25 real ms more make local 100
	EXPECT_FALSE(Frame(24));
	EXPECT_TRUE(Frame(1));
	EXPECT_EQ(gc::Turn(), 2u);
	// the frame's game ms follow the speed too
	Frame(10);
	EXPECT_EQ(gc::FrameGameMs(), 20u);
	// paused, the speed is only kept and comes back with the unpause
	gc::Pause(true);
	gc::SetSpeed(0.5f);
	gc::Pause(false);
	Frame(0);
	Frame(40);
	EXPECT_EQ(gc::FrameGameMs(), 20u);
}

TEST_F(GameClockTest, OnLoadSetsTheVisualClockToTheTurn)
{
	gc::SetTurn(1234);
	gc::OnLoad();
	EXPECT_EQ(gc::VisualMs(), 123400u);
	EXPECT_EQ(gc::FrameGameMs(), 0u);
	EXPECT_FLOAT_EQ(gc::TurnFraction(), 0.0f);
	gc::Start(false);
	// ResetLocalTimer: the timer at 1234 * 100, so a turn is due right away
	EXPECT_TRUE(Frame(0));
	EXPECT_EQ(gc::Turn(), 1235u);
}

TEST_F(GameClockTest, TicksForSeconds)
{
	// TicksForSeconds: trunc(1000 / 100 * s)
	EXPECT_EQ(gc::TicksForSeconds(1.0f), 10);
	EXPECT_EQ(gc::TicksForSeconds(2.55f), 25);
	EXPECT_EQ(gc::TicksForSeconds(0.09f), 0);
	// SET_GAME_TICK_TIME changes the logic's ms per turn (integer division: 1000 / 300 = 3)
	gc::SetMsPerTurn(300);
	EXPECT_EQ(gc::MsPerTurn(), 300u);
	EXPECT_EQ(gc::TicksForSeconds(2.0f), 6);
}

TEST_F(GameClockTest, RealClockAndSelectors)
{
	// the engine timer is stopped until the renderer's initialisation starts it: 0 and 1 ms frames
	g_Now += 20;
	EXPECT_EQ(gc::EngineMs(), 0);
	gc::UpdateRealClock();
	g_Now += 20;
	gc::UpdateRealClock();
	EXPECT_EQ(gc::FrameRealMs(), 1u);
	gc::StartEngineTimer();
	const uint32_t started = g_Now;
	gc::UpdateRealClock();
	g_Now += 40;
	gc::UpdateRealClock();
	EXPECT_EQ(gc::FrameRealMs(), 40u);
	gc::UpdateRealClock(); // same tick: at least 1
	EXPECT_EQ(gc::FrameRealMs(), 1u);
	g_Now += 900;
	gc::UpdateRealClock();
	EXPECT_EQ(gc::CameraFrameMs(), 900u);
	EXPECT_EQ(gc::ClampedFrameMs(true), 500u);
	// started, it runs at speed 1 from about 0 (elapsed = trunc(40 ms stopped * 1e-5) = 0): ms since the start
	EXPECT_EQ(gc::EngineMs(), static_cast<int32_t>(g_Now - started));
	Frame(0);
	EXPECT_EQ(gc::CameraFrameMs(true), gc::FrameGameMs());
	EXPECT_EQ(gc::ClampedFrameMs(), gc::FrameGameMs());
	EXPECT_EQ(gc::FrameGameMs(), 199u);
}

TEST_F(GameClockTest, EngineTimerAfterLongUptime)
{
	// GetTickCount after 40 days of uptime: the engine timer counts from its start, so its ms are whole
	// (as float, the raw ticks would round to 256 ms and overflow the int32)
	g_Now = 0xCE000000u;
	gc::Reset();
	g_Now += 3000;
	gc::StartEngineTimer();
	g_Now += 12345;
	EXPECT_EQ(gc::EngineMs(), 12345);
	gc::UpdateRealClock();
	g_Now += 17;
	gc::UpdateRealClock();
	EXPECT_EQ(gc::FrameRealMs(), 17u);
}

TEST(PausedTurnTimer, EveryHundredMsFromTheFirstAsk)
{
	gc::PausedTurnTimer timer;
	// the first ask starts the count: nothing is due yet, nor at exactly 100 ms
	EXPECT_FALSE(timer.Due(1000));
	EXPECT_EQ(timer.last, 1000u);
	EXPECT_FALSE(timer.Due(1100));
	// past 100 ms a turn, and the count moves on by 100, not to now: no time is lost between turns
	EXPECT_TRUE(timer.Due(1133));
	EXPECT_EQ(timer.last, 1100u);
	EXPECT_FALSE(timer.Due(1166));
	EXPECT_FALSE(timer.Due(1200));
	EXPECT_TRUE(timer.Due(1233));
	EXPECT_EQ(timer.last, 1200u);
	// one turn a call at most: 33 ms frames give 10 turns in a second
	int turns = 0;
	for (uint32_t now = 1266; now <= 2266; now += 33)
	{
		turns += timer.Due(now) ? 1 : 0;
	}
	EXPECT_EQ(turns, 10);
}

TEST(PausedTurnTimer, MoreThanTwoHundredBehindStartsAgainFromNow)
{
	gc::PausedTurnTimer timer;
	EXPECT_FALSE(timer.Due(5000));
	// a long gap, as when the player comes back into the temple later: one turn, and the count is now
	EXPECT_TRUE(timer.Due(9000));
	EXPECT_EQ(timer.last, 9000u);
	EXPECT_FALSE(timer.Due(9100));
	EXPECT_TRUE(timer.Due(9101));
	// 200 behind after moving on is not more than 200: the count stays 100 on
	EXPECT_TRUE(timer.Due(9400));
	EXPECT_EQ(timer.last, 9200u);
	EXPECT_TRUE(timer.Due(9401));
	EXPECT_EQ(timer.last, 9300u);
	// 201 behind after moving on: the count is now
	EXPECT_TRUE(timer.Due(9701));
	EXPECT_EQ(timer.last, 9701u);
}

TEST(PausedTurnTimer, TheTicksMayWrap)
{
	gc::PausedTurnTimer timer;
	EXPECT_FALSE(timer.Due(0xFFFFFFF0u));
	EXPECT_FALSE(timer.Due(0x50u)); // 96 ms later
	EXPECT_TRUE(timer.Due(0x60u));  // 112 ms later
	EXPECT_EQ(timer.last, 0x54u);
}
