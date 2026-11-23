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

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ECS/VillagerBreedingRules.h"

using namespace openblack;
using namespace openblack::ecs::villager_breeding;

namespace
{
/// The distance in metres between two map positions
float Metres(const map_coords::MapCoords& a, const map_coords::MapCoords& b)
{
	return gutils::GetDistanceInMetres(a, b);
}

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
	const auto from = map_coords::FromMetres({100.0f, 100.0f});
	const auto to = map_coords::FromMetres({110.0f, 100.0f});
	const auto point = MeetingPoint(from, to, k_MeetingGap);
	EXPECT_NEAR(Metres(from, point), (10.0f - k_MeetingGap) * 0.5f, 0.01f);
	EXPECT_NEAR(Metres(point, MeetingPoint(to, from, k_MeetingGap)), k_MeetingGap, 0.02f);
	EXPECT_EQ(MeetingPoint(from, from, k_MeetingGap), from);
}

TEST(VillagerBreeding, ACoupleNearOrTooNearMeetsAtTheGap)
{
	// A couple that touches already steps nearer, and a couple that stands too near steps back
	for (const float distance : {1.0f, 0.3f})
	{
		const auto from = map_coords::FromMetres({100.0f, 100.0f});
		const auto to = map_coords::FromMetres({100.0f + distance, 100.0f});
		const auto point = MeetingPoint(from, to, k_MeetingGap);
		EXPECT_NEAR(Metres(point, MeetingPoint(to, from, k_MeetingGap)), k_MeetingGap, 0.02f);
		EXPECT_EQ(point.x < from.x, distance < k_MeetingGap);
	}
}

// The meeting point starts at the map position of the villager, not at its metres, and keeps its altitude. A map
// position that does not come back the same through metres shows it.
TEST(VillagerBreeding, AMeetingPointStartsAtTheMapPositionOfTheVillager)
{
	constexpr int32_t k_Unround = 8090858;
	static_assert(map_coords::ToFixed(map_coords::ToMetres(k_Unround)) != k_Unround);
	const map_coords::MapCoords from {.x = k_Unround, .z = k_Unround, .altitude = 0.5f};
	const map_coords::MapCoords to {.x = k_Unround + 65536, .z = k_Unround, .altitude = 0.0f};
	const auto point = MeetingPoint(from, to, k_MeetingGap);
	// Half of the map distance less the gap, along x, truncated to map units
	EXPECT_EQ(point.x, k_Unround + map_coords::ToFixedGUtils((Metres(from, to) - k_MeetingGap) * 0.5f));
	EXPECT_EQ(point.z, k_Unround);
	EXPECT_FLOAT_EQ(point.altitude, 0.5f);
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
	const auto home = map_coords::FromMetres({200.0f, 200.0f});
	const auto door = map_coords::FromMetres({200.0f, 205.0f});
	const auto spot = BreederSpotByHome(home, door, 6.0f, side.Make(), floats.Make());
	EXPECT_EQ(side.limits, (std::vector<uint32_t> {2}));
	EXPECT_EQ(floats.limits.size(), 2u);
	EXPECT_FLOAT_EQ(floats.limits.at(0), k_BreederFan);
	EXPECT_FLOAT_EQ(floats.limits.at(1), 3.0f);
	// Straight out from the door, half the home's size
	const auto metres = map_coords::ToMetres(spot);
	EXPECT_NEAR(metres.x, 200.0f, 0.05f);
	EXPECT_NEAR(metres.y, 208.0f, 0.05f);
}

TEST(VillagerBreeding, ABreedersSpotByAHomeAddsItsStepToTheDoorInWholeMapUnits)
{
	// A door far out on the map, whose last unit metres cannot keep, with a height above the land
	const map_coords::MapCoords home {.x = 0x1FB5355, .z = 0x1234567 - 0x8000, .altitude = 0.0f};
	const map_coords::MapCoords door {.x = 0x1FB5355, .z = 0x1234567, .altitude = 1.5f};
	Draws<uint32_t> side {.values = {0}};
	Draws<float> floats {.values = {0.25f, 1.0f}};
	const auto spot = BreederSpotByHome(home, door, 6.0f, side.Make(), floats.Make());
	// The other side: the fan draw turns the way out back by a quarter of a radian
	const float angle = 0.25f * -1.0f + gutils::Get3DAngleFromXZ(home, door);
	const auto step = gutils::GetPosFromAngle(angle, 1.0f + 3.0f);
	EXPECT_EQ(spot.x, door.x + step.x);
	EXPECT_EQ(spot.z, door.z + step.z);
	EXPECT_EQ(spot.altitude, 1.5f);
}

TEST(VillagerBreeding, ABreedersSpotByTheStoreAndItsAmbles)
{
	Draws<float> store {.values = {1.0f, 0.0f}};
	const map_coords::MapCoords pit {.x = 0x1FB5355, .z = 0x1234567, .altitude = 0.5f};
	const auto spot = BreederSpotByStore(pit, store.Make());
	EXPECT_FLOAT_EQ(store.limits.at(0), k_BreederStoreRange);
	EXPECT_FLOAT_EQ(store.limits.at(1), 6.2831855f);
	// Four metres along x, added to the store in whole map units
	const auto step = gutils::GetPosFromAngle(0.0f, 4.0f);
	EXPECT_EQ(spot.x, pit.x + step.x);
	EXPECT_EQ(spot.z, pit.z);
	EXPECT_EQ(spot.altitude, 0.5f);
	Draws<float> walk {.values = {0.0f, 6.0f}};
	Draws<uint32_t> pause {.values = {39}};
	const glm::ivec2 kept {0x320000, 0x330000};
	const auto amble = BreederAmble(kept, walk.Make(), pause.Make());
	// The amble goes through metres from the kept spot, on the land
	map_coords::MapCoords goal {.x = kept.x, .z = kept.y, .altitude = 0.0f};
	gutils::AddDistanceFromAngle(goal, 0.0f, 6.0f);
	EXPECT_EQ(amble.goal, goal);
	EXPECT_NEAR(map_coords::ToMetres(amble.goal.x) - map_coords::ToMetres(kept.x), 6.0f, 0.05f);
	EXPECT_FLOAT_EQ(walk.limits.at(1), k_BreederAmble);
	EXPECT_EQ(amble.pause, 69);
	EXPECT_EQ(pause.limits, (std::vector<uint32_t> {40}));
}

TEST(VillagerBreeding, AChildMakesForAPointFiveMetresFromItsMother)
{
	Draws<float> angle {.values = {1.0f}};
	const map_coords::MapCoords mother {.x = map_coords::ToFixed(400.0f), .z = map_coords::ToFixed(400.0f), .altitude = 0.0f};
	const auto goal = NearMother(mother, angle.Make());
	EXPECT_NEAR(glm::distance(map_coords::ToMetres(goal), map_coords::ToMetres(mother)), k_NearMother, 0.05f);
	EXPECT_EQ(angle.limits, (std::vector<float> {6.2831855f}));
}

// The step adds to the map position of the mother in whole map units, and the goal keeps her altitude
TEST(VillagerBreeding, AChildsGoalAddsItsStepToTheMothersMapPositionInWholeUnits)
{
	Draws<float> angle {.values = {1.0f}};
	const map_coords::MapCoords mother {.x = 0x1FB5355, .z = 0x1234567, .altitude = 2.5f};
	const auto goal = NearMother(mother, angle.Make());
	const auto step = gutils::GetPosFromAngle(1.0f, k_NearMother);
	EXPECT_EQ(goal.x, mother.x + step.x);
	EXPECT_EQ(goal.z, mother.z + step.z);
	EXPECT_EQ(goal.altitude, 2.5f);
}

TEST(VillagerBreeding, OnlyADyingVillagerCannotBeCalledOn)
{
	EXPECT_FALSE(CanBeCalledOn(VillagerStates::Dying));
	// Held, in flight or dead, a villager stays about
	EXPECT_TRUE(CanBeCalledOn(VillagerStates::InHand));
	EXPECT_TRUE(CanBeCalledOn(VillagerStates::Flying));
	EXPECT_TRUE(CanBeCalledOn(VillagerStates::Dead));
	EXPECT_TRUE(CanBeCalledOn(VillagerStates::DecideWhatToDo));
}

TEST(VillagerBreeding, AChildKeepsItsMotherWhileTheHandHoldsHer)
{
	const MotherFacts held {.available = CanBeCalledOn(VillagerStates::InHand), .sameTribe = true, .female = true};
	EXPECT_TRUE(KeepsMother(held));
	auto dying = held;
	dying.dying = true;
	EXPECT_FALSE(KeepsMother(dying));
	auto otherTribe = held;
	otherTribe.sameTribe = false;
	EXPECT_FALSE(KeepsMother(otherTribe));
	auto male = held;
	male.female = false;
	EXPECT_FALSE(KeepsMother(male));
	auto gone = held;
	gone.available = CanBeCalledOn(VillagerStates::Dying);
	EXPECT_FALSE(KeepsMother(gone));
}
