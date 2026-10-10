/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a town's people live: the building that suits a newcomer best, and the homeless

#include <map>
#include <optional>
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
