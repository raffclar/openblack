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

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/VillagerBreedingRules.h"

using namespace openblack;
using namespace openblack::ecs::villager_breeding;

namespace
{
/// Hands out the given draws in turn, recording the limits asked for
template <typename T>
struct Draws
{
	std::vector<T> values;
	std::vector<T> limits;
	size_t next {0};
	std::function<T(T)> Make()
	{
		return [this](T limit) {
			limits.push_back(limit);
			return values.at(next++);
		};
	}
};

HomeConception Fertile()
{
	return {.pregnant = false,
	        .hasTown = true,
	        .hasAbode = true,
	        .sexuallyActive = true,
	        .childrenWanted = 0.1f,
	        .children = 1,
	        .pregnantWomen = 0,
	        .childRoom = 3};
}
} // namespace

TEST(VillagerBreeding, OfAnAgeFromTheStartAgeUpToTheStop)
{
	EXPECT_FALSE(IsSexuallyActive(12, 13, 100));
	EXPECT_TRUE(IsSexuallyActive(13, 13, 100));
	EXPECT_TRUE(IsSexuallyActive(99, 13, 100));
	EXPECT_FALSE(IsSexuallyActive(100, 13, 100));
}

TEST(VillagerBreeding, PregnantIsAFemaleWithTurnsLeft)
{
	EXPECT_TRUE(IsPregnant(true, 999));
	EXPECT_TRUE(IsPregnant(true, -3));
	EXPECT_FALSE(IsPregnant(true, 0));
	EXPECT_FALSE(IsPregnant(false, 999));
	EXPECT_TRUE(IsWoman(true, false));
	EXPECT_FALSE(IsWoman(true, true));
	EXPECT_FALSE(IsWoman(false, false));
}

TEST(VillagerBreeding, AWomanConceivesAtHomeOnlyWhenTheTownWantsChildrenAndTheHomeHasRoom)
{
	EXPECT_TRUE(ConceivesAtHome(Fertile()));
	auto facts = Fertile();
	facts.childrenWanted = 0.0f;
	EXPECT_FALSE(ConceivesAtHome(facts));
	facts = Fertile();
	facts.pregnant = true;
	EXPECT_FALSE(ConceivesAtHome(facts));
	facts = Fertile();
	facts.hasTown = false;
	EXPECT_FALSE(ConceivesAtHome(facts));
	facts = Fertile();
	facts.sexuallyActive = false;
	EXPECT_FALSE(ConceivesAtHome(facts));
	// Children on the way take room as those born do
	facts = Fertile();
	facts.children = 2;
	facts.pregnantWomen = 1;
	EXPECT_FALSE(ConceivesAtHome(facts));
	facts.pregnantWomen = 0;
	EXPECT_TRUE(ConceivesAtHome(facts));
}

TEST(VillagerBreeding, BedTimeNeedsTheTownToFeelSleepWithAllItsHeart)
{
	EXPECT_TRUE(BedTimeMayConceive(true, 1.0f, true));
	EXPECT_FALSE(BedTimeMayConceive(true, 0.99f, true));
	EXPECT_FALSE(BedTimeMayConceive(false, 1.0f, true));
	EXPECT_FALSE(BedTimeMayConceive(true, 1.0f, false));
}

TEST(VillagerBreeding, AtBedTimeAWomanTriesOnceAndAManGivesEveryWomanInHerChance)
{
	// Newest first: a boy in, the woman going to bed, a man out, a girl in, another woman in
	const std::array<Housemate, 5> home {{
	    {.male = true, .atHome = true},
	    {.male = false, .atHome = true},
	    {.male = true, .atHome = false},
	    {.male = false, .atHome = true},
	    {.male = false, .atHome = true},
	}};
	// A boy counts as a male in for her, as in the game
	EXPECT_EQ(BedTimeConceptions(true, 1, home), (std::vector<size_t> {1}));
	const std::array<Housemate, 2> noMan {{{.male = true, .atHome = false}, {.male = false, .atHome = true}}};
	EXPECT_TRUE(BedTimeConceptions(true, 1, noMan).empty());
	// A man going to bed: every female in, girls too
	EXPECT_EQ(BedTimeConceptions(false, 2, home), (std::vector<size_t> {1, 3, 4}));
}

TEST(VillagerBreeding, APregnancyCountsDownByTheTurnsSinceTheLastCheck)
{
	EXPECT_EQ(CountDown(999, 9).pregnancy, 990);
	EXPECT_FALSE(CountDown(999, 9).birthDue);
	EXPECT_TRUE(CountDown(9, 9).birthDue);
	EXPECT_EQ(CountDown(5, 9).pregnancy, -4);
	EXPECT_TRUE(CountDown(5, 9).birthDue);
	// At home the countdown runs every turn by the turns since the last check, which isn't reset until the next check:
	// nine turns take off 1 + 2 + ... + 8 between checks, then 9 at the check
	int16_t pregnancy = 999;
	for (uint32_t since = 1; since <= 8; ++since)
	{
		pregnancy = CountDown(pregnancy, since).pregnancy;
	}
	pregnancy = CountDown(pregnancy, 9).pregnancy;
	EXPECT_EQ(pregnancy, 999 - 45);
}

TEST(VillagerBreeding, ABirthTakesAQuarterOfADayAndUpToADayMore)
{
	Draws<uint32_t> draws {.values = {0}};
	EXPECT_EQ(BirthTurns(k_TurnsPerDay, draws.Make()), 25);
	EXPECT_EQ(draws.limits, (std::vector<uint32_t> {98}));
	Draws<uint32_t> most {.values = {97}};
	EXPECT_EQ(BirthTurns(k_TurnsPerDay, most.Make()), 122);
}

TEST(VillagerBreeding, ANewbornIsABoyOfAJobHalfTheTimeElseAGirlOfHerMothersKind)
{
	Draws<uint32_t> boy {.values = {49, 0}};
	EXPECT_EQ(NewbornJob(49, boy.Make()), VillagerNumber::Forester);
	EXPECT_EQ(boy.limits, (std::vector<uint32_t> {100, 6}));
	Draws<uint32_t> trader {.values = {0, 5}};
	EXPECT_EQ(NewbornJob(49, trader.Make()), VillagerNumber::Trader);
	Draws<uint32_t> girl {.values = {50}};
	EXPECT_EQ(NewbornJob(49, girl.Make()), std::nullopt);
	EXPECT_EQ(girl.limits, (std::vector<uint32_t> {100}));
}

TEST(VillagerBreeding, LoversMakeLoveForHalfOfTheirTimesWithTheirOwnCountedTwice)
{
	EXPECT_EQ(LoveTurns(2, 2), 4);
	EXPECT_EQ(LoveTurns(4, 2), 5);
	EXPECT_EQ(LoveTurns(2, 4), 6);
}

TEST(VillagerBreeding, MeetingStopsAGapShortOfHalfway)
{
	const glm::vec2 from {100.0f, 100.0f};
	const glm::vec2 to {110.0f, 100.0f};
	const auto point = MeetingPoint(from, to, k_MeetingGap);
	EXPECT_NEAR(glm::distance(from, point), (10.0f - k_MeetingGap) * 0.5f, 0.01f);
	EXPECT_NEAR(glm::distance(point, MeetingPoint(to, from, k_MeetingGap)), k_MeetingGap, 0.02f);
	EXPECT_EQ(MeetingPoint(from, from, k_MeetingGap), from);
}

TEST(VillagerBreeding, TouchingIsEdgeToEdgeWithinAGap)
{
	EXPECT_TRUE(Touching(1.0f, 0.3f, 0.3f, k_KissingGap));
	EXPECT_TRUE(Touching(1.17f, 0.3f, 0.3f, k_KissingGap));
	EXPECT_FALSE(Touching(1.2f, 0.3f, 0.3f, k_KissingGap));
}

TEST(VillagerBreeding, TheFirstToArriveWaitsForItsPartnerWalkingUpToKiss)
{
	EXPECT_TRUE(WaitsForPartner(true, true));
	// Arrived, or gone off to something else: the kiss starts at once
	EXPECT_FALSE(WaitsForPartner(true, false));
	// Too far off: they set out to meet again, as in the game
	EXPECT_FALSE(WaitsForPartner(false, true));
	EXPECT_FALSE(WaitsForPartner(false, false));
}

TEST(VillagerBreeding, LoveEndsOnlyOnGoingOnToAnEndInItselfOutsideLove)
{
	EXPECT_TRUE(LeavesLove(false, true));
	// Walking to meet the partner again is on the way to love, not an end in itself
	EXPECT_FALSE(LeavesLove(false, false));
	EXPECT_FALSE(LeavesLove(true, true));
	EXPECT_FALSE(LeavesLove(true, false));
}

TEST(VillagerBreeding, ABreederBreedsUntilItsTownIsTooFarOverFull)
{
	const TownHeadcount full {.children = 10, .childPlaces = 10, .adults = 20, .adultPlaces = 20};
	EXPECT_TRUE(BreederMayBreed(full, 8.0f));
	EXPECT_TRUE(BreederMayBreed({.children = 17, .childPlaces = 10, .adults = 20, .adultPlaces = 20}, 8.0f));
	EXPECT_FALSE(BreederMayBreed({.children = 18, .childPlaces = 10, .adults = 20, .adultPlaces = 20}, 8.0f));
	EXPECT_TRUE(BreederMayBreed({.children = 0, .childPlaces = 10, .adults = 51, .adultPlaces = 20}, 8.0f));
	EXPECT_FALSE(BreederMayBreed({.children = 0, .childPlaces = 10, .adults = 52, .adultPlaces = 20}, 8.0f));
}

TEST(VillagerBreeding, ABreedersSpotByAHomeIsOutFromItsDoor)
{
	Draws<uint32_t> side {.values = {1}};
	Draws<float> floats {.values = {0.0f, 0.0f}};
	const glm::vec2 home {200.0f, 200.0f};
	const glm::vec2 door {200.0f, 205.0f};
	const auto spot = BreederSpotByHome(home, door, 6.0f, side.Make(), floats.Make());
	EXPECT_EQ(side.limits, (std::vector<uint32_t> {2}));
	EXPECT_EQ(floats.limits.size(), 2u);
	EXPECT_FLOAT_EQ(floats.limits.at(0), k_BreederFan);
	EXPECT_FLOAT_EQ(floats.limits.at(1), 3.0f);
	// Straight out from the door, half the home's size
	EXPECT_NEAR(spot.x, 200.0f, 0.05f);
	EXPECT_NEAR(spot.y, 208.0f, 0.05f);
}

TEST(VillagerBreeding, ABreedersSpotByTheStoreAndItsAmbles)
{
	Draws<float> store {.values = {1.0f, 0.0f}};
	const glm::vec2 pit {300.0f, 300.0f};
	EXPECT_NEAR(glm::distance(BreederSpotByStore(pit, store.Make()), pit), 4.0f, 0.05f);
	EXPECT_FLOAT_EQ(store.limits.at(0), k_BreederStoreRange);
	Draws<float> walk {.values = {0.0f, 6.0f}};
	Draws<uint32_t> pause {.values = {39}};
	const auto amble = BreederAmble(pit, walk.Make(), pause.Make());
	EXPECT_NEAR(glm::distance(amble.goal, pit), 6.0f, 0.05f);
	EXPECT_FLOAT_EQ(walk.limits.at(1), k_BreederAmble);
	EXPECT_EQ(amble.pause, 69);
	EXPECT_EQ(pause.limits, (std::vector<uint32_t> {40}));
}

TEST(VillagerBreeding, AChildMakesForAPointFiveMetresFromItsMother)
{
	Draws<float> angle {.values = {1.0f}};
	const glm::vec2 mother {400.0f, 400.0f};
	EXPECT_NEAR(glm::distance(NearMother(mother, angle.Make()), mother), k_NearMother, 0.05f);
}
