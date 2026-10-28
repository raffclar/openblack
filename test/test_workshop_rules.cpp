/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <array>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Registry.h"
#include "ECS/WorkshopRules.h"

using namespace openblack::ecs;
using namespace openblack::ecs::components;
using Place = Workshop::Place;

namespace
{
uint32_t BitsOf(float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

workshop::TurnFacts Facts(bool functional, int32_t room, uint32_t wood)
{
	return {.functional = functional, .room = room, .wood = wood, .scaffoldWood = 100};
}
} // namespace

// The scale of a scaffold of each worth, as the game's floats give it
TEST(WorkshopRules, ScaleOfValueMatchesTheGameFloats)
{
	constexpr std::array<uint32_t, 7> k_Bits {0x3F000000, 0x3F333333, 0x3F666666, 0x3F8CCCCD,
	                                          0x3FA66666, 0x3FC00000, 0x3FD9999A};
	for (uint32_t value = 0; value < k_Bits.size(); ++value)
	{
		EXPECT_EQ(BitsOf(workshop::ScaleOfValue(value)), k_Bits.at(value)) << value;
	}
}

// The worth of a scaffold comes back from its scale, also from the scale that a script gives
TEST(WorkshopRules, ValueOfScaleRoundsToTheNearestWorth)
{
	for (uint32_t value = 0; value < 7; ++value)
	{
		EXPECT_EQ(workshop::ValueOfScale(workshop::ScaleOfValue(value)), value) << value;
	}
	EXPECT_EQ(workshop::ValueOfScale(1.0f), 3u);
	EXPECT_EQ(workshop::ValueOfScale(0.79f), 1u);
	EXPECT_EQ(workshop::ValueOfScale(0.81f), 2u);
}

// The game cuts the fraction off the time to make a scaffold
TEST(WorkshopRules, MakingTurnsCutsTheFractionOff)
{
	EXPECT_EQ(workshop::MakingTurns(40.9f), 40);
	EXPECT_EQ(workshop::MakingTurns(40.0f), 40);
	EXPECT_EQ(workshop::MakingTurns(0.5f), 0);
}

// A workshop starts a scaffold only when it works, has room and has the wood for it
TEST(WorkshopRules, StartsWithRoomAndWood)
{
	int32_t turns = 0;
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 1, 100)), workshop::TurnEvent::Starts);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 1, 99)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 500)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(false, 3, 500)), workshop::TurnEvent::None);
	// The room can be less than 0, which counts as room
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, -1, 100)), workshop::TurnEvent::Starts);
	EXPECT_EQ(turns, 0);
}

// A workshop counts its turns down and finishes the scaffold at 0; it does not count while it does not work
TEST(WorkshopRules, CountsDownAndFinishes)
{
	int32_t turns = 3;
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(turns, 2);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(false, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(turns, 2);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::None);
	EXPECT_EQ(workshop::NextEvent(turns, Facts(true, 0, 0)), workshop::TurnEvent::Finishes);
	EXPECT_EQ(turns, 0);
}

TEST(WorkshopRules, FirstFreePlaceIsTheFirstPlaceWhenNoneIsFree)
{
	EXPECT_EQ(workshop::FirstFreePlace({Place::Free, Place::Free, Place::Free}), 0);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Away, Place::Free}), 2);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Free, Place::Free}), 1);
	EXPECT_EQ(workshop::FirstFreePlace({Place::Holds, Place::Away, Place::Holds}), 0);
}

// A workshop lets a scaffold go when it lay for the turns after a hand put it down, unless a script gave its building
TEST(WorkshopRules, KeepsScaffoldUntilItLayForTheTurns)
{
	// A scaffold that no hand put down lay for no turns
	EXPECT_TRUE(workshop::KeepsScaffold(0, 5000, 150, false));
	EXPECT_FALSE(workshop::KeepsScaffold(0, 5000, 0, false));
	EXPECT_TRUE(workshop::KeepsScaffold(1000, 1149, 150, false));
	EXPECT_FALSE(workshop::KeepsScaffold(1000, 1150, 150, false));
	EXPECT_TRUE(workshop::KeepsScaffold(1000, 1150, 150, true));
}

TEST(WorkshopRules, AddAndRemoveKeepTheCount)
{
	Registry registry;
	const auto a = registry.Create();
	const auto b = registry.Create();
	const auto c = registry.Create();
	Workshop data;
	workshop::AddScaffold(data, a);
	workshop::AddScaffold(data, b);
	workshop::AddScaffold(data, c);
	ASSERT_EQ(data.scaffolds.size(), 3u);
	EXPECT_EQ(data.scaffolds.front(), c);
	EXPECT_EQ(data.counted, 3u);
	data.places = {Place::Holds, Place::Holds, Place::Holds};
	// The yard is not full now: the town wants again what it needs
	EXPECT_TRUE(workshop::RemoveScaffold(data, b, 1));
	EXPECT_EQ(data.counted, 2u);
	EXPECT_EQ(data.places.at(1), Place::Free);
	// A scaffold that the workshop does not have changes nothing
	EXPECT_FALSE(workshop::RemoveScaffold(data, b, 0));
	EXPECT_EQ(data.places.at(0), Place::Holds);
	EXPECT_FALSE(workshop::RemoveScaffold(data, a, 0));
	EXPECT_EQ(data.counted, 1u);
	// None counts, but its count goes up
	workshop::AddScaffold(data, entt::null);
	EXPECT_EQ(data.scaffolds.size(), 1u);
	EXPECT_EQ(data.counted, 2u);
}

TEST(WorkshopRules, StandsInYardOnlyInAPlaceThatHoldsIt)
{
	Registry registry;
	const auto owner = registry.Create();
	auto& data = registry.Assign<Workshop>(owner);
	data.places = {Place::Free, Place::Holds, Place::Away};
	const auto held = registry.Create();
	registry.Assign<Scaffold>(held, Scaffold {.workshop = owner, .place = 1});
	const auto away = registry.Create();
	registry.Assign<Scaffold>(away, Scaffold {.workshop = owner, .place = 2});
	const auto loose = registry.Create();
	registry.Assign<Scaffold>(loose, Scaffold {});
	const auto other = registry.Create();
	EXPECT_TRUE(workshop::StandsInYard(registry, held));
	EXPECT_FALSE(workshop::StandsInYard(registry, away));
	EXPECT_FALSE(workshop::StandsInYard(registry, loose));
	EXPECT_FALSE(workshop::StandsInYard(registry, other));
}

TEST(WorkshopRules, OnlyTheWoodPileOfAWorkshopIsItsWoodPile)
{
	Registry registry;
	const auto owner = registry.Create();
	const auto pile = registry.Create();
	const auto other = registry.Create();
	registry.Assign<Workshop>(owner).woodPile = pile;
	registry.Assign<Workshop>(other);
	EXPECT_TRUE(workshop::IsWoodPile(registry, pile));
	EXPECT_FALSE(workshop::IsWoodPile(registry, other));
	EXPECT_FALSE(workshop::IsWoodPile(registry, entt::null));
}

TEST(WorkshopRules, TheSpecialPointIsThePlaceOfTheSpecialPlaceWithItsHeight)
{
	glm::mat4 point {1.0f};
	point[3] = glm::vec4(1.5f, 2.0f, -3.25f, 1.0f);
	const auto building =
	    workshop::BuildingMatrix(glm::vec3(0.0f, 0.75f, 0.0f), false, 1.2f, glm::vec3(2000.0f, 40.0f, 2100.0f));
	const auto world = workshop::SpecialPointOf(point, building);
	const auto place = workshop::SpecialPlaceOf(point, building);
	const auto coords = openblack::map_coords::FromMetres(glm::vec2(world.x, world.z));
	EXPECT_EQ(coords.x, place.coords.x);
	EXPECT_EQ(coords.z, place.coords.z);
	// The height is the building's height and the point's height, by the building's scale
	EXPECT_NEAR(world.y, 40.0f + (2.0f * 1.2f), 1e-4f);
}

TEST(WorkshopRules, TheHandJoinsScaffoldsOnlyWhenTheirSumIsBelowTheMost)
{
	const workshop::JoinFacts facts {.heldValue = 2, .targetValue = 3, .mostForCombining = 6};
	EXPECT_TRUE(workshop::CanJoin(facts));
	// The two together must be worth less than the most, or as much with the football feature
	EXPECT_FALSE(workshop::CanJoin({.heldValue = 3, .targetValue = 3, .mostForCombining = 6}));
	EXPECT_TRUE(workshop::CanJoin({.heldValue = 3, .targetValue = 3, .mostForCombining = 6, .football = true}));
	// A table row with no most lets any sum through
	EXPECT_TRUE(workshop::CanJoin({.heldValue = 40, .targetValue = 50, .mostForCombining = 0}));
}

TEST(WorkshopRules, TheHandDoesNotJoinScaffoldsOfScriptsOrInThePhysicsOrOnAStartedSite)
{
	const workshop::JoinFacts facts {.heldValue = 1, .targetValue = 1, .mostForCombining = 6};
	auto script = facts;
	script.heldScriptBuilding = true;
	EXPECT_FALSE(workshop::CanJoin(script));
	script = facts;
	script.targetScriptBuilding = true;
	EXPECT_FALSE(workshop::CanJoin(script));
	auto physics = facts;
	physics.targetInPhysics = true;
	EXPECT_FALSE(workshop::CanJoin(physics));
	auto site = facts;
	site.targetOnSite = true;
	EXPECT_TRUE(workshop::CanJoin(site));
	site.targetSiteBuilt = 0.01f;
	EXPECT_FALSE(workshop::CanJoin(site));
}

TEST(WorkshopRules, AJoinedScaffoldBuildsForTheTownOfTheScaffoldWorthMore)
{
	using workshop::JoinedTown;
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = false, .targetHasTown = true}), JoinedTown::Target);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = false, .targetHasTown = false}), JoinedTown::Target);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = true, .targetHasTown = false}), JoinedTown::Held);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = true, .targetHasTown = true, .heldValue = 3, .targetValue = 2}),
	          JoinedTown::Held);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = true, .targetHasTown = true, .heldValue = 2, .targetValue = 3}),
	          JoinedTown::Target);
	// When the two are worth the same, the held town only when the player who holds it has that town
	EXPECT_EQ(workshop::TownOfJoined(
	              {.heldHasTown = true, .targetHasTown = true, .heldValue = 2, .targetValue = 2, .holderOwnsHeldTown = true}),
	          JoinedTown::Held);
	EXPECT_EQ(workshop::TownOfJoined({.heldHasTown = true, .targetHasTown = true, .heldValue = 2, .targetValue = 2}),
	          JoinedTown::Target);
}
