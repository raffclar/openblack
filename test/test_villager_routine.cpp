/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <deque>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/VillagerRoutine.h"

using namespace openblack;
using namespace openblack::ecs::villager_routine;

namespace
{
/// Hands out set answers in turn and keeps the limits it was asked for
struct FakeFloats
{
	std::deque<float> answers;
	std::vector<float> limits;

	[[nodiscard]] FloatRandom Random()
	{
		return [this](float limit) {
			limits.push_back(limit);
			const float answer = answers.front();
			answers.pop_front();
			return answer;
		};
	}
};

struct FakeInts
{
	std::deque<uint32_t> answers;
	std::vector<uint32_t> limits;

	[[nodiscard]] IntRandom Random()
	{
		return [this](uint32_t limit) {
			limits.push_back(limit);
			const uint32_t answer = answers.front();
			answers.pop_front();
			return answer;
		};
	}
};

/// The map's grid rounds positions a little
constexpr float k_GridLeeway = 0.05f;
} // namespace

TEST(VillagerRoutine, OutOfTheDoorTheDistanceIsDrawnBeforeTheAngle)
{
	FakeFloats random {.answers = {0.0f, k_EighthPi}};
	const glm::vec2 abode {100.0f, 100.0f};
	const glm::vec2 door {100.0f, 104.0f};
	const auto spot = PosOutsideDoor(abode, door, random.Random());
	ASSERT_EQ(random.limits.size(), 2u);
	EXPECT_FLOAT_EQ(random.limits[0], k_OutsideDoorRange);
	EXPECT_FLOAT_EQ(random.limits[1], k_QuarterPi);
	// No jitter and the least distance: straight out from the home, a step and a half past the door
	EXPECT_NEAR(glm::distance(spot, door), k_OutsideDoorLeast, k_GridLeeway);
	EXPECT_GT(glm::distance(spot, abode), glm::distance(door, abode));
}

TEST(VillagerRoutine, SittingAboutTownIsWithinTheChillOutDistance)
{
	FakeFloats random {.answers = {k_EighthPi, 0.0f}};
	const glm::vec2 congregation {200.0f, 200.0f};
	const glm::vec2 villager {200.0f, 260.0f};
	const auto spot = ChillOutPos(congregation, villager, 30.0f, random.Random());
	ASSERT_EQ(random.limits.size(), 2u);
	EXPECT_FLOAT_EQ(random.limits[0], k_QuarterPi);
	EXPECT_FLOAT_EQ(random.limits[1], 27.0f);
	// The least distance, a tenth of the chill-out distance, on the villager's side
	EXPECT_NEAR(glm::distance(spot, congregation), 3.0f, k_GridLeeway);
	EXPECT_LT(glm::distance(spot, villager), glm::distance(congregation, villager));
}

TEST(VillagerRoutine, NothingToDoIsOneInNineHomeThreeOutsideFiveInTown)
{
	FakeInts home {.answers = {0}};
	EXPECT_EQ(NothingToDo(true, true, true, home.Random()), Idle::GoHome);
	FakeInts outside {.answers = {3}};
	EXPECT_EQ(NothingToDo(true, true, true, outside.Random()), Idle::ChillOutsideHome);
	FakeInts town {.answers = {4}};
	EXPECT_EQ(NothingToDo(true, true, true, town.Random()), Idle::SitInTown);
	EXPECT_EQ(town.limits, std::vector<uint32_t>({9}));
}

TEST(VillagerRoutine, NothingToDoWithABrokenHomeGoesHomeOneTimeInTen)
{
	FakeInts goes {.answers = {0, 9}};
	EXPECT_EQ(NothingToDo(true, false, true, goes.Random()), Idle::GoHome);
	EXPECT_EQ(goes.limits, std::vector<uint32_t>({9, 100}));
	FakeInts stays {.answers = {0, 10}};
	EXPECT_EQ(NothingToDo(true, false, true, stays.Random()), Idle::ChillOutsideHome);
}

TEST(VillagerRoutine, NothingToDoWithNoTownEndsAtHome)
{
	FakeInts random {.answers = {8}};
	EXPECT_EQ(NothingToDo(false, false, false, random.Random()), Idle::GoHome);
}

TEST(VillagerRoutine, AKnockedVillagerSkipsBedUnlessBadlyHurt)
{
	EXPECT_TRUE(MaySleep(false, 1.0f, 0.3f));
	EXPECT_FALSE(MaySleep(true, 1.0f, 0.3f));
	EXPECT_TRUE(MaySleep(true, 0.2f, 0.3f));
}

TEST(VillagerRoutine, SleepRestoresLifeUpToFull)
{
	const auto result = CheckSleep({.life = 0.95f, .fullLife = 1.0f, .restores = 0.1f, .sleepUntil = 0.5f});
	EXPECT_FLOAT_EQ(result.gain, 0.05f);
	EXPECT_FALSE(result.sleepsOn);
}

TEST(VillagerRoutine, SleepGoesOnWhileTheTownWantsItOrLifeIsLow)
{
	EXPECT_TRUE(CheckSleep({.life = 1.0f, .townWantsSleep = true}).sleepsOn);
	const auto low = CheckSleep({.life = 0.2f, .fullLife = 1.0f, .restores = 0.01f, .multiplier = 2.0f, .sleepUntil = 0.5f});
	EXPECT_FLOAT_EQ(low.gain, 0.02f);
	EXPECT_TRUE(low.sleepsOn);
}

TEST(VillagerRoutine, PoisonWakesASleeper)
{
	const auto result = CheckSleep({.poisoned = true, .life = 0.2f, .restores = 0.1f, .townWantsSleep = true});
	EXPECT_FLOAT_EQ(result.gain, 0.0f);
	EXPECT_FALSE(result.sleepsOn);
}

TEST(VillagerRoutine, AnEmergencyLastsItsTurnsFromTheLastCall)
{
	EXPECT_FALSE(IsInStateOfEmergency(0, 500, 1200));
	EXPECT_TRUE(IsInStateOfEmergency(100, 1299, 1200));
	EXPECT_FALSE(IsInStateOfEmergency(100, 1300, 1200));
}

TEST(VillagerRoutine, ThoseInsideOrAsleepDontAnswerAnEmergency)
{
	EXPECT_FALSE(AnswersTownEmergency(VillagerStates::AtHome));
	EXPECT_FALSE(AnswersTownEmergency(VillagerStates::SleepingAtHome));
	EXPECT_FALSE(AnswersTownEmergency(VillagerStates::SitAndChillout));
	EXPECT_TRUE(AnswersTownEmergency(VillagerStates::MoveToPos));
}

TEST(VillagerRoutine, ARoamingSpotIsDrawnDistanceFirst)
{
	FakeFloats random {.answers = {0.0f, 0.0f}};
	const glm::vec2 from {50.0f, 50.0f};
	const auto spot = TentRetry(from, random.Random());
	ASSERT_EQ(random.limits.size(), 2u);
	EXPECT_FLOAT_EQ(random.limits[0], k_TentRetryRange);
	EXPECT_FLOAT_EQ(random.limits[1], k_TwoPi);
	EXPECT_NEAR(glm::distance(spot, from), k_TentRetryLeast, k_GridLeeway);
}
