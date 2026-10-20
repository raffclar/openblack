/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
