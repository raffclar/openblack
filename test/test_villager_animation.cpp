/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "ECS/VillagerAnimation.h"

using namespace openblack;
using namespace openblack::ecs::villager_animation;

namespace
{
/// Hands out the given draws in turn, recording the limits asked for
struct Rolls
{
	std::vector<uint32_t> values;
	std::vector<uint32_t> limits;
	size_t next {0};
	IntRandom Random()
	{
		return [this](uint32_t limit) {
			limits.push_back(limit);
			return values.at(next++);
		};
	}
};

WalkInputs Walker(float speed, bool female = false)
{
	return {.female = female, .speed = speed, .walkMax = 3.0f, .runMax = 4.0f};
}
} // namespace

TEST(VillagerAnimation, StatesChooseTheirClipsAsTheGameDoes)
{
	EXPECT_EQ(ClipChoiceOf(VillagerStates::MoveToPos), ClipChoice::Walk);
	EXPECT_EQ(ClipChoiceOf(VillagerStates::FleeingFromPredatorReaction), ClipChoice::Walk);
	EXPECT_EQ(ClipChoiceOf(VillagerStates::AfterTapOnAbode), ClipChoice::Yawn);
	EXPECT_EQ(ClipChoiceOf(VillagerStates::SitAndChillout), ClipChoice::SitDown);
	// Going home is walking only through the moving states it sets up
	EXPECT_EQ(ClipChoiceOf(VillagerStates::GoHome), ClipChoice::Table);
	EXPECT_EQ(TransitionOf(VillagerStates::SleepInTent), Transition::SleepOnTheGround);
	EXPECT_EQ(TransitionOf(VillagerStates::MoveOnPath), Transition::Walk);
	EXPECT_EQ(TransitionOf(VillagerStates::AtHome), Transition::None);
	EXPECT_TRUE(IsDeathState(VillagerStates::SetDying));
	EXPECT_TRUE(IsDeathState(VillagerStates::Downed));
	EXPECT_FALSE(IsDeathState(VillagerStates::BeingEaten));
	EXPECT_FALSE(IsDeathState(VillagerStates::LookAtFlyingObjectReaction));
}

TEST(VillagerAnimation, VillagersWalkRunOrSprintBySpeedAndSex)
{
	EXPECT_EQ(WalkClip(Walker(2.0f)), AnimId::PWalkMan);
	EXPECT_EQ(WalkClip(Walker(3.0f)), AnimId::PWalkMan);
	EXPECT_EQ(WalkClip(Walker(3.5f)), AnimId::PRunMan);
	EXPECT_EQ(WalkClip(Walker(5.0f)), AnimId::PSprintRunMan);
	EXPECT_EQ(WalkClip(Walker(2.0f, true)), AnimId::PWalkWoman);
	EXPECT_EQ(WalkClip(Walker(3.5f, true)), AnimId::PRunWoman);
	EXPECT_EQ(WalkClip(Walker(5.0f, true)), AnimId::PSprintRunWoman);
}

TEST(VillagerAnimation, TheHurtCrawlOrLimpUnlessAScriptMovesThem)
{
	auto in = Walker(2.0f);
	in.life = 0.1f;
	EXPECT_EQ(WalkClip(in), AnimId::PCrawlInjured);
	in.life = 0.3f;
	EXPECT_EQ(WalkClip(in), AnimId::PWalkInjured);
	in.scriptControlled = true;
	EXPECT_EQ(WalkClip(in), AnimId::PWalkMan);
}

TEST(VillagerAnimation, ALoadInTheArmsIsCarriedWalkingOrRunning)
{
	auto in = Walker(2.0f, true);
	in.carried = CarriedObject::Wood;
	EXPECT_EQ(WalkClip(in), AnimId::PCarryAxe);
	in.speed = 5.0f;
	EXPECT_EQ(WalkClip(in), AnimId::PCarryObjectRun);
	// A saw, the ball and a hammer don't change the walk
	in.carried = CarriedObject::Hammer;
	EXPECT_EQ(WalkClip(in), AnimId::PSprintRunWoman);
	EXPECT_FALSE(CarriesInArms(CarriedObject::Saw));
	EXPECT_TRUE(CarriesInArms(CarriedObject::Crook));
}

TEST(VillagerAnimation, TheDyingFallAndTheBody)
{
	EXPECT_EQ(DyingClip(true, true), AnimId::PIntoDeadDrowned);
	EXPECT_EQ(DyingClip(false, true), AnimId::PDead2);
	EXPECT_EQ(DyingClip(false, false), AnimId::PDying);
	EXPECT_EQ(DeadClip(true, false), AnimId::PDeadDrowned);
	EXPECT_EQ(DeadClip(false, true), AnimId::PDead2);
	EXPECT_EQ(DeadClip(false, false), AnimId::PDead1);
}

TEST(VillagerAnimation, RandomClipsDrawOnceOnTheGamesStream)
{
	Rolls yawn {.values = {1, 0}};
	EXPECT_EQ(YawnClip(yawn.Random()), AnimId::PYawn2);
	EXPECT_EQ(YawnClip(yawn.Random()), AnimId::PYawn);
	EXPECT_EQ(yawn.limits, (std::vector<uint32_t> {2, 2}));

	Rolls pause {.values = {1}};
	EXPECT_EQ(PauseForASecondClip(true, pause.Random()), AnimId::PPoisoned);
	EXPECT_TRUE(pause.limits.empty());
	EXPECT_EQ(PauseForASecondClip(false, pause.Random()), AnimId::POverworked2);

	Rolls emergency {.values = {3, 3, 0}};
	EXPECT_EQ(TownEmergencyClip(false, emergency.Random()), AnimId::PPanicMan);
	EXPECT_EQ(TownEmergencyClip(true, emergency.Random()), AnimId::PPanicWoman);
	EXPECT_EQ(TownEmergencyClip(true, emergency.Random()), AnimId::PAttractYourAttention);
}

TEST(VillagerAnimation, InspectingTheCreatureDrawsOnlyWhatItNeeds)
{
	// A man far from the creature only decides between talking and standing
	Rolls far {.values = {3}};
	EXPECT_EQ(InspectCreatureClip(false, std::nullopt, far.Random()), AnimId::PTalkingAndPointing);
	EXPECT_EQ(far.limits, (std::vector<uint32_t> {8}));
	// A woman near it is first scared stiff one time in three, then looks at it one time in three
	Rolls near {.values = {1, 0}};
	EXPECT_EQ(InspectCreatureClip(true, 5.0f, near.Random()), AnimId::PLookAtHand);
	EXPECT_EQ(near.limits, (std::vector<uint32_t> {3, 3}));
}

TEST(VillagerAnimation, SittingDownMatchesTheWayItSat)
{
	Rolls none;
	EXPECT_EQ(SitDownClip(true, AnimId::PSittingDown1Into, none.Random()), AnimId::PSittingDown1Sitting);
	EXPECT_EQ(SitDownClip(true, AnimId::PSittingDown2Into, none.Random()), AnimId::PSittingDown2Sitting);
	Rolls one {.values = {1}};
	EXPECT_EQ(SitDownClip(false, AnimId::PStand, one.Random()), AnimId::PSittingDown2Sitting);
	EXPECT_EQ(TransitionClip(Transition::SitDown, true, false, false, AnimId::PSittingDown1Sitting), AnimId::PSittingDown1Into);
	EXPECT_EQ(TransitionClip(Transition::SitDown, false, false, false, AnimId::PSittingDown2Sitting),
	          AnimId::PSittingDown2OutOf);
}

TEST(VillagerAnimation, BuildersCarryTheToolOfTheirStroke)
{
	Rolls saw {.values = {1}};
	const auto choice = BuildingClip(false, AnimId::PStand, saw.Random());
	EXPECT_EQ(choice.clip, AnimId::PSawWood);
	EXPECT_EQ(choice.tool, CarriedObject::Saw);
	Rolls none;
	EXPECT_EQ(BuildingClip(true, AnimId::PSledgehammer, none.Random()).tool, CarriedObject::MalletHeavy);
	EXPECT_EQ(TransitionClip(Transition::Building, false, false, false, AnimId::PHammering), AnimId::POutOfHammering);
	EXPECT_EQ(TransitionClip(Transition::Building, true, false, false, AnimId::PStand), std::nullopt);
}

TEST(VillagerAnimation, OnlyACrawlerGetsUpOrGoesDownIntoACrawl)
{
	EXPECT_EQ(TransitionClip(Transition::Walk, false, false, false, AnimId::PWalkMan), std::nullopt);
	EXPECT_EQ(TransitionClip(Transition::Walk, false, false, false, AnimId::PCrawlInjured), AnimId::POutOfSleep);
	EXPECT_EQ(TransitionClip(Transition::Walk, true, false, false, AnimId::PCrawlInjured), AnimId::PCrawlInjuredInto);
	EXPECT_EQ(TransitionClip(Transition::Walk, false, false, true, AnimId::PCrawlInjured), std::nullopt);
	// Picking up sticks only on the way out, and only to another state
	EXPECT_EQ(TransitionClip(Transition::ArrivesAtResource, false, false, false, AnimId::PStand), AnimId::PPickUpSticks);
	EXPECT_EQ(TransitionClip(Transition::ArrivesAtResource, false, true, false, AnimId::PStand), std::nullopt);
	EXPECT_EQ(TransitionClip(Transition::SleepOnTheGround, true, false, false, AnimId::PStand), AnimId::PIntoSleep);
}

TEST(VillagerAnimation, WhatAVillagerCarries)
{
	CarryInputs in {.wood = 60, .food = 200, .minWoodToShowGraphic = 50, .minFoodToShowGraphic = 100};
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Wood);
	in.woodKind = 2;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Tree_2);
	in.wood = 50;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Bag);
	in.building = true;
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::None);
	// The badly hurt show nothing but what their states give them, the top state's before the final one's
	in.life = 0.1f;
	in.finalStateCarries = static_cast<int32_t>(CarriedObject::Spade);
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Spade);
	in.topStateCarries = static_cast<int32_t>(CarriedObject::Axe);
	EXPECT_EQ(CarriedObjectOf(in), CarriedObject::Axe);
	in.scriptHeld = true;
	EXPECT_EQ(CarriedObjectOf(in), std::nullopt);
}

TEST(VillagerAnimation, LookingUpAtFlyingThings)
{
	EXPECT_EQ(LookAtFlyingObjectClip(k_HiddenClip, 10.0f), k_HiddenClip);
	EXPECT_EQ(LookAtFlyingObjectClip(385, 10.0f), static_cast<int32_t>(AnimId::PLookAtHand));
	EXPECT_EQ(LookAtFlyingObjectClip(385, 2.0f), static_cast<int32_t>(AnimId::PStand));
}
