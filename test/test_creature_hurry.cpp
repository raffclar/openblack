/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/LeashOrders.h"

using namespace openblack;
using creature_layers::BodyAction;

namespace
{
constexpr float k_Tolerance = 1e-4f;
} // namespace

TEST(CreatureHurry, AHurriedCreatureDoublesTheStepOfItsClock)
{
	using creature_layers::HurriedStep;
	EXPECT_FLOAT_EQ(HurriedStep(12.5f, false), 12.5f);
	EXPECT_FLOAT_EQ(HurriedStep(12.5f, true), 25.0f);
	EXPECT_EQ(HurriedStep(int32_t {37}, false), 37);
	EXPECT_EQ(HurriedStep(int32_t {37}, true), 74);
}

TEST(CreatureHurry, AnIndividualActionHurriesAndPlaysAtTwoTimesTheSpeed)
{
	auto body = *creature_layers::PlayOnce({}, creature_layers::animations::k_Happy, false);
	body = creature_layers::Hurry(body);
	EXPECT_TRUE(body.hurried);
	body = creature_layers::AdvanceBody(body, 100.0f, 1000);
	EXPECT_NEAR(body.timeMs, 200.0f, k_Tolerance);
}

TEST(CreatureHurry, AStaticPoseHurriesAndStaysHurriedToItsEnd)
{
	auto body = *creature_layers::PlaySequence({}, creature_layers::animations::k_StartSit, creature_layers::animations::k_Sit,
	                                           creature_layers::animations::k_EndSit);
	body = creature_layers::Hurry(body);
	body = creature_layers::EndLoop(body);
	EXPECT_TRUE(body.hurried);
}

TEST(CreatureHurry, AKissACastAndAConfusedLookKeepTheirPace)
{
	// The body marks each of these as one that keeps its pace when it starts
	auto body = *creature_layers::PlayOnce({}, creature_layers::animations::k_Happy, false);
	body.keepsPace = true;
	body = creature_layers::Hurry(body);
	EXPECT_FALSE(body.hurried);
	body = creature_layers::AdvanceBody(body, 100.0f, 1000);
	EXPECT_NEAR(body.timeMs, 100.0f, k_Tolerance);
}

TEST(CreatureHurry, OnlyTheActionsThatMoveTheBodyAboutAThingHurry)
{
	using creature_object_actions::Hurries;
	using creature_object_actions::Kind;
	EXPECT_TRUE(Hurries(Kind::PickUp));
	EXPECT_TRUE(Hurries(Kind::PutDown));
	EXPECT_TRUE(Hurries(Kind::Discard));
	EXPECT_TRUE(Hurries(Kind::Eat));
	EXPECT_TRUE(Hurries(Kind::Keep));
	EXPECT_TRUE(Hurries(Kind::Destroy));
	EXPECT_TRUE(Hurries(Kind::Point));
	EXPECT_FALSE(Hurries(Kind::Throw));
	EXPECT_FALSE(Hurries(Kind::Catch));
	EXPECT_FALSE(Hurries(Kind::TakeFromHand));
	EXPECT_FALSE(Hurries(Kind::Kick));
}

TEST(CreatureHurry, AFighterHurriesInItsStartStanceActionsAndFinish)
{
	using creature_fight::Hurries;
	using creature_fight::State;
	EXPECT_TRUE(Hurries(State::Start));
	EXPECT_TRUE(Hurries(State::Stance));
	EXPECT_TRUE(Hurries(State::Action));
	EXPECT_TRUE(Hurries(State::Finish));
	for (const auto state : {State::Idle, State::BlockStart, State::Block, State::BlockRecoil, State::BlockEnd, State::Faint,
	                         State::GetUp, State::CastStart, State::Cast, State::CastEnd, State::Lying})
	{
		EXPECT_FALSE(Hurries(state)) << creature_fight::Name(state);
	}
}

TEST(CreatureHurry, ALandOrderHurriesAndStopsUnlessItWalksToAPointInAForcedPlan)
{
	using creature_leash_orders::OnGroundInterrupts;
	const auto free = OnGroundInterrupts(false, false);
	EXPECT_TRUE(free.hurries);
	EXPECT_TRUE(free.stopsWalk);
	EXPECT_TRUE(free.endsBody);
	// Sent again while it walks to a point in a forced plan, it neither hurries nor stops, and its body goes on
	const auto again = OnGroundInterrupts(false, true);
	EXPECT_FALSE(again.hurries);
	EXPECT_FALSE(again.stopsWalk);
	EXPECT_FALSE(again.endsBody);
	// Carrying, it stops and does not hurry, whatever it does now
	for (const bool forcedWalk : {false, true})
	{
		const auto carrying = OnGroundInterrupts(true, forcedWalk);
		EXPECT_FALSE(carrying.hurries);
		EXPECT_TRUE(carrying.stopsWalk);
		EXPECT_TRUE(carrying.endsBody);
	}
}

TEST(CreatureHurry, AThingOrderHurriesAndLetsTheWalkContinueExceptToFish)
{
	using creature_leash_orders::Attempt;
	using creature_leash_orders::OnThingInterrupts;
	for (const auto& attempts : std::vector<std::vector<Attempt>> {
	         {Attempt::Hold}, {Attempt::Look}, {Attempt::Desires, Attempt::Fight, Attempt::Hold}, {}})
	{
		const auto order = OnThingInterrupts(attempts);
		EXPECT_TRUE(order.hurries);
		EXPECT_FALSE(order.stopsWalk);
		EXPECT_TRUE(order.endsBody);
	}
	const auto fish = OnThingInterrupts({Attempt::FishAndEat});
	EXPECT_FALSE(fish.hurries);
	EXPECT_TRUE(fish.stopsWalk);
	EXPECT_TRUE(fish.endsBody);
}

TEST(CreatureHurry, TheWalkBackHurriesAndLetsTheWalkContinue)
{
	constexpr auto k_WalkBack = creature_leash_orders::k_WalkBackInterrupts;
	EXPECT_TRUE(k_WalkBack.hurries);
	EXPECT_FALSE(k_WalkBack.stopsWalk);
	EXPECT_TRUE(k_WalkBack.endsBody);
}
