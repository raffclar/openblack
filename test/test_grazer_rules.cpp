/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Animals/AnimalMove.h"
#include "Animals/GrazerRules.h"

using namespace openblack;
using namespace openblack::animals::grazers;

namespace
{
/// A roll that always gives the same whole number, below whatever it is asked for
auto Always(uint32_t value)
{
	return [value](uint32_t below) { return below == 0 ? 0 : value % below; };
}

/// A float random that gives the same share of whatever it is asked for
auto Share(float share)
{
	return [share](float max) { return max * share; };
}

/// The cow's speeds of the table: it walks at 492 and flees at 2621; its clips change at 1638
constexpr SpeedThresholds k_CowThresholds {.walk = 1638, .run = 1966};
constexpr SpeedThresholds k_HorseThresholds {.walk = 1966, .run = 4588};
constexpr uint16_t k_CowWalk = 492;
constexpr uint16_t k_CowFlee = 2621;
} // namespace

TEST(GrazerRules, SheepCowsHorsesPigsAndTortoisesGrazeButGoatsAndZebrasAreNeverMade)
{
	for (const auto type : {AnimalInfo::Sheep, AnimalInfo::Cow, AnimalInfo::Horse, AnimalInfo::Pig, AnimalInfo::Tortoise})
	{
		EXPECT_TRUE(IsGrazer(type));
	}
	for (const auto type : {AnimalInfo::Goat, AnimalInfo::Zebra, AnimalInfo::Lion, AnimalInfo::Dove, AnimalInfo::PuzzleCow})
	{
		EXPECT_FALSE(IsGrazer(type));
	}
}

TEST(GrazerRules, ACowWalksAtItsUsualSpeedAndRunsWhenFleeing)
{
	EXPECT_EQ(Clip(AnimalInfo::Cow, ClipSlot::Move, k_CowWalk, k_CowThresholds, Always(0)), AnimId::ACowWalk);
	EXPECT_EQ(Clip(AnimalInfo::Cow, ClipSlot::Move, k_CowFlee, k_CowThresholds, Always(0)), AnimId::ACowRun);
	// Exactly at the threshold it still walks
	EXPECT_EQ(Clip(AnimalInfo::Sheep, ClipSlot::Move, 1638, k_CowThresholds, Always(0)), AnimId::ASheepWalk);
	EXPECT_EQ(Clip(AnimalInfo::Pig, ClipSlot::Move, 1639, k_CowThresholds, Always(0)), AnimId::APigRun);
	EXPECT_EQ(ThresholdRowOf(AnimalInfo::Pig), SpeedThreshold::AnimalCow);
	EXPECT_EQ(ThresholdRowOf(AnimalInfo::Horse), SpeedThreshold::AnimalHorse);
}

TEST(GrazerRules, AHorseWalksTrotsAndGallops)
{
	EXPECT_EQ(Clip(AnimalInfo::Horse, ClipSlot::Move, 983, k_HorseThresholds, Always(0)), AnimId::AHorseWalk);
	EXPECT_EQ(Clip(AnimalInfo::Horse, ClipSlot::Move, 3000, k_HorseThresholds, Always(0)), AnimId::AHorseTrot);
	EXPECT_EQ(Clip(AnimalInfo::Horse, ClipSlot::Move, 5898, k_HorseThresholds, Always(0)), AnimId::AHorseRun);
}

TEST(GrazerRules, AGrazerGrazesInEitherOfItsTwoClipsAndATortoiseOnlyEverStandsOrWalks)
{
	EXPECT_EQ(Clip(AnimalInfo::Cow, ClipSlot::Eat, k_CowWalk, k_CowThresholds, Always(1)), AnimId::ACowEat_1);
	EXPECT_EQ(Clip(AnimalInfo::Cow, ClipSlot::Eat, k_CowWalk, k_CowThresholds, Always(0)), AnimId::ACowEat_2);
	EXPECT_EQ(Clip(AnimalInfo::Cow, ClipSlot::Stand, 0, k_CowThresholds, Always(0)), AnimId::ACowStand);
	EXPECT_EQ(Clip(AnimalInfo::Cow, ClipSlot::Sleep, 0, k_CowThresholds, Always(0)), AnimId::ACowStand);
	EXPECT_EQ(Clip(AnimalInfo::Cow, ClipSlot::StartToEat, 0, k_CowThresholds, Always(0)), AnimId::ACowGotoEat);
	EXPECT_EQ(Clip(AnimalInfo::Horse, ClipSlot::FinishEating, 0, k_HorseThresholds, Always(0)), AnimId::AHorseUpFromEat);
	EXPECT_EQ(Clip(AnimalInfo::Tortoise, ClipSlot::Move, 5000, k_CowThresholds, Always(0)), AnimId::ATortoiseWalk);
	for (const auto slot : {ClipSlot::Eat, ClipSlot::Stand, ClipSlot::Dying, ClipSlot::InHand})
	{
		EXPECT_EQ(Clip(AnimalInfo::Tortoise, slot, 0, k_CowThresholds, Always(1)), AnimId::ATortoiseStand);
	}
}

TEST(GrazerRules, ASteerIsCutToWhatIsLeftOfTheSpeed)
{
	glm::ivec2 step {0};
	// A push adds its share of the speed it takes: its larger side over the speed, so a push of 100 adds a fifth of it
	EXPECT_FALSE(AddSteer(step, {100, 50}, 492));
	EXPECT_EQ(step, glm::ivec2(20, 10));
	// What is left is 472; a push of 1000 along x is cut to it: 1000 x 472 / 492, its z share likewise
	EXPECT_TRUE(AddSteer(step, {1000, 500}, 492));
	EXPECT_EQ(step, glm::ivec2(20 + 959, 10 + 479));
	// Nothing left: nothing more is added
	EXPECT_TRUE(AddSteer(step, {10, 10}, 492));
	EXPECT_EQ(step, glm::ivec2(979, 489));
}

TEST(GrazerRules, AWandererAloneTurnsAtRandomByUpToHalfItsTurnAngle)
{
	const WanderSetup setup {.position = {100000, 100000}, .angle = 0, .speed = 492, .turnAngle = 34};
	// A roll of 17 turns it not at all, 0 by half its turn angle one way
	// A step along its way (480 for a speed of 492) adds its share of the speed: 480 x 480 / 492
	const auto straight = NewWanderStep(setup, {}, Always(17));
	EXPECT_EQ(straight, glm::ivec2(468, 0));
	const auto along = animals::StepAlong(static_cast<uint16_t>(0x800 - 17), 492);
	const auto turned = NewWanderStep(setup, {}, Always(0));
	EXPECT_EQ(turned, glm::ivec2(Scale(along.x, along.x, 492), Scale(along.y, along.x, 492)));
}

TEST(GrazerRules, AWandererTooFarFromItsLeaderHeadsBackAtNineTenthsOfItsSpeed)
{
	// Its leader 20 m along +z, beyond 10 m: back at nine tenths, then the rest at random
	const glm::ivec2 here {100000, 100000};
	const glm::ivec2 leader {100000, 100000 + 131072};
	const WanderSetup setup {
	    .position = here, .angle = 0, .speed = 500, .turnAngle = 34, .centre = leader, .inner = 0, .outer = 10};
	const auto step = NewWanderStep(setup, {}, Always(17));
	// Nine tenths of the way along +z first, then the tenth left along the way it faces, +x
	const auto back = animals::StepAlong(0x200, 500);
	const int32_t pull = Scale(back.y, 450, 500);
	EXPECT_EQ(step.y, Scale(pull, pull, 500));
	EXPECT_GT(step.x, 0);
	// Within its distances there is no pull back
	const WanderSetup near {
	    .position = here, .angle = 0, .speed = 500, .turnAngle = 34, .centre = leader, .inner = 0, .outer = 30};
	const auto along = animals::StepAlong(0, 500);
	EXPECT_EQ(NewWanderStep(near, {}, Always(17)), glm::ivec2(Scale(along.x, along.x, 500), 0));
}

TEST(GrazerRules, TheHerdPullsAWandererTowardsTheOthers)
{
	glm::ivec2 step {0};
	const glm::ivec2 here {0, 0};
	const std::array<HerdMate, 1> mates {{{.position = {0, 655360}, .step = {0, 0}}}};
	const bool usedUp = HerdSteer(step, here, 500, mates, 30);
	EXPECT_FALSE(usedUp);
	// A fifth towards the mate, then that again with the mate's pull towards it along z (it is far beyond 30 units);
	// across x the mate is level with it, nearer than the flock distance, which pushes it the -x way
	EXPECT_GT(step.y, 0);
	EXPECT_LT(step.x, 0);
	glm::ivec2 none {0};
	EXPECT_FALSE(HerdSteer(none, here, 500, {}, 30));
	EXPECT_EQ(none, glm::ivec2(0));
}

TEST(GrazerRules, NeedsGrowToTheirLimitsAndBreedingOnlyInAHerdThatLostMembers)
{
	const NeedLimits limits {.hunger = 50, .sleep = 1000, .breed = 3000, .grownUpAge = 13};
	Needs needs {.hunger = 49, .sleep = 1000, .breed = 0};
	GrowNeeds(needs, limits, 20, HerdSize {.members = 6, .most = 6});
	EXPECT_EQ(needs.hunger, 50);
	EXPECT_EQ(needs.sleep, 1000);
	EXPECT_EQ(needs.breed, 0);
	GrowNeeds(needs, limits, 20, HerdSize {.members = 5, .most = 6});
	EXPECT_EQ(needs.hunger, 50);
	EXPECT_EQ(needs.breed, 1);
	// A young one, or one alone, never needs to breed
	GrowNeeds(needs, limits, 3, HerdSize {.members = 5, .most = 6});
	GrowNeeds(needs, limits, 20, HerdSize {.members = 1, .most = 6});
	EXPECT_EQ(needs.breed, 1);
}

TEST(GrazerRules, AGrazerBreedsFirstThenGrazesThenSleeps)
{
	const NeedLimits limits {.hunger = 50, .sleep = 1000, .breed = 3000, .grownUpAge = 13};
	Needs needs {.hunger = 50, .sleep = 1000, .breed = 2999};
	EXPECT_EQ(NeedToSee(needs, limits, 20, HerdSize {.members = 5, .most = 6}, true), Need::Breed);
	EXPECT_EQ(needs.breed, 0);
	// Full in a herd that has lost nobody, the need starts again and it goes on to graze
	needs.breed = 2999;
	EXPECT_EQ(NeedToSee(needs, limits, 20, HerdSize {.members = 6, .most = 6}, true), Need::Graze);
	EXPECT_EQ(needs.breed, 0);
	needs.hunger = 10;
	EXPECT_EQ(NeedToSee(needs, limits, 20, HerdSize {.members = 6, .most = 6}, true), Need::Sleep);
	EXPECT_EQ(NeedToSee(needs, limits, 20, HerdSize {.members = 6, .most = 6}, false), Need::None);
}

TEST(GrazerRules, ReadyToBreedOnlyWithTheNeedFullAndMembersLost)
{
	const NeedLimits limits {.hunger = 50, .sleep = 1000, .breed = 3000, .grownUpAge = 13};
	Needs needs {.breed = 3000};
	EXPECT_TRUE(ReadyToBreed(needs, limits, HerdSize {.members = 4, .most = 5}));
	EXPECT_FALSE(ReadyToBreed(needs, limits, HerdSize {.members = 5, .most = 5}));
	EXPECT_EQ(needs.breed, 0);
}

TEST(GrazerRules, GrassIsLookedForInASpiralOfCellsAsFarAsTheHerdsReach)
{
	EXPECT_EQ(GrazeSearchCells(80), 64);
	EXPECT_EQ(GrazeSearchCells(29), 8);
	std::vector<glm::ivec2> seen;
	const glm::ivec2 from {0x12345, 0x23456};
	const auto spot = FindGrazeSpot(from, 30, [&](glm::ivec2 point) {
		seen.push_back(point);
		return seen.size() == 4;
	});
	ASSERT_TRUE(spot.has_value());
	// Its own place, then one cell -x, one -z, one +x: the spiral, keeping its place within each cell
	EXPECT_EQ(seen.at(0), from);
	EXPECT_EQ(seen.at(1), from + glm::ivec2(-0x10000, 0));
	EXPECT_EQ(seen.at(2), from + glm::ivec2(-0x10000, -0x10000));
	EXPECT_EQ(*spot, from + glm::ivec2(0, -0x10000));
	EXPECT_FALSE(FindGrazeSpot(from, 30, [](glm::ivec2) { return false; }).has_value());
}

TEST(GrazerRules, ASleeperLiesDownInASquareAboutItsHerdsSleepingCellsCorner)
{
	// Six members: a square 12 m a side about the corner, the draws for x then z
	const auto middle = SleepSpot({200, 300}, 6, Share(0.5f));
	EXPECT_NEAR(static_cast<float>(middle.x) / 6553.6f, 2000.0f, 0.01f);
	EXPECT_NEAR(static_cast<float>(middle.y) / 6553.6f, 3000.0f, 0.01f);
	const auto corner = SleepSpot({200, 300}, 6, Share(0.0f));
	EXPECT_NEAR(static_cast<float>(corner.x) / 6553.6f, 2006.0f, 0.01f);
}

TEST(GrazerRules, ASleeperWakesWhenItsNeedToSleepRunsOut)
{
	Needs needs {.sleep = 3};
	EXPECT_FALSE(SleepTurn(needs));
	EXPECT_EQ(needs.sleep, 1);
	EXPECT_TRUE(SleepTurn(needs));
	EXPECT_EQ(needs.sleep, 0);
}

TEST(GrazerRules, AYoungGrazerGrowsUpToThreeQuartersOfTheWayToNextYearsSize)
{
	std::array<float, 20> table {};
	table.fill(1.0f);
	table[5] = 0.8f;
	EXPECT_FLOAT_EQ(GrownScale(0.6f, 4, table, Share(1.0f)), 0.6f + 0.15f);
	EXPECT_FLOAT_EQ(GrownScale(0.6f, 4, table, Share(0.0f)), 0.6f);
}

TEST(GrazerRules, TheBiggerHerdKeepsThemAllUnlessTheOtherIsAScripts)
{
	EXPECT_TRUE(MergeKeepsLooker(5, 5, false));
	EXPECT_FALSE(MergeKeepsLooker(4, 5, false));
	EXPECT_FALSE(MergeKeepsLooker(9, 1, true));
	// The other's most is added once for each that joins, within the largest herd
	EXPECT_EQ(MergedMost(5, 3, 3, 50), 14u);
	EXPECT_EQ(MergedMost(5, 30, 3, 50), 50u);
}
