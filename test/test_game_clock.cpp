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
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <ECS/Systems/Implementations/TimeSystem.h>

using openblack::ecs::systems::TimeSystem;

namespace
{
struct Clock
{
	uint32_t now = 1000;
	TimeSystem time {[this] { return now; }};

	/// Plays a frame: the turn if one is due, then the frame clock. Whether it played a turn
	bool Frame()
	{
		const auto played = time.IsTurnDue();
		if (played)
		{
			time.StartTurn();
		}
		time.UpdateFrame();
		return played;
	}
};
} // namespace

TEST(GameClock, TurnsFollowTheTimer)
{
	Clock clock;
	clock.time.StartGameClock(false);
	// turn 0 is due at once, and turn 1 after 100 ms
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 1);
	clock.now += 99;
	EXPECT_FALSE(clock.Frame());
	clock.now += 1;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 2);
}

TEST(GameClock, NoTurnLosesItsLeftover)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	// Frames 150 ms apart still play a turn every 100 ms on average
	int turns = 0;
	for (int i = 0; i < 20; ++i)
	{
		clock.now += 150;
		turns += clock.Frame() ? 1 : 0;
		clock.now += 1;
		turns += clock.Frame() ? 1 : 0;
	}
	EXPECT_EQ(clock.time.GetTurn(), 1 + turns);
	EXPECT_EQ(turns, 30);
}

TEST(GameClock, OneTurnAFrame)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	clock.now += 500;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 2);
}

TEST(GameClock, FarBehindGivesUpTheLostTime)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	clock.now += 10000;
	EXPECT_TRUE(clock.Frame());
	// Back to the turn played: one more turn after another 100 ms, not a hundred to catch up
	clock.now += 50;
	EXPECT_FALSE(clock.Frame());
	clock.now += 50;
	EXPECT_TRUE(clock.Frame());
	clock.now += 1;
	EXPECT_FALSE(clock.Frame());
}

TEST(GameClock, PausingStopsTheClock)
{
	Clock clock;
	clock.time.StartGameClock(true);
	clock.now += 1000;
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 0);
	clock.time.SetPaused(false);
	EXPECT_TRUE(clock.Frame());
	clock.now += 50;
	clock.Frame();
	clock.time.SetPaused(true);
	clock.now += 5000;
	clock.time.SetPaused(false);
	// The paused time doesn't count: the next turn is 50 ms on
	clock.now += 49;
	EXPECT_FALSE(clock.Frame());
	clock.now += 1;
	EXPECT_TRUE(clock.Frame());
}

TEST(GameClock, SpeedScalesTheTimer)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	clock.time.SetSpeed(2.0f);
	clock.now += 50;
	EXPECT_TRUE(clock.Frame());
	clock.time.SetSpeed(0.5f);
	clock.now += 199;
	EXPECT_FALSE(clock.Frame());
	clock.now += 1;
	EXPECT_TRUE(clock.Frame());
}

TEST(GameClock, TheFrameClockFollowsTheTurn)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	clock.now += 30;
	clock.Frame();
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 30);
	EXPECT_FLOAT_EQ(clock.time.GetTurnFraction(), 0.3f);
	clock.now += 30;
	clock.Frame();
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 30);
	EXPECT_FLOAT_EQ(clock.time.GetTurnFraction(), 0.6f);
	// The next turn starts the fraction again
	clock.now += 40;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 40);
	EXPECT_FLOAT_EQ(clock.time.GetTurnFraction(), 0.0f);
}

namespace
{
/// Plays turns until the given one, 100 ms apart
void PlayTo(Clock& clock, uint32_t turn)
{
	clock.time.StartGameClock(false);
	clock.Frame();
	while (clock.time.GetTurn() < turn)
	{
		clock.now += 100;
		clock.Frame();
	}
}

/// The story's next land loads during a turn, taking this long, and the frame ends
void LoadNextLandInATurn(Clock& clock, uint32_t loadMs)
{
	clock.now += 100;
	ASSERT_TRUE(clock.time.IsTurnDue());
	clock.time.StartTurn();
	clock.time.RestartTurns();
	clock.now += loadMs;
	clock.time.UpdateFrame();
}
} // namespace

TEST(GameClock, TheStorysNextLandRestartsTheTurnsAndTheClockRunsOn)
{
	Clock clock;
	PlayTo(clock, 50);
	LoadNextLandInATurn(clock, 3000);
	EXPECT_EQ(clock.time.GetTurn(), 0);
	// The frame clock went back with the turn: the frame of the load takes no time
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 0);
	// The clock is far ahead of turn 0: the next turn plays at once and gives that time up
	clock.now += 16;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 1);
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 1);
	clock.now += 99;
	EXPECT_FALSE(clock.Frame());
	clock.now += 1;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 2);
}

TEST(GameClock, AClockLessThanTwoSecondsAheadOfTheRestartedTurnsCatchesUpATurnAFrame)
{
	Clock clock;
	PlayTo(clock, 5);
	// The clock is at 500 ms as the land starts to load and 1500 ms after: turns 1 to 16 play, one a frame
	LoadNextLandInATurn(clock, 1000);
	for (uint32_t turn = 1; turn <= 16; ++turn)
	{
		clock.now += 1;
		EXPECT_TRUE(clock.Frame());
		EXPECT_EQ(clock.time.GetTurn(), turn);
	}
	clock.now += 1;
	EXPECT_FALSE(clock.Frame());
}

TEST(GameClock, TheStorysNextLandKeepsTheSpeedAndThePause)
{
	Clock clock;
	PlayTo(clock, 5);
	clock.time.SetSpeed(2.0f);
	LoadNextLandInATurn(clock, 0);
	EXPECT_FLOAT_EQ(clock.time.GetSpeed(), 2.0f);
	EXPECT_FALSE(clock.time.IsPaused());
	clock.time.SetPaused(true);
	clock.time.RestartTurns();
	EXPECT_TRUE(clock.time.IsPaused());
	EXPECT_FLOAT_EQ(clock.time.GetSpeed(), 2.0f);
}

TEST(GameClock, ANewLandsClockStartsAtTurnZeroAndItsFirstFrameTakesTheTurn)
{
	Clock clock;
	PlayTo(clock, 50);
	clock.now += 3000;
	clock.time.StartGameClock(false);
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 1);
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 100);
}

TEST(GameClock, TheFrameRealTimeIsWholeMillisecondsOfTheWallClockAndNeverNone)
{
	Clock clock;
	clock.time.Start();
	clock.now += 16;
	clock.time.Update();
	clock.time.StartDraw();
	EXPECT_EQ(clock.time.GetFrameRealTime().count(), 16);
	// A frame within the same millisecond still takes one
	clock.time.Update();
	clock.time.StartDraw();
	EXPECT_EQ(clock.time.GetFrameRealTime().count(), 1);
	// Pausing the game doesn't stop it
	clock.time.StartGameClock(true);
	clock.now += 33;
	clock.time.Update();
	clock.time.StartDraw();
	EXPECT_EQ(clock.time.GetFrameRealTime().count(), 33);
}

// While the window is minimised the game does not draw. The real time stays that of the last draw, and the first draw
// after it takes all the time since then.
TEST(GameClock, TheFrameRealTimeIsTheTimeSinceTheLastDraw)
{
	Clock clock;
	clock.time.Start();
	clock.now += 20;
	clock.time.Update();
	clock.time.StartDraw();
	EXPECT_EQ(clock.time.GetFrameRealTime().count(), 20);
	for (int frame = 0; frame < 3; ++frame)
	{
		clock.now += 500;
		clock.time.Update();
		EXPECT_EQ(clock.time.GetFrameRealTime().count(), 20);
	}
	clock.now += 10;
	clock.time.Update();
	clock.time.StartDraw();
	EXPECT_EQ(clock.time.GetFrameRealTime().count(), 1510);
	EXPECT_EQ(clock.time.GetElapsedTime().count(), 1530);
}

TEST(GameClock, TheHandAndCameraStepByRealTimeExceptInAScriptsCutScene)
{
	using namespace std::chrono_literals;
	EXPECT_EQ(openblack::ecs::systems::CameraStep(17ms, 0ms, false, false), 17ms);
	EXPECT_EQ(openblack::ecs::systems::CameraStep(17ms, 20ms, true, false), 20ms);
}

TEST(GameClock, InsideTheTempleTheCameraStepsByRealTimeUnderTheCinemaBars)
{
	using namespace std::chrono_literals;
	// The world is paused in the temple, so the advisors' scenes there would otherwise freeze its camera
	EXPECT_EQ(openblack::ecs::systems::CameraStep(17ms, 0ms, true, true), 17ms);
	EXPECT_EQ(openblack::ecs::systems::CameraStep(17ms, 0ms, false, true), 17ms);
}

// Stepped deterministically, each frame takes the fixed time whatever the wall clock does, and the clock carries on from
// there once it goes back to the wall clock
TEST(GameClock, FixedFrameTimeIgnoresTheWallClock)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.time.Update();
	clock.time.StartDraw();
	clock.Frame();
	EXPECT_EQ(clock.time.GetTurn(), 1);

	clock.time.SetFixedFrameTime(std::chrono::milliseconds(50));
	// The wall clock runs wild, but two frames of 50 ms make a turn
	for (int frame = 0; frame < 10; ++frame)
	{
		clock.now += (frame % 2 == 0) ? 1000 : 3;
		clock.time.Update();
		clock.time.StartDraw();
		EXPECT_EQ(clock.time.GetFrameRealTime(), std::chrono::milliseconds(50));
		clock.Frame();
	}
	EXPECT_EQ(clock.time.GetTurn(), 6);

	// Back on the wall clock, time goes on from where the fixed frames left it, with no jump
	clock.time.SetFixedFrameTime(std::nullopt);
	clock.now += 10;
	clock.time.Update();
	clock.time.StartDraw();
	EXPECT_EQ(clock.time.GetFrameRealTime(), std::chrono::milliseconds(10));
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 6);
	clock.now += 90;
	clock.time.Update();
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 7);
}

// What times the testbed's scenarios: the frame's game time follows a fixed frame time while stepping, so that their
// seconds and fades keep in line with the turns stepped, and is none while paused
TEST(GameClock, TheFrameGameTimeFollowsAFixedFrameTime)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.time.SetFixedFrameTime(std::chrono::milliseconds(20));
	clock.time.Update();
	clock.Frame();
	for (int frame = 0; frame < 3; ++frame)
	{
		clock.now += 500;
		clock.time.Update();
		clock.Frame();
		EXPECT_EQ(clock.time.GetFrameGameTime().count(), 20);
	}
	clock.time.SetPaused(true);
	clock.time.Update();
	clock.Frame();
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 0);
}

// The temple holds the clock while the player is in it. The hold stops the world at once, and the player's pause stays.
TEST(GameClock, TheTempleHoldStopsTheClockAtOnceAndKeepsThePause)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	clock.now += 30;
	clock.Frame();
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 30);
	// The frame that enters the temple gives no game time to the things that it steps after the entry
	clock.time.SetHeld(true);
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 0);
	EXPECT_FALSE(clock.time.IsPaused());
	clock.now += 5000;
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 0);
	EXPECT_EQ(clock.time.GetTurn(), 1);
	// The player's pause in the temple does not start the clock when the hold ends
	clock.time.SetPaused(true);
	clock.time.SetHeld(false);
	EXPECT_FALSE(clock.Frame());
	clock.time.SetPaused(false);
	// The time in the temple does not count: the next turn is 70 ms on
	clock.now += 69;
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 69);
	clock.now += 1;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 2);
}

TEST(GameClock, ASavedGameResumesAtItsTurnWithTheTimePastIt)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	clock.time.ResumeAtTurn(500, 40);
	EXPECT_EQ(clock.time.GetTurn(), 500u);
	EXPECT_EQ(clock.time.GetTurnRemainderMs(), 40);
	// The next turn comes when the rest of the turn's time has gone
	clock.now += 59;
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 500u);
	clock.now += 1;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 501u);
}

TEST(GameClock, ASavedGameResumesPausedAsItWasSaved)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.time.SetPaused(true);
	clock.time.ResumeAtTurn(20, 0);
	clock.now += 1000;
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 20u);
	clock.time.SetPaused(false);
	clock.now += 100;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 21u);
}

TEST(GameClock, TheTimeBeforeTheFirstFrameIsNotGameTime)
{
	Clock clock;
	clock.time.StartGameClock(false);
	// The rest of the load and the logo take 1.5 s before the first frame. Then the timer goes back to turn 0.
	clock.now += 1500;
	clock.time.ResetTimerToTurn();
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 1);
	// No turn hurries to catch up the time of the load: the next turn comes 100 ms later
	clock.now += 99;
	EXPECT_FALSE(clock.Frame());
	clock.now += 1;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 2);
}

TEST(GameClock, ATimerPutBackUnderAPauseStaysStopped)
{
	Clock clock;
	clock.time.StartGameClock(true);
	clock.now += 1500;
	clock.time.ResetTimerToTurn();
	clock.now += 5000;
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 0);
	// Under the pause, the timer did not count the 5 s
	clock.time.SetPaused(false);
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 1);
	clock.now += 99;
	EXPECT_FALSE(clock.Frame());
}

// A fixed frame time with a part of a millisecond adds the parts up: a frame of 16.7 ms moves the tick count on by 16 or
// 17 ms, and ten frames move it on by 167 ms
TEST(GameClock, AFixedFrameTimeWithAPartOfAMillisecondAddsThePartsUp)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.time.SetFixedFrameTime(std::chrono::microseconds(16700));
	const auto start = clock.time.GetTicks();
	std::vector<uint32_t> steps;
	auto last = start;
	for (int frame = 0; frame < 10; ++frame)
	{
		clock.time.Update();
		steps.push_back(clock.time.GetTicks() - last);
		last = clock.time.GetTicks();
	}
	EXPECT_EQ(clock.time.GetTicks() - start, 167u);
	EXPECT_TRUE(std::ranges::all_of(steps, [](uint32_t step) { return step == 16u || step == 17u; }));
	EXPECT_EQ(clock.time.GetFixedFrameTime(), std::optional(std::chrono::microseconds(16700)));
}

TEST(GameClock, ThePlayersPauseStopsTheClockInASinglePlayerGame)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	clock.time.SetPlayerPause(true);
	EXPECT_TRUE(clock.time.IsPaused());
	clock.now += 500;
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 1);
	clock.time.SetPlayerPause(false);
	EXPECT_FALSE(clock.time.IsPaused());
}

TEST(GameClock, ANetworkGameNeverStopsForThePlayersPauseOrTheTempleHold)
{
	static_assert(openblack::ecs::systems::PlayerCanStopClock(false));
	static_assert(!openblack::ecs::systems::PlayerCanStopClock(true));
	Clock clock;
	clock.time.SetNetworkGame(true);
	EXPECT_TRUE(clock.time.IsNetworkGame());
	clock.time.StartGameClock(false);
	clock.Frame();
	// The P key, the menu, the start question and the films ask for the player's pause: the clock runs on
	clock.time.SetPlayerPause(true);
	EXPECT_FALSE(clock.time.IsPaused());
	// The temple's hold does not stop the clock
	clock.time.SetHeld(true);
	EXPECT_FALSE(clock.time.IsHeld());
	clock.now += 100;
	EXPECT_TRUE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 2);
	EXPECT_EQ(clock.time.GetFrameGameTime().count(), 100);
	// The land's own start and the tools still stop the clock
	clock.time.SetPaused(true);
	clock.now += 100;
	EXPECT_FALSE(clock.Frame());
	EXPECT_EQ(clock.time.GetTurn(), 2);
}

TEST(GameClock, AHoldThatStartedBeforeTheNetworkGameEnds)
{
	Clock clock;
	clock.time.StartGameClock(false);
	clock.Frame();
	clock.time.SetHeld(true);
	EXPECT_TRUE(clock.time.IsHeld());
	clock.time.SetNetworkGame(true);
	EXPECT_FALSE(clock.time.IsHeld());
	clock.now += 100;
	EXPECT_TRUE(clock.Frame());
}
