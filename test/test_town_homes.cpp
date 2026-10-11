/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a town's people live: the building that suits a newcomer best, the homeless, couples, moving house and towns,
// and evening out a town's homes

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/TownHomes.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using town_homes::AbodeRoom;
using town_homes::Occupancy;

namespace
{
constexpr float k_Epsilon = 1e-6f;
constexpr uint32_t k_TownId = 3;

class TownHomesTest: public ::testing::Test
{
protected:
	TownHomesTest()
	{
		town = registry.Create();
		registry.Assign<Town>(town, Town {.id = k_TownId});
		registry.Context().towns[k_TownId] = town;
	}

	entt::entity MakeAbode(glm::vec3 position, AbodeRoom room)
	{
		const auto abode = registry.Create();
		registry.Assign<Abode>(abode, Abode {.townId = k_TownId});
		registry.Assign<Transform>(abode, position, glm::mat3(1.0f), glm::vec3(1.0f));
		// The newest building first, as the town keeps them
		auto& abodes = registry.Get<Town>(town).abodes;
		abodes.insert(abodes.begin(), abode);
		rooms[abode] = room;
		return abode;
	}

	entt::entity MakeVillager(glm::vec3 position, Villager::Sex sex, Villager::LifeStage stage = Villager::LifeStage::Adult)
	{
		const auto villager = registry.Create();
		registry.Assign<Villager>(villager, Villager {.lifeStage = stage, .sex = sex, .town = entt::null, .abode = entt::null});
		registry.Assign<Transform>(villager, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return villager;
	}

	[[nodiscard]] town_homes::Homes Rooms()
	{
		return {.room = [this](entt::entity abode) -> std::optional<AbodeRoom> {
			        const auto found = rooms.find(abode);
			        return found == rooms.end() ? std::nullopt : std::optional<AbodeRoom>(found->second);
		        },
		        .leaving = [this](entt::entity villager) { left.push_back(villager); }};
	}

	[[nodiscard]] const std::vector<entt::entity>& Homeless() const { return registry.Get<const Town>(town).homelessVillagers; }

	Registry registry;
	entt::entity town {entt::null};
	std::map<entt::entity, AbodeRoom> rooms;
	/// Who was about to leave a home, in turn
	std::vector<entt::entity> left;
};
} // namespace

TEST(TownHomesScore, ARoomierBuildingOfTheOtherSexNearbyIsLikedBetter)
{
	// Nearness counts half: all of it here, none 500 m off and further
	const float here = (gutils::GetDistanceModifier(0.0f, 500.0f) + 1.0f) * 0.5f;
	const float far = (gutils::GetDistanceModifier(500.0f, 500.0f) + 1.0f) * 0.5f;
	EXPECT_NEAR(here, 1.0f, 1e-4f);
	EXPECT_NEAR(far, 0.5f, 1e-4f);
	// Empty: all of it
	EXPECT_NEAR(town_homes::ScoreForAddingVillager({.maxAdults = 4}), here, k_Epsilon);
	// A quarter full, all of the other sex: three quarters
	EXPECT_NEAR(town_homes::ScoreForAddingVillager({.adults = 1, .maxAdults = 4, .people = 1}), 0.75f * here, k_Epsilon);
	// The same sex halves it
	EXPECT_NEAR(town_homes::ScoreForAddingVillager({.adults = 1, .maxAdults = 4, .people = 1, .sameSex = 1}), 0.375f * here,
	            k_Epsilon);
	// Full, or over full, or unable to house its age: nothing
	EXPECT_EQ(town_homes::ScoreForAddingVillager({.adults = 4, .maxAdults = 4, .people = 4}), 0.0f);
	EXPECT_EQ(town_homes::ScoreForAddingVillager({.adults = 6, .maxAdults = 4, .people = 6}), 0.0f);
	EXPECT_EQ(town_homes::ScoreForAddingVillager({.maxAdults = 0}), 0.0f);
	// A child counts the children's room only
	EXPECT_EQ(town_homes::ScoreForAddingVillager({.child = true, .maxAdults = 4, .maxChildren = 0}), 0.0f);
	EXPECT_NEAR(town_homes::ScoreForAddingVillager(
	                {.child = true, .adults = 4, .children = 1, .maxAdults = 4, .maxChildren = 2, .people = 5}),
	            0.5f * here, k_Epsilon);
	EXPECT_NEAR(town_homes::ScoreForAddingVillager({.maxAdults = 4, .distance = 500.0f}), far, k_Epsilon);
	EXPECT_EQ(town_homes::ScoreForAddingVillager({.maxAdults = 4, .distance = 2000.0f}),
	          town_homes::ScoreForAddingVillager({.maxAdults = 4, .distance = 500.0f}));
	const float near = town_homes::ScoreForAddingVillager({.maxAdults = 4, .distance = 100.0f});
	EXPECT_GT(near, far);
	EXPECT_LT(near, here);
}

TEST_F(TownHomesTest, ANewcomerMovesIntoTheBestFunctionalBuildingTheFirstOfEqualsInTheTownsOrder)
{
	const glm::vec3 here(100.0f, 0.0f, 100.0f);
	const auto broken = MakeAbode(here, {.maxAdults = 4, .maxChildren = 2, .functional = false});
	const auto older = MakeAbode(here, {.maxAdults = 4, .maxChildren = 2, .functional = true});
	const auto newer = MakeAbode(here, {.maxAdults = 4, .maxChildren = 2, .functional = true});
	const auto villager = MakeVillager(here, Villager::Sex::MALE);

	EXPECT_TRUE(town_homes::AddVillagerToTown(registry, Rooms(), town, villager));
	// The two equal: the newer, first in the town's list
	EXPECT_EQ(registry.Get<Villager>(villager).abode, newer);
	EXPECT_EQ(registry.Get<Villager>(villager).town, town);
	EXPECT_EQ(registry.Get<Abode>(newer).inhabitants, std::vector<entt::entity> {villager});
	EXPECT_TRUE(Homeless().empty());

	// The next man prefers the older, which has no man in it yet
	const auto second = MakeVillager(here, Villager::Sex::MALE);
	town_homes::AddVillagerToTown(registry, Rooms(), town, second);
	EXPECT_EQ(registry.Get<Villager>(second).abode, older);
	EXPECT_TRUE(registry.Get<Abode>(broken).inhabitants.empty());
}

TEST_F(TownHomesTest, WithNoRoomTheNewcomerIsHomelessTheNewestFirst)
{
	const glm::vec3 here(0.0f);
	MakeAbode(here, {.maxAdults = 1, .maxChildren = 0, .functional = true});
	const auto first = MakeVillager(here, Villager::Sex::FEMALE);
	const auto second = MakeVillager(here, Villager::Sex::FEMALE);
	const auto third = MakeVillager(here, Villager::Sex::MALE);
	const auto child = MakeVillager(here, Villager::Sex::MALE, Villager::LifeStage::Child);
	town_homes::AddVillagerToTown(registry, Rooms(), town, first);
	town_homes::AddVillagerToTown(registry, Rooms(), town, second);
	town_homes::AddVillagerToTown(registry, Rooms(), town, third);
	town_homes::AddVillagerToTown(registry, Rooms(), town, child);
	EXPECT_NE(registry.Get<Villager>(first).abode, entt::entity {entt::null});
	EXPECT_EQ(registry.Get<Villager>(second).abode, entt::entity {entt::null});
	EXPECT_EQ(registry.Get<Villager>(second).town, town);
	EXPECT_EQ(Homeless(), (std::vector<entt::entity> {child, third, second}));
	// Joining again changes nothing
	EXPECT_FALSE(town_homes::MakeHomeless(registry, Rooms(), second));
	EXPECT_EQ(Homeless().size(), 3u);
}

TEST_F(TownHomesTest, NobodyJoinsATownNobodyCanLiveIn)
{
	registry.Get<Town>(town).uninhabitable = true;
	MakeAbode(glm::vec3(0.0f), {.maxAdults = 4, .functional = true});
	const auto villager = MakeVillager(glm::vec3(0.0f), Villager::Sex::MALE);
	EXPECT_FALSE(town_homes::AddVillagerToTown(registry, Rooms(), town, villager));
	EXPECT_EQ(registry.Get<Villager>(villager).town, entt::entity {entt::null});
	EXPECT_TRUE(Homeless().empty());
}

TEST_F(TownHomesTest, AHomelessVillagerMovingInLeavesTheHomelessAndOneMadeHomelessKeepsItsTown)
{
	const glm::vec3 here(0.0f);
	const auto villager = MakeVillager(here, Villager::Sex::MALE);
	town_homes::AddVillagerToTown(registry, Rooms(), town, villager);
	ASSERT_EQ(Homeless(), std::vector<entt::entity> {villager});

	const auto abode = MakeAbode(here, {.maxAdults = 2, .functional = true});
	ASSERT_EQ(town_homes::FindAbodeWithSpace(registry, Rooms(), town, villager, 0.0f), abode);
	town_homes::AddVillagerToAbode(registry, Rooms(), abode, villager);
	EXPECT_TRUE(Homeless().empty());
	EXPECT_EQ(registry.Get<Villager>(villager).abode, abode);

	// Newest first in the building too
	const auto other = MakeVillager(here, Villager::Sex::FEMALE);
	town_homes::AddVillagerToAbode(registry, Rooms(), abode, other);
	EXPECT_EQ(registry.Get<Abode>(abode).inhabitants, (std::vector<entt::entity> {other, villager}));
	EXPECT_EQ(registry.Get<Villager>(other).town, town);

	// Leaving its home, it is told first
	left.clear();
	EXPECT_TRUE(town_homes::MakeHomeless(registry, Rooms(), villager));
	EXPECT_EQ(left, std::vector<entt::entity> {villager});
	EXPECT_EQ(registry.Get<Villager>(villager).abode, entt::entity {entt::null});
	EXPECT_EQ(registry.Get<Villager>(villager).town, town);
	EXPECT_EQ(registry.Get<Abode>(abode).inhabitants, std::vector<entt::entity> {other});
	EXPECT_EQ(Homeless(), std::vector<entt::entity> {villager});
}

namespace
{
/// A second town, and buildings of it
class TwoTownsTest: public TownHomesTest
{
protected:
	static constexpr uint32_t k_OtherId = 7;

	TwoTownsTest()
	{
		other = registry.Create();
		registry.Assign<Town>(other, Town {.id = k_OtherId});
		registry.Context().towns[k_OtherId] = other;
	}

	entt::entity MakeOtherAbode(glm::vec3 position, AbodeRoom room, AbodeNumber type = AbodeNumber::A)
	{
		const auto abode = registry.Create();
		registry.Assign<Abode>(abode, Abode {.type = type, .townId = k_OtherId});
		registry.Assign<Transform>(abode, position, glm::mat3(1.0f), glm::vec3(1.0f));
		auto& abodes = registry.Get<Town>(other).abodes;
		abodes.insert(abodes.begin(), abode);
		rooms[abode] = room;
		return abode;
	}

	entt::entity other {entt::null};
};

constexpr AbodeRoom k_Hut {.maxAdults = 4, .maxChildren = 2, .functional = true, .built = true, .tooCrowded = 0.75f};
} // namespace

TEST_F(TownHomesTest, TheFirstAdultManAndWomanToMoveInAreTheCoupleUntilOneDies)
{
	const glm::vec3 here(0.0f);
	const auto abode = MakeAbode(here, k_Hut);
	const auto boy = MakeVillager(here, Villager::Sex::MALE, Villager::LifeStage::Child);
	const auto man = MakeVillager(here, Villager::Sex::MALE);
	const auto second = MakeVillager(here, Villager::Sex::MALE);
	const auto woman = MakeVillager(here, Villager::Sex::FEMALE);
	for (const auto villager : {boy, man, second, woman})
	{
		town_homes::AddVillagerToTown(registry, Rooms(), town, villager);
		ASSERT_EQ(registry.Get<Villager>(villager).abode, abode);
	}
	// A child is no part of it, and the second man comes too late
	EXPECT_EQ(registry.Get<Abode>(abode).couple, (std::array {man, woman}));
	EXPECT_EQ(town_homes::SpouseOf(registry, abode, man), woman);
	EXPECT_EQ(town_homes::SpouseOf(registry, abode, woman), man);
	EXPECT_EQ(town_homes::SpouseOf(registry, abode, second), entt::entity {entt::null});
	EXPECT_EQ(town_homes::SpouseOf(registry, abode, boy), entt::entity {entt::null});
	const auto household = town_homes::HouseholdOf(registry, abode);
	EXPECT_EQ(household.adults, 3u);
	EXPECT_EQ(household.adultMen, 2u);
	EXPECT_EQ(household.children, 1u);

	// Moving out alive he keeps his place: no newcomer takes it
	town_homes::MakeHomeless(registry, Rooms(), man);
	const auto third = MakeVillager(here, Villager::Sex::MALE);
	town_homes::AddVillagerToAbode(registry, Rooms(), abode, third);
	EXPECT_EQ(registry.Get<Abode>(abode).couple, (std::array {man, woman}));

	// Her death ends it, and she leaves her home and town without a word to her
	left.clear();
	town_homes::RemoveDeadVillager(registry, woman);
	EXPECT_EQ(registry.Get<Abode>(abode).couple, (std::array {entt::entity {entt::null}, entt::entity {entt::null}}));
	EXPECT_EQ(registry.Get<Villager>(woman).abode, entt::entity {entt::null});
	EXPECT_EQ(registry.Get<Villager>(woman).town, entt::entity {entt::null});
	EXPECT_TRUE(left.empty());
	// The next man and woman to move in are the new couple
	const auto fourth = MakeVillager(here, Villager::Sex::FEMALE);
	town_homes::RemoveDeadVillager(registry, third);
	town_homes::AddVillagerToAbode(registry, Rooms(), abode, fourth);
	EXPECT_EQ(registry.Get<Abode>(abode).couple, (std::array {entt::entity {entt::null}, fourth}));
}

TEST_F(TownHomesTest, ADeadHomelessVillagerLeavesTheHomeless)
{
	const auto villager = MakeVillager(glm::vec3(0.0f), Villager::Sex::FEMALE);
	town_homes::AddVillagerToTown(registry, Rooms(), town, villager);
	ASSERT_EQ(Homeless(), std::vector<entt::entity> {villager});
	town_homes::RemoveDeadVillager(registry, villager);
	EXPECT_TRUE(Homeless().empty());
	EXPECT_EQ(registry.Get<Villager>(villager).town, entt::entity {entt::null});
}

TEST(TownHomesCrowding, TooCrowdedAtItsShareOfPlacesOrWithNoPlaces)
{
	EXPECT_FALSE(town_homes::IsTooCrowded(2, 4, 0.75f));
	EXPECT_TRUE(town_homes::IsTooCrowded(3, 4, 0.75f));
	EXPECT_TRUE(town_homes::IsTooCrowded(5, 4, 0.75f));
	EXPECT_TRUE(town_homes::IsTooCrowded(0, 0, 0.75f));
	EXPECT_FALSE(town_homes::IsTooCrowded(3, 4, 1.0f));
}

TEST_F(TownHomesTest, ACrowdedAdultMovesToABetterHomeOrIsHomeless)
{
	const glm::vec3 here(0.0f);
	const auto crowded = MakeAbode(here, k_Hut);
	std::vector<entt::entity> people;
	for (int i = 0; i < 3; ++i)
	{
		people.push_back(MakeVillager(here, Villager::Sex::MALE));
		town_homes::AddVillagerToAbode(registry, Rooms(), crowded, people.back());
	}
	registry.Get<Villager>(people.front()).town = town;
	const auto child = MakeVillager(here, Villager::Sex::MALE, Villager::LifeStage::Child);
	town_homes::AddVillagerToAbode(registry, Rooms(), crowded, child);
	EXPECT_EQ(town_homes::LookForBetterHome(registry, Rooms(), child), town_homes::NewHome::Stays);

	// Three of four is past three quarters: with nowhere better it is homeless
	const auto mover = people.front();
	EXPECT_EQ(town_homes::LookForBetterHome(registry, Rooms(), mover), town_homes::NewHome::MadeHomeless);
	EXPECT_EQ(Homeless(), std::vector<entt::entity> {mover});

	// Two of four is not crowded
	EXPECT_EQ(town_homes::LookForBetterHome(registry, Rooms(), people[1]), town_homes::NewHome::Stays);

	// A new hut suits the homeless one best, and it moves in
	const auto roomy = MakeAbode(here, k_Hut);
	EXPECT_EQ(town_homes::LookForBetterHome(registry, Rooms(), mover), town_homes::NewHome::Moved);
	EXPECT_EQ(registry.Get<Villager>(mover).abode, roomy);
	EXPECT_TRUE(Homeless().empty());

	// Without a town it has nowhere to look
	const auto vagrant = MakeVillager(here, Villager::Sex::FEMALE);
	EXPECT_EQ(town_homes::LookForBetterHome(registry, Rooms(), vagrant), town_homes::NewHome::NoTown);
}

TEST_F(TownHomesTest, ACrowdedAdultOnlyMovesToAHomeItLikesBetterThanItsOwn)
{
	const glm::vec3 here(0.0f);
	// Its own, half full, is too crowded at half; another has room but three men in it, which it likes less
	const AbodeRoom half {.maxAdults = 4, .functional = true, .built = true, .tooCrowded = 0.5f};
	const auto own = MakeAbode(here, half);
	const auto others = MakeAbode(here, half);
	const auto villager = MakeVillager(here, Villager::Sex::MALE);
	town_homes::AddVillagerToAbode(registry, Rooms(), own, villager);
	town_homes::AddVillagerToAbode(registry, Rooms(), own, MakeVillager(here, Villager::Sex::FEMALE));
	for (int i = 0; i < 3; ++i)
	{
		town_homes::AddVillagerToAbode(registry, Rooms(), others, MakeVillager(here, Villager::Sex::MALE));
	}
	EXPECT_EQ(town_homes::LookForBetterHome(registry, Rooms(), villager), town_homes::NewHome::MadeHomeless);
	EXPECT_EQ(registry.Get<Abode>(others).inhabitants.size(), 3u);
}

TEST_F(TownHomesTest, AHomelessAdultWithNowhereToGoStaysHomeless)
{
	const auto villager = MakeVillager(glm::vec3(0.0f), Villager::Sex::MALE);
	town_homes::AddVillagerToTown(registry, Rooms(), town, villager);
	EXPECT_EQ(town_homes::LookForBetterHome(registry, Rooms(), villager), town_homes::NewHome::AlreadyHomeless);
	EXPECT_EQ(Homeless(), std::vector<entt::entity> {villager});
}

TEST_F(TwoTownsTest, MovingToAnotherTownsHouseLeavesTheOldTownAndAFullHouseOnlyTakesItIntoTheTown)
{
	const glm::vec3 here(0.0f);
	const auto home = MakeAbode(here, k_Hut);
	const auto villager = MakeVillager(here, Villager::Sex::MALE);
	town_homes::AddVillagerToTown(registry, Rooms(), town, villager);
	ASSERT_EQ(registry.Get<Villager>(villager).abode, home);

	const auto tiny = MakeOtherAbode(here, {.maxAdults = 1, .functional = true, .built = true});
	const auto owner = MakeVillager(here, Villager::Sex::FEMALE);
	town_homes::AddVillagerToTown(registry, Rooms(), other, owner);
	ASSERT_EQ(registry.Get<Villager>(owner).abode, tiny);

	// Full: it joins the town, finds no home there and is one of its homeless
	town_homes::MoveInAnyway(registry, Rooms(), tiny, villager);
	EXPECT_EQ(registry.Get<Villager>(villager).town, other);
	EXPECT_EQ(registry.Get<Villager>(villager).abode, entt::entity {entt::null});
	EXPECT_TRUE(registry.Get<Abode>(home).inhabitants.empty());
	EXPECT_EQ(registry.Get<const Town>(other).homelessVillagers, std::vector<entt::entity> {villager});

	// Into one with room it moves in, leaving the homeless
	const auto hut = MakeOtherAbode(here, k_Hut);
	EXPECT_TRUE(town_homes::MoveInByBuilding(registry, Rooms(), hut, villager, 0));
	EXPECT_EQ(registry.Get<Villager>(villager).abode, hut);
	EXPECT_TRUE(registry.Get<const Town>(other).homelessVillagers.empty());

	// And back home to its first town
	EXPECT_TRUE(town_homes::MoveInByBuilding(registry, Rooms(), home, villager, 0));
	EXPECT_EQ(registry.Get<Villager>(villager).town, town);
	EXPECT_EQ(registry.Get<Abode>(home).inhabitants, std::vector<entt::entity> {villager});
	EXPECT_TRUE(registry.Get<Abode>(hut).inhabitants.empty());
}

TEST_F(TwoTownsTest, MovingIntoATownWithRoomButNoHomeToSuitMakesForItsVillageCentre)
{
	const glm::vec3 here(0.0f);
	// A damaged hut has room but takes nobody; the village centre houses nobody
	const auto centre = MakeOtherAbode(here, {.functional = true, .built = true}, AbodeNumber::TownCentre);
	MakeOtherAbode(here, {.maxAdults = 4, .functional = false, .built = true});
	const auto villager = MakeVillager(here, Villager::Sex::MALE);
	town_homes::AddVillagerToTown(registry, Rooms(), town, villager);
	EXPECT_TRUE(town_homes::HasRoomForNewcomer(registry, Rooms(), other, 0));
	// The centre has no places, so it only joins the town, one of its homeless
	EXPECT_TRUE(town_homes::MoveIntoTown(registry, Rooms(), other, villager, 0));
	EXPECT_EQ(registry.Get<Villager>(villager).town, other);
	EXPECT_EQ(registry.Get<Villager>(villager).abode, entt::entity {entt::null});
	EXPECT_TRUE(Homeless().empty());
	EXPECT_EQ(registry.Get<const Town>(other).homelessVillagers, std::vector<entt::entity> {villager});
	EXPECT_TRUE(registry.Get<Abode>(centre).inhabitants.empty());
}

TEST_F(TwoTownsTest, ATownWithNoHomesBuiltLetsInAFewPeople)
{
	const glm::vec3 here(0.0f);
	MakeOtherAbode(here, {.maxAdults = 4, .functional = false, .built = false});
	EXPECT_TRUE(town_homes::HasRoomForNewcomer(registry, Rooms(), other, 1));
	EXPECT_FALSE(town_homes::HasRoomForNewcomer(registry, Rooms(), other, 0));
	const auto villager = MakeVillager(here, Villager::Sex::MALE);
	town_homes::AddVillagerToTown(registry, Rooms(), other, villager);
	EXPECT_FALSE(town_homes::HasRoomForNewcomer(registry, Rooms(), other, 1));
	EXPECT_FALSE(town_homes::MoveIntoTown(registry, Rooms(), town, MakeVillager(here, Villager::Sex::MALE), 0));
}

TEST(TownHomesShuffle, WantsOfMenAndPeopleAgainstTheTown)
{
	// A town of 3 men and 3 women in 8 places; a hut of 4 places with 2 men
	const town_homes::TownPeople town {.men = 3, .women = 3, .adults = 6, .adultPlaces = 8};
	const auto wants = town_homes::WantsOf(town, {.adults = 2, .adultMen = 2}, 4);
	const float townMen = (3.0f + 0.001f) / (3.0f + 0.001f);
	const float ownMen = (2.0f + 0.001f) / (0.0f + 0.001f);
	EXPECT_FLOAT_EQ(wants.men, townMen - ownMen);
	EXPECT_FLOAT_EQ(wants.people, ((6.0f + 0.001f) / (8.0f + 0.001f) - 0.5f) * 0.5f);
	EXPECT_TRUE(wants.roomForAdult);
	// No places for adults: it wants nothing and has no room
	const auto none = town_homes::WantsOf(town, {.children = 1}, 0);
	EXPECT_EQ(none.men, 0.0f);
	EXPECT_EQ(none.people, 0.0f);
	EXPECT_FALSE(none.roomForAdult);
	EXPECT_FALSE(town_homes::WantsOf(town, {.adults = 4}, 4).roomForAdult);
}

TEST(TownHomesShuffle, AHutOfMenAndAHutOfWomenSwapAManForAWoman)
{
	using town_homes::ShuffleMove;
	// Equal want of people; one wants women, the other men
	const std::vector<town_homes::ShuffleWants> wants {{.men = -1.0f, .people = 0.1f, .roomForAdult = true},
	                                                   {.men = 1.0f, .people = 0.1f, .roomForAdult = true}};
	const auto moves = town_homes::EvenOutMoves(wants);
	ASSERT_EQ(moves.size(), 1u);
	EXPECT_EQ(moves.front(), (ShuffleMove {.kind = ShuffleMove::Kind::Swap, .to = 1, .from = 0}));
}

TEST(TownHomesShuffle, WhenOnlyOneWantsPeopleAndHasRoomOneVillagerMoves)
{
	using town_homes::ShuffleMove;
	// The empty hut wants people and a man; the full one has too many and wants women
	const std::vector<town_homes::ShuffleWants> wants {{.men = -0.2f, .people = -0.3f, .roomForAdult = false},
	                                                   {.men = 0.1f, .people = 0.4f, .roomForAdult = true}};
	auto moves = town_homes::EvenOutMoves(wants);
	ASSERT_EQ(moves.size(), 1u);
	EXPECT_EQ(moves.front(), (ShuffleMove {.kind = ShuffleMove::Kind::Take, .to = 1, .from = 0, .man = true}));
	// Without room it swaps instead
	auto full = wants;
	full[1].roomForAdult = false;
	moves = town_homes::EvenOutMoves(full);
	ASSERT_EQ(moves.size(), 1u);
	EXPECT_EQ(moves.front().kind, ShuffleMove::Kind::Swap);
	// Pairs that would only make things worse aren't tried
	const std::vector<town_homes::ShuffleWants> alike {{.men = 0.5f, .people = 0.5f}, {.men = 0.5f, .people = 0.5f}};
	EXPECT_TRUE(town_homes::EvenOutMoves(alike).empty());
}

TEST(TownHomesShuffle, TheMostOutOfBalanceAreTriedFirst)
{
	using town_homes::ShuffleMove;
	const std::vector<town_homes::ShuffleWants> wants {{.men = 0.1f, .people = 0.0f},
	                                                   {.men = -0.1f, .people = 0.0f},
	                                                   {.men = 2.0f, .people = 0.0f},
	                                                   {.men = -2.0f, .people = 0.0f}};
	const auto moves = town_homes::EvenOutMoves(wants);
	ASSERT_GE(moves.size(), 2u);
	// The hut wanting two more men pairs with the one wanting two more women first
	EXPECT_EQ(moves[0], (ShuffleMove {.kind = ShuffleMove::Kind::Swap, .to = 2, .from = 3}));
}

TEST(TownHomesShuffle, EachTownEvensOutItsHomesOnItsOwnTurn)
{
	EXPECT_TRUE(town_homes::EvenOutDue(0, 5000, 5000));
	EXPECT_FALSE(town_homes::EvenOutDue(0, 5001, 5000));
	// Town 3 a little earlier than town 0: 60 turns
	EXPECT_TRUE(town_homes::EvenOutDue(3, 4940, 5000));
	EXPECT_FALSE(town_homes::EvenOutDue(3, 5000, 5000));
	EXPECT_FALSE(town_homes::EvenOutDue(0, 0, 0));
}

TEST_F(TownHomesTest, ShufflingSwapsOnlyThoseOutOfDoors)
{
	const glm::vec3 here(0.0f);
	const auto men = MakeAbode(here, k_Hut);
	const auto women = MakeAbode(here, k_Hut);
	std::vector<entt::entity> people;
	for (const auto [abode, sex] : {std::pair {men, Villager::Sex::MALE}, std::pair {men, Villager::Sex::MALE},
	                                std::pair {women, Villager::Sex::FEMALE}, std::pair {women, Villager::Sex::FEMALE}})
	{
		people.push_back(MakeVillager(here, sex));
		registry.Get<Villager>(people.back()).town = town;
		town_homes::AddVillagerToAbode(registry, Rooms(), abode, people.back());
	}
	std::vector<entt::entity> inside {people[0], people[1]};
	auto homes = Rooms();
	homes.atHome = [&inside](entt::entity villager) { return std::ranges::find(inside, villager) != inside.end(); };
	// Both men are in: nothing moves
	EXPECT_FALSE(town_homes::EvenOutHomes(registry, homes, town));
	// The newer man out: he and the first woman out of the other swap
	inside = {people[0]};
	EXPECT_TRUE(town_homes::EvenOutHomes(registry, homes, town));
	EXPECT_EQ(registry.Get<Villager>(people[1]).abode, women);
	EXPECT_EQ(registry.Get<Villager>(people[3]).abode, men);
	EXPECT_EQ(registry.Get<Abode>(men).inhabitants, (std::vector<entt::entity> {people[3], people[0]}));
	EXPECT_EQ(registry.Get<Abode>(women).inhabitants, (std::vector<entt::entity> {people[1], people[2]}));
}

TEST_F(TownHomesTest, AMothersChildrenAreFoundBuildingByBuildingThenAmongTheHomeless)
{
	const glm::vec3 here(0.0f);
	const auto older = MakeAbode(here, k_Hut);
	const auto newer = MakeAbode(here, k_Hut);
	const auto mother = MakeVillager(here, Villager::Sex::FEMALE);
	const auto other = MakeVillager(here, Villager::Sex::FEMALE);
	const auto inOlder = MakeVillager(here, Villager::Sex::MALE, Villager::LifeStage::Child);
	const auto firstInNewer = MakeVillager(here, Villager::Sex::FEMALE, Villager::LifeStage::Child);
	const auto secondInNewer = MakeVillager(here, Villager::Sex::MALE, Villager::LifeStage::Child);
	const auto homeless = MakeVillager(here, Villager::Sex::MALE, Villager::LifeStage::Child);
	const auto someoneElses = MakeVillager(here, Villager::Sex::MALE, Villager::LifeStage::Child);
	for (const auto child : {inOlder, firstInNewer, secondInNewer, homeless})
	{
		registry.Get<Villager>(child).mother = mother;
	}
	registry.Get<Villager>(someoneElses).mother = other;
	registry.Get<Abode>(older).inhabitants = {inOlder, someoneElses};
	// Each building keeps its people newest first
	registry.Get<Abode>(newer).inhabitants = {secondInNewer, firstInNewer};
	registry.Get<Town>(town).homelessVillagers = {homeless};

	EXPECT_EQ(town_homes::ChildrenOf(registry, town, mother),
	          (std::vector<entt::entity> {secondInNewer, firstInNewer, inOlder, homeless}));
	EXPECT_EQ(town_homes::ChildrenOf(registry, town, other), std::vector<entt::entity> {someoneElses});
	EXPECT_TRUE(town_homes::ChildrenOf(registry, entt::null, mother).empty());
}

TEST_F(TwoTownsTest, AMotherLeavingHerTownAliveIsToldFirstWithTheTownSheLeaves)
{
	const glm::vec3 here(0.0f);
	MakeAbode(here, k_Hut);
	const auto mother = MakeVillager(here, Villager::Sex::FEMALE);
	town_homes::AddVillagerToTown(registry, Rooms(), town, mother);
	const auto hut = MakeOtherAbode(here, k_Hut);

	std::vector<std::pair<entt::entity, entt::entity>> told;
	auto homes = Rooms();
	homes.leavingTown = [&told](entt::entity villager, entt::entity from) { told.emplace_back(villager, from); };
	town_homes::MoveInAnyway(registry, homes, hut, mother);
	EXPECT_EQ(told, (std::vector<std::pair<entt::entity, entt::entity>> {{mother, town}}));
	EXPECT_EQ(registry.Get<Villager>(mother).town, other);
}
