/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string_view>

#include <gtest/gtest.h>

#include "Creature/CreaturePlanActions.h"

using namespace openblack;

namespace
{
// The dance actions that the dance skill lets a creature do
constexpr std::array<std::string_view, 10> k_DanceActions {
    "DanceWithVillagers",
    "PracticeDance",
    "DanceOutsideWorshipSite",
    "DanceAroundArtefact",
    "PlayGameWithVillagers",
    "DanceOnYourOwnByTheSea",
    "DancePlayfullyWithVillagersWatch",
    "DancePlayfullyWithVillagersParti",
    "TellVillagersAStory",
    "DanceAmorouslyWithVillagers",
};

// The fish actions that the fishing skill lets a creature do
constexpr std::array<std::string_view, 4> k_FishActions {
    "FishAndEat",
    "GiveFishToStoragePit",
    "TakeFishHome",
    "PutFishByWorshipSite",
};

// The actions that the scripts of the game force a creature to do
constexpr std::array<std::string_view, 28> k_ScriptActions {
    "LookAtHand",
    "EatAlive",
    "StompAndEat",
    "Hurl",
    "SleepAtHome",
    "ExamineByLooking",
    "Fight",
    "RaiseTotemPole",
    "SmileAtFriend",
    "PutDown",
    "SleepByObject",
    "SitDown",
    "LookAtCamera",
    "WaveAtPlayer",
    "WaveAtObject",
    "CastFireballPU1",
    "CastLightningBoltPU1",
    "CastHealSpellPU1",
    "CastMakeCreatureItchy",
    "EatFromFoodPile",
    "BeIdle",
    "PointAtObject",
    "PointAtCamera",
    "BeFrightenedOnTheSpot",
    "Fart",
    "LookButDontApproach",
    "LookForever",
    "DiePermanently",
};

// The actions that nothing in the game starts: no desire list, no copy of a deed, no script and no part of the game
// itself. The creature must not do them.
constexpr std::array<std::string_view, 16> k_ActionsThatNeverStart {
    "PutOutFire",     "DestroyAggressor", "SleepAtPos",       "RunToObject",          "HelpBuildHouse",   "BuildHome",
    "RunAwayFromPos", "WatchTelly",       "HurlObjectInHand", "StealSpell",           "StealScaffolding", "SteaSpellSeed",
    "StealAnimal",    "StealVillager",    "CastTeleport",     "CastTeleportAndUseIt",
};

// The gesture actions. No desire and no shipped script starts them, but a script that forces one gets the drawing of
// the circle.
constexpr std::array<std::string_view, 12> k_GestureActions {
    "GestureFullCircle", "GestureStar",    "GestureSpiral", "GestureSquareWave", "GestureScribble", "GestureSquare",
    "GestureTriangle",   "GestureS-Shape", "GestureV-Ball", "GestureMoon",       "GestureHeart",    "GestureBowTie",
};
} // namespace

TEST(CreatureActionCoverage, EveryDanceAndFishActionHasSteps)
{
	for (const auto action : k_DanceActions)
	{
		EXPECT_NE(creature_plan_actions::For(action), nullptr) << action;
	}
	for (const auto action : k_FishActions)
	{
		EXPECT_NE(creature_plan_actions::For(action), nullptr) << action;
	}
}

TEST(CreatureActionCoverage, EveryActionThatAScriptForcesHasSteps)
{
	for (const auto action : k_ScriptActions)
	{
		EXPECT_NE(creature_plan_actions::For(action), nullptr) << action;
	}
	EXPECT_NE(creature_plan_actions::For("DeadForever"), nullptr);
}

TEST(CreatureActionCoverage, ActionsThatNothingStartsHaveNoSteps)
{
	for (const auto action : k_ActionsThatNeverStart)
	{
		EXPECT_EQ(creature_plan_actions::For(action), nullptr) << action;
	}
}

TEST(CreatureActionCoverage, EveryGestureActionHasTheStepsOfTheCircle)
{
	for (const auto action : k_GestureActions)
	{
		const auto* executor = creature_plan_actions::For(action);
		ASSERT_NE(executor, nullptr) << action;
		EXPECT_EQ(executor->build, creature_plan_actions::Build::Gesture) << action;
		EXPECT_EQ(executor->target, creature_plan_actions::Target::None) << action;
	}
}
