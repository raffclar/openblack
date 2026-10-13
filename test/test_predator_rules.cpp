/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Animals/PredatorRules.h"

using namespace openblack;
using namespace openblack::animals::predators;

namespace
{
/// The hunters' reaction: priority 160, run from 30 m, give up beyond 75 m, reaching 25 m
constexpr uint32_t k_TablePriority = 160;
constexpr float k_LeastRun = 30.0f;
constexpr float k_FurthestRun = 75.0f;
constexpr float k_Reach = 25.0f;
/// A hunter's speed state 3
constexpr uint32_t k_HunterSpeed3 = 6554;

VillagerSees Near()
{
	return {.animal = true,
	        .distance = 10.0f,
	        .leastRunDistance = k_LeastRun,
	        .speed = 10.0f,
	        .kindSpeed3 = k_HunterSpeed3,
	        .tablePriority = k_TablePriority};
}

auto Rolls(uint32_t value)
{
	return [value](uint32_t below) { return below == 0 ? 0 : value % below; };
}
} // namespace

TEST(PredatorRules, TheHuntersAreTheBigCatsAndTheWolves)
{
	EXPECT_TRUE(IsPredator(AnimalInfo::Lion));
	EXPECT_TRUE(IsPredator(AnimalInfo::Tiger));
	EXPECT_TRUE(IsPredator(AnimalInfo::Wolf));
	EXPECT_TRUE(IsPredator(AnimalInfo::Leopard));
	EXPECT_TRUE(IsPredator(AnimalInfo::SpellWolf));
	EXPECT_FALSE(IsPredator(AnimalInfo::Cow));
	EXPECT_FALSE(IsPredator(AnimalInfo::Tortoise));
	EXPECT_FALSE(IsPredator(AnimalInfo::Dove));
}

TEST(PredatorRules, ASpeedStateOf6554IsTenMetresASecond)
{
	EXPECT_NEAR(SpeedMetresPerSecond(6554), 10.0006f, 0.0001f);
	EXPECT_FLOAT_EQ(SpeedMetresPerSecond(0), 0.0f);
}

TEST(PredatorRules, AVillagerWeighsAHunterAtAboutHalfTheTable)
{
	// The speed is divided by a raw speed state, so the factor is a hair over a half
	EXPECT_EQ(VillagerPriority(Near()), 80);
	auto still = Near();
	still.speed = 0.0f;
	EXPECT_EQ(VillagerPriority(still), 80);
}

TEST(PredatorRules, AVillagerIgnoresWhatIsNotAnAnimalOrWhenIndoors)
{
	auto thing = Near();
	thing.animal = false;
	EXPECT_EQ(VillagerPriority(thing), 0);
	auto home = Near();
	home.atHome = true;
	EXPECT_EQ(VillagerPriority(home), 0);
	auto hiding = Near();
	hiding.hiding = true;
	EXPECT_EQ(VillagerPriority(hiding), 0);
}

TEST(PredatorRules, OnItsWayToHideOnlyANearHunterCounts)
{
	auto going = Near();
	going.goingToHide = true;
	EXPECT_EQ(VillagerPriority(going), 80);
	going.distance = 31.0f;
	EXPECT_EQ(VillagerPriority(going), 0);
}

TEST(PredatorRules, ARestingHunterFrightensNoOneButALongLookout)
{
	auto resting = Near();
	resting.hunterResting = true;
	EXPECT_EQ(VillagerPriority(resting), 0);
	resting.lookingAbout = true;
	resting.turnsLookingAbout = 19;
	EXPECT_EQ(VillagerPriority(resting), 0);
	resting.turnsLookingAbout = 20;
	EXPECT_EQ(VillagerPriority(resting), 80);
}

TEST(PredatorRules, AFarHunterFledBeforeCountsOnlyComingAtIt)
{
	auto far = Near();
	far.distance = 30.5f;
	EXPECT_EQ(VillagerPriority(far), 80);
	far.frightenedBefore = true;
	EXPECT_EQ(VillagerPriority(far), 0);
	far.comingAt = true;
	EXPECT_EQ(VillagerPriority(far), 80);
	// At the least run distance it isn't far
	auto edge = Near();
	edge.distance = k_LeastRun;
	edge.frightenedBefore = true;
	EXPECT_EQ(VillagerPriority(edge), 80);
}

TEST(PredatorRules, OnlyAStrongerHunterFrightensAHunter)
{
	EXPECT_EQ(HunterPriority(true, 0.45f, 0.40f, k_TablePriority), 160);
	EXPECT_EQ(HunterPriority(true, 0.40f, 0.40f, k_TablePriority), 0);
	EXPECT_EQ(HunterPriority(true, 0.40f, 0.45f, k_TablePriority), 0);
	EXPECT_EQ(HunterPriority(false, 0.0f, 0.45f, k_TablePriority), 160);
}

TEST(PredatorRules, AGrazerTakesFrightOnlyAtAHunterOverItsSecondSpeed)
{
	// A hunter's speed state 2 is 819; walking it goes at 983
	EXPECT_EQ(OtherPriority(true, 983, 819, k_TablePriority), 160);
	EXPECT_EQ(OtherPriority(true, 819, 819, k_TablePriority), 0);
	EXPECT_EQ(OtherPriority(true, 0, 819, k_TablePriority), 0);
	EXPECT_EQ(OtherPriority(false, 0, 819, k_TablePriority), 160);
}

TEST(PredatorRules, AFrightenedVillagerHidesFromAFastHunterComingAtItWithoutARoll)
{
	const VillagerFleeTurn turn {.hunterThere = true,
	                             .animal = true,
	                             .distance = 20.0f,
	                             .furthestRunDistance = k_FurthestRun,
	                             .fast = true,
	                             .comingAt = true};
	bool rolled = false;
	const auto roll = [&rolled](uint32_t) {
		rolled = true;
		return 1u;
	};
	EXPECT_EQ(VillagerFleeStep(turn, roll), VillagerFlees::TryToHide);
	EXPECT_FALSE(rolled);
}

TEST(PredatorRules, OtherwiseOneInSixHides)
{
	VillagerFleeTurn turn {
	    .hunterThere = true, .animal = true, .distance = 20.0f, .furthestRunDistance = k_FurthestRun, .fast = true};
	EXPECT_EQ(VillagerFleeStep(turn, Rolls(0)), VillagerFlees::TryToHide);
	EXPECT_EQ(VillagerFleeStep(turn, Rolls(1)), VillagerFlees::RunOrWatch);
	turn.distance = 76.0f;
	EXPECT_EQ(VillagerFleeStep(turn, Rolls(0)), VillagerFlees::Stop);
	turn.animal = false;
	EXPECT_EQ(VillagerFleeStep(turn, Rolls(0)), VillagerFlees::Nothing);
	turn.hunterThere = false;
	EXPECT_EQ(VillagerFleeStep(turn, Rolls(0)), VillagerFlees::Stop);
}

TEST(PredatorRules, AVillagerWatchesAFarHunterNotComingAtIt)
{
	EXPECT_TRUE(WatchesRatherThanRuns(31.0f, k_LeastRun, false));
	EXPECT_FALSE(WatchesRatherThanRuns(31.0f, k_LeastRun, true));
	EXPECT_FALSE(WatchesRatherThanRuns(k_LeastRun, k_LeastRun, false));
}

TEST(PredatorRules, ALookoutIsFrightenedByAnAwakeHunterWithinReach)
{
	EXPECT_TRUE(StillFrightensLookout(true, k_Reach, k_Reach, true, false));
	EXPECT_FALSE(StillFrightensLookout(true, 25.5f, k_Reach, true, false));
	EXPECT_FALSE(StillFrightensLookout(true, 10.0f, k_Reach, true, true));
	EXPECT_FALSE(StillFrightensLookout(true, 10.0f, k_Reach, false, false));
	EXPECT_FALSE(StillFrightensLookout(false, 10.0f, k_Reach, true, false));
}

TEST(PredatorRules, AnAnimalRunsUntilTheHunterIsGoneOrFar)
{
	EXPECT_EQ(AnimalFleeStep(false, false, 0.0f, k_FurthestRun), AnimalFlees::Nothing);
	EXPECT_EQ(AnimalFleeStep(true, false, 0.0f, k_FurthestRun), AnimalFlees::Stop);
	EXPECT_EQ(AnimalFleeStep(true, true, 76.0f, k_FurthestRun), AnimalFlees::Stop);
	EXPECT_EQ(AnimalFleeStep(true, true, k_FurthestRun, k_FurthestRun), AnimalFlees::Run);
}
