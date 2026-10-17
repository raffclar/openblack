/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <bit>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/VillagerAge.h"

using namespace openblack::ecs::villager_age;

namespace
{
constexpr std::array<float, 20> k_AgeToScale = {0.41f, 0.45f, 0.5f,  0.54f, 0.58f, 0.63f, 0.67f, 0.71f, 0.76f, 0.8f,
                                                0.84f, 0.89f, 0.93f, 0.97f, 0.97f, 0.97f, 0.97f, 1.15f, 1.19f, 1.19f};
constexpr uint32_t k_GrownUp = 13;

/// Hands out the given draws in turn, recording the limits asked for
struct Draws
{
	std::vector<float> values;
	std::vector<float> limits;
	size_t next {0};
	FloatRandom Float()
	{
		return [this](float limit) {
			limits.push_back(limit);
			return values.at(next++);
		};
	}
};
} // namespace

TEST(VillagerAge, AgeCountsWholeYearsOfFifteenHundredTurns)
{
	EXPECT_EQ(AgeOf(1499, 0), 0u);
	EXPECT_EQ(AgeOf(1500, 0), 1u);
	EXPECT_EQ(AgeOf(31500, 1500), 20u);
	// Born before the game began: the turns wrap round
	const auto born = BirthTurnFor(100, 30);
	EXPECT_EQ(AgeOf(100, born), 30u);
	EXPECT_EQ(AgeOf(100 + 1500, born), 31u);
}

TEST(VillagerAge, AdultsAreAtLeastEighteen)
{
	EXPECT_EQ(GivenAge(5, k_GrownUp), 5u);
	EXPECT_EQ(GivenAge(13, k_GrownUp), 18u);
	EXPECT_EQ(GivenAge(40, k_GrownUp), 40u);
	EXPECT_TRUE(IsChildAge(12, k_GrownUp));
	EXPECT_FALSE(IsChildAge(13, k_GrownUp));
}

TEST(VillagerAge, ChildrenStartAtTheSizeOfTheYearBeforeAndAdultsANearlyFullOne)
{
	// Age 1 takes the first size of the table, age 12 the size of age 11
	EXPECT_EQ(StartScale(1, k_GrownUp, k_AgeToScale, 0.0f), 0.41f);
	EXPECT_EQ(StartScale(12, k_GrownUp, k_AgeToScale, 0.0f), 0.89f);
	EXPECT_EQ(StartScale(30, k_GrownUp, k_AgeToScale, 0.0f), 0.9f);
	// Age 0 takes the value before the table: the dancing speed read as a size
	EXPECT_EQ(StartScale(0, k_GrownUp, k_AgeToScale, BeforeTable(0x3F000000u)), 0.5f);
	EXPECT_EQ(BeforeTable(10u), std::bit_cast<float>(10u));
}

TEST(VillagerAge, AChildGrowsUpToThreeQuartersOfTheWayToTheSizeOfTheYearAfter)
{
	// Age 5 grows towards the size of age 6
	Draws draws {.values = {0.03f}};
	const float scale = GrownScale(5, k_GrownUp, k_AgeToScale, 0.58f, draws.Float());
	ASSERT_EQ(draws.limits.size(), 1u);
	EXPECT_EQ(draws.limits[0], (0.67f - 0.58f) * 0.75f);
	EXPECT_EQ(scale, 0.58f + 0.03f);
	// The last year of the table has no year after it: the size stays
	Draws none {.values = {}};
	EXPECT_EQ(GrownScale(19, 20, k_AgeToScale, 0.7f, none.Float()), 0.7f);
	EXPECT_TRUE(none.limits.empty());
}

TEST(VillagerAge, AnAdultKeepsItsSizeUnlessTheDrawIsBiggerThenDrawsAgain)
{
	// (0.05 - 0.08) + 1 = 0.97 is no bigger than 0.98: it stays
	Draws keep {.values = {0.08f}};
	EXPECT_FLOAT_EQ(GrownScale(30, k_GrownUp, k_AgeToScale, 0.98f, keep.Float()), 0.98f);
	EXPECT_EQ(keep.limits.size(), 1u);
	// (0.05 - 0.02) + 1 is bigger than 0.9: a second draw decides, with the spread first and the one added last
	Draws redraw {.values = {0.02f, 0.09f}};
	const float adult = GrownScale(30, k_GrownUp, k_AgeToScale, 0.9f, redraw.Float());
	EXPECT_EQ(adult, (0.05f - 0.09f) + 1.0f);
	ASSERT_EQ(redraw.limits.size(), 2u);
	EXPECT_FLOAT_EQ(redraw.limits[1], 0.1f);
}

TEST(VillagerAge, ChildrenGrowFourTimesAYear)
{
	EXPECT_TRUE(IsGrowthTurn(0));
	EXPECT_FALSE(IsGrowthTurn(374));
	EXPECT_TRUE(IsGrowthTurn(375));
	EXPECT_TRUE(IsGrowthTurn(1500));
}

TEST(VillagerAge, TheOldAreCheckedOnTheirOwnTurnOfEightHundred)
{
	// Its own turn, 10 + 790 = 800, fell within the last 8 turns
	EXPECT_TRUE(IsOldAgeCheckDue(790, 10, 8));
	EXPECT_TRUE(IsOldAgeCheckDue(797, 10, 8));
	EXPECT_FALSE(IsOldAgeCheckDue(798, 10, 8));
}

TEST(VillagerAge, OnlyThePastOldAgeDieOfItAndTheOlderTheLikelier)
{
	const Ages ages {.grownUp = 13, .old = 60, .oldest = 100};
	std::vector<uint32_t> intLimits;
	const IntRandom extra = [&intLimits](uint32_t limit) {
		intLimits.push_back(limit);
		return limit;
	};
	const FloatRandom full = [](float limit) { return limit; };
	EXPECT_FALSE(DiesOfOldAge(60, ages, full, extra));
	EXPECT_TRUE(intLimits.empty());
	// The draw at its most adds all forty years: 61 + 40 is past 100
	EXPECT_TRUE(DiesOfOldAge(61, ages, full, extra));
	EXPECT_EQ(intLimits.back(), 40u);
	// A half draw cubed is an eighth: five more years, 90 + 5 is not past 100
	const FloatRandom half = [](float limit) { return limit * 0.5f; };
	EXPECT_FALSE(DiesOfOldAge(90, ages, half, extra));
	EXPECT_EQ(intLimits.back(), 5u);
	EXPECT_TRUE(DiesOfOldAge(96, ages, half, extra));
}

namespace
{
/// Hands out float and whole draws, recording each draw's limit in order
struct Stream
{
	std::vector<float> floats;
	std::vector<uint32_t> ints;
	std::vector<std::string> order;
	size_t nextFloat {0};
	size_t nextInt {0};
	FloatRandom Float()
	{
		return [this](float limit) {
			order.push_back("f" + std::to_string(limit));
			return floats.at(nextFloat++);
		};
	}
	IntRandom Int()
	{
		return [this](uint32_t limit) {
			order.push_back("i" + std::to_string(limit));
			return ints.at(nextInt++);
		};
	}
};
const NewbornKind k_Kind {.grownUp = k_GrownUp, .hungryForFood = 0.5f, .processChecksEvery = 8, .ageToScale = k_AgeToScale};
} // namespace

TEST(VillagerAge, ANewAdultDrawsItsSizeFoodCheckAndWaitInTheGamesOrder)
{
	Stream stream {.floats = {0.02f, 0.08f, 0.3f, 0.45f}, .ints = {5, 3, 41}};
	const auto born = MakeNewborn(13, 1000, k_Kind, stream.Float(), stream.Int());
	EXPECT_EQ(born.age, 18u);
	EXPECT_FALSE(born.child);
	// (0.05 - 0.02) + 1 is bigger than 0.9, so the size is a second draw
	EXPECT_EQ(born.scale, (0.05f - 0.08f) + 1.0f);
	// 0.3 + 0.5 is under full, so the food is a second draw
	EXPECT_FLOAT_EQ(born.food, 0.95f);
	// 5 is under the turn, so the check is drawn again
	EXPECT_EQ(born.lastCheckTurn, 997u);
	EXPECT_EQ(born.turnsUntilFirstDecision, 42u);
	const std::vector<std::string> expected = {"f" + std::to_string(0.1f),
	                                           "f" + std::to_string(0.1f),
	                                           "f" + std::to_string(0.6f),
	                                           "f" + std::to_string(0.6f),
	                                           "i8",
	                                           "i8",
	                                           "i500"};
	EXPECT_EQ(stream.order, expected);
}

TEST(VillagerAge, ANewChildIsTheSizeOfTheYearBeforeAndAFullOneDrawsOnce)
{
	Stream stream {.floats = {0.0f, 0.55f}, .ints = {5, 0}};
	const auto born = MakeNewborn(5, 2, k_Kind, stream.Float(), stream.Int());
	EXPECT_TRUE(born.child);
	EXPECT_EQ(born.age, 5u);
	EXPECT_EQ(born.scale, 0.58f);
	// 0.55 + 0.5 is full: exactly full, no second draw
	EXPECT_FLOAT_EQ(born.food, 1.0f);
	// The game is younger than the draw: last checked when the game began
	EXPECT_EQ(born.lastCheckTurn, 0u);
	EXPECT_EQ(born.turnsUntilFirstDecision, 1u);
}

TEST(VillagerAge, AScriptMakingAGrownVillagerYoungGivesItTheChildsModel)
{
	// The opening's son is made grown (14) and then given the age 8
	const auto setting = openblack::ecs::villager_age::SetAge(8, 18, 13);
	EXPECT_EQ(setting.age, 8u);
	EXPECT_TRUE(setting.child);
	ASSERT_TRUE(setting.childModel.has_value());
	EXPECT_TRUE(*setting.childModel);
}

TEST(VillagerAge, AnAgeOnTheSameSideOfGrowingUpKeepsTheModel)
{
	const auto child = openblack::ecs::villager_age::SetAge(3, 8, 13);
	EXPECT_TRUE(child.child);
	EXPECT_FALSE(child.childModel.has_value());
	// An adult is never younger than the youngest adult's age
	const auto adult = openblack::ecs::villager_age::SetAge(14, 40, 13);
	EXPECT_EQ(adult.age, 18u);
	EXPECT_FALSE(adult.child);
	EXPECT_FALSE(adult.childModel.has_value());
}

TEST(VillagerAge, AChildGivenAGrownAgeWearsItsAdultModel)
{
	const auto setting = openblack::ecs::villager_age::SetAge(13, 5, 13);
	EXPECT_EQ(setting.age, 18u);
	EXPECT_FALSE(setting.child);
	ASSERT_TRUE(setting.childModel.has_value());
	EXPECT_FALSE(*setting.childModel);
}

TEST(VillagerAge, AVillagerGivenAnAgeWearsItsKindsModelsForItAtEveryDistance)
{
	const std::array child {1, 2, 3};
	const std::array adult {4, 5, 6};
	EXPECT_EQ(openblack::ecs::villager_age::ModelsForAge(true, child, adult), child);
	EXPECT_EQ(openblack::ecs::villager_age::ModelsForAge(false, child, adult), adult);
}

TEST(VillagerAge, AChildGrowsOnGrowthTurnsAndGrowsUpAtTheAgeOfGrowingUp)
{
	// Younger than growing up: only on a growth turn
	EXPECT_EQ(CheckChild(5, k_GrownUp, 375), ChildGrowth::Grows);
	EXPECT_EQ(CheckChild(5, k_GrownUp, 750), ChildGrowth::Grows);
	EXPECT_EQ(CheckChild(5, k_GrownUp, 376), ChildGrowth::None);
	EXPECT_EQ(CheckChild(12, k_GrownUp, 374), ChildGrowth::None);
	// Old enough, on any turn
	EXPECT_EQ(CheckChild(13, k_GrownUp, 376), ChildGrowth::GrowsUp);
	EXPECT_EQ(CheckChild(13, k_GrownUp, 375), ChildGrowth::GrowsUp);
	// It is made the youngest adult's age
	EXPECT_EQ(GrownUpAge(k_GrownUp), 18u);
	EXPECT_EQ(GrownUpAge(25), 25u);
}

TEST(VillagerAge, ChecksEveryNineTurnsMeetGrowthTurnsOnlyOnSomePhases)
{
	// Checks fall every nine turns at a phase of their own; the growth turns, every 375, fall on phases 0, 3 and 6 only
	const auto growths = [](uint32_t phase) {
		uint32_t count = 0;
		for (uint32_t turn = phase; turn < 375 * 9; turn += 9)
		{
			count += CheckChild(5, k_GrownUp, turn) == ChildGrowth::Grows ? 1 : 0;
		}
		return count;
	};
	EXPECT_EQ(growths(0), 3u);
	EXPECT_EQ(growths(3), 3u);
	EXPECT_EQ(growths(6), 3u);
	EXPECT_EQ(growths(1), 0u);
	EXPECT_EQ(growths(8), 0u);
}

TEST(VillagerAge, AGrownChildTakesAnAdultsSizeAndIsNotSizedAgainAtHome)
{
	// Grown up it starts at the adult start size and draws twice, since the first draw is always above it
	Draws draws {.values = {0.02f, 0.06f}};
	const auto start = StartScale(GrownUpAge(k_GrownUp), k_GrownUp, k_AgeToScale, 0.0f);
	EXPECT_FLOAT_EQ(start, 0.9f);
	const auto scale = GrownScale(GrownUpAge(k_GrownUp), k_GrownUp, k_AgeToScale, start, draws.Float());
	EXPECT_EQ(scale, (0.05f - 0.06f) + 1.0f);
	EXPECT_EQ(draws.limits.size(), 2u);
	// Even the smallest adult size drawn (a draw is always under its limit) is full grown, so it isn't sized again
	// arriving home
	EXPECT_GE((0.05f - 0.0999f) + 1.0f, k_FullGrownScale);
}
