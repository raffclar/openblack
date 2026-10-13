/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ECS/VillagerWalkRules.h>
#include <gtest/gtest.h>

using namespace openblack;
namespace walk = openblack::ecs::villager_walk;

// The walks themselves and the dances walk a villager
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(VillagerWalkRules, TheWalksAndDancesWalkAVillager)
{
	EXPECT_TRUE(walk::Walks(VillagerStates::MoveToPos));
	EXPECT_TRUE(walk::Walks(VillagerStates::MoveToObject));
	EXPECT_TRUE(walk::Walks(VillagerStates::MoveOnPath));
	EXPECT_TRUE(walk::Walks(VillagerStates::MoveToDancePos));
	EXPECT_TRUE(walk::Walks(VillagerStates::ForesterMoveToForest));
	EXPECT_TRUE(walk::Walks(VillagerStates::MoveTowardsObjectToLookAt));
	EXPECT_TRUE(walk::Walks(VillagerStates::MoveTowardsCreatureReaction));
	EXPECT_TRUE(walk::Walks(VillagerStates::DanceWhileReacting));
	EXPECT_TRUE(walk::Walks(VillagerStates::WorshippingAtWorshipSite));
	EXPECT_TRUE(walk::Walks(VillagerStates::ArtifactDance));
}

// Kissing, looking, pausing, fleeing and watching don't: their walks wait
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(VillagerWalkRules, OtherStatesLeaveTheWalkWaiting)
{
	EXPECT_FALSE(walk::Walks(VillagerStates::StartHavingSex));
	EXPECT_FALSE(walk::Walks(VillagerStates::HavingSex));
	EXPECT_FALSE(walk::Walks(VillagerStates::LookingAtObjectReaction));
	EXPECT_FALSE(walk::Walks(VillagerStates::PauseForASecond));
	EXPECT_FALSE(walk::Walks(VillagerStates::FleeingFromObjectReaction));
	EXPECT_FALSE(walk::Walks(VillagerStates::FleeingAndLookingAtObjectReaction));
	EXPECT_FALSE(walk::Walks(VillagerStates::DecideWhatToDo));
	EXPECT_FALSE(walk::Walks(VillagerStates::MoveInFlock));
	EXPECT_FALSE(walk::Walks(VillagerStates::MoveAroundFire));
}
