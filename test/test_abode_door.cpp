/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/AbodeDoor.h"

using namespace openblack;
using namespace openblack::ecs::abode_door;

TEST(AbodeDoor, TheVillageCentreHasAFixedDoorAndAFieldHasNone)
{
	EXPECT_EQ(DoorOf(AbodeType::TownCentre), Door::VillageCentre);
	EXPECT_EQ(DoorOf(AbodeType::Field), Door::Middle);
	EXPECT_EQ(DoorOf(AbodeType::LivingQuarters), Door::Model);
	EXPECT_EQ(DoorOf(AbodeType::Creche), Door::Model);
	EXPECT_EQ(DoorOf(AbodeType::Graveyard), Door::Model);
	EXPECT_EQ(DoorOf(AbodeType::StoragePit), Door::Model);
	EXPECT_EQ(DoorOf(AbodeType::Workshop), Door::Model);
	EXPECT_EQ(DoorOf(AbodeType::Wonder), Door::Model);
}

TEST(AbodeDoor, VillagersArriveAtTheMiddleOfSomeCivicBuildings)
{
	EXPECT_TRUE(ArrivesAtMiddle(AbodeType::Creche));
	EXPECT_TRUE(ArrivesAtMiddle(AbodeType::Wonder));
	EXPECT_TRUE(ArrivesAtMiddle(AbodeType::Graveyard));
	EXPECT_TRUE(ArrivesAtMiddle(AbodeType::TownCentre));
	EXPECT_TRUE(ArrivesAtMiddle(AbodeType::Field));
	EXPECT_FALSE(ArrivesAtMiddle(AbodeType::LivingQuarters));
	EXPECT_FALSE(ArrivesAtMiddle(AbodeType::Windmill));
	EXPECT_FALSE(ArrivesAtMiddle(AbodeType::StoragePit));
	EXPECT_FALSE(ArrivesAtMiddle(AbodeType::Workshop));
	EXPECT_FALSE(ArrivesAtMiddle(AbodeType::SpellDispenser));
}

TEST(AbodeDoor, TheVillageCentreDoorIsTwoMetresAcrossAndTwelveBack)
{
	const auto door = VillageCentreDoor({100.0f, 5.0f, 200.0f}, glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(door.x, 102.0f);
	EXPECT_EQ(door.y, 188.0f);
}

TEST(AbodeDoor, TheVillageCentreDoorTurnsAndGrowsWithTheCentre)
{
	// A quarter turn: the centre's x axis points along the world's -z, its z axis along the world's x
	const glm::mat3 quarter(glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	const auto door = VillageCentreDoor({100.0f, 0.0f, 200.0f}, quarter, glm::vec3(0.5f));
	EXPECT_EQ(door.x, 94.0f);
	EXPECT_EQ(door.y, 199.0f);
}

TEST(AbodeDoor, AFieldAndAFishFarmHaveTheirMiddleAsTheirDoor)
{
	EXPECT_EQ(DoorOfThing(std::nullopt, true), Door::Middle);
	EXPECT_EQ(DoorOfThing(AbodeType::Graveyard, true), Door::Middle);
	// A building of the abodes has the door of its kind, and any other thing its model's door
	EXPECT_EQ(DoorOfThing(AbodeType::TownCentre, false), Door::VillageCentre);
	EXPECT_EQ(DoorOfThing(AbodeType::Graveyard, false), Door::Model);
	EXPECT_EQ(DoorOfThing(std::nullopt, false), Door::Model);
}

TEST(AbodeDoor, AModelDoorChangesFromMetresToWholeMapUnits)
{
	const openblack::map_coords::MapCoords middle {.x = 0x1FB5355, .z = 0x400001};
	// 12.5 metres is 81920 map units; -3.25 metres is -21299.2, which goes to -21299
	const auto door = DoorPlace(Door::Model, glm::vec2(12.5f, -3.25f), middle);
	EXPECT_EQ(door.x, 81920);
	EXPECT_EQ(door.z, -21299);
	EXPECT_EQ(door.altitude, 0.0f);
}

TEST(AbodeDoor, ABuildingWithNoDoorKeepsEveryUnitOfItsMapPosition)
{
	// Far out on the map, metres cannot hold each map unit: 0x1FB5355 in metres and back is 0x1FB5356
	const openblack::map_coords::MapCoords middle {.x = 0x1FB5355, .z = 0x1234567, .altitude = 0.0f};
	EXPECT_EQ(DoorPlace(Door::Model, std::nullopt, middle), middle);
	EXPECT_EQ(DoorPlace(Door::Middle, glm::vec2(12.5f, -3.25f), middle), middle);
	EXPECT_NE(openblack::map_coords::ToFixed(openblack::map_coords::ToMetres(middle.x)), middle.x);
}

TEST(AbodeDoor, AModelDoorOnAnAxisOfTheMapCountsAsNoDoor)
{
	const openblack::map_coords::MapCoords middle {.x = 0x320000, .z = 0x330000};
	// 0.0001 metres is less than one map unit, so x is 0
	EXPECT_EQ(DoorPlace(Door::Model, glm::vec2(0.0001f, 40.0f), middle), middle);
	EXPECT_EQ(DoorPlace(Door::Model, glm::vec2(40.0f, 0.0f), middle), middle);
	// The village centre door has no such test
	const auto centre = DoorPlace(Door::VillageCentre, glm::vec2(0.0001f, 40.0f), middle);
	EXPECT_EQ(centre.x, 0);
	EXPECT_EQ(centre.z, 262144);
}
