/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/LeashOrders.h"

using namespace openblack;
using namespace openblack::creature_leash_orders;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
using creature_mind::Step;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr uint32_t k_Creature = 1;
constexpr uint32_t k_Cow = 2;
constexpr uint32_t k_Tree = 3;

Leash Held()
{
	return {.worn = true, .creature = k_Creature};
}

Leash TiedTo(uint32_t object)
{
	return {.worn = true, .tiedTo = object, .creature = k_Creature};
}
} // namespace

TEST(LeashOrders, ASingleTapGivesAnOrderAndNeverTies)
{
	EXPECT_EQ(OnTap(Held(), {.object = k_Cow}), Tap::OrderOnThing);
	EXPECT_EQ(OnTap(Held(), {}), Tap::OrderOnLand);
	// The creature's own taps are the hand's
	EXPECT_EQ(OnTap(Held(), {.object = k_Creature}), Tap::Normal);
	// Posts and worship icons aren't ordered on
	EXPECT_EQ(OnTap(Held(), {.object = k_Cow, .leashTarget = false}), Tap::Normal);
	// Tied, or without the leash, a tap is an ordinary tap
	EXPECT_EQ(OnTap(TiedTo(k_Tree), {.object = k_Tree}), Tap::Normal);
	EXPECT_EQ(OnTap(TiedTo(k_Tree), {.object = k_Cow}), Tap::Normal);
	EXPECT_EQ(OnTap({}, {.object = k_Cow}), Tap::Normal);
	// Nor is a game with another creature broken into
	auto playing = Held();
	playing.playingGame = true;
	EXPECT_EQ(OnTap(playing, {.object = k_Cow}), Tap::Normal);
}

TEST(LeashOrders, ADoubleTapTiesAndUnties)
{
	EXPECT_EQ(OnDoubleTap(Held(), {.object = k_Tree}), Tap::Tie);
	EXPECT_EQ(OnDoubleTap(Held(), {.object = k_Creature}), Tap::Nothing);
	EXPECT_EQ(OnDoubleTap(Held(), {}), Tap::Nothing);
	EXPECT_EQ(OnDoubleTap(Held(), {.object = k_Tree, .leashTarget = false}), Tap::Nothing);
	// A miracle bubble taken only once is taken as usual
	EXPECT_EQ(OnDoubleTap(Held(), {.object = k_Tree, .miracleBubble = true}), Tap::Normal);
	// Tied: the tied thing or the creature unties it; anything else does nothing
	EXPECT_EQ(OnDoubleTap(TiedTo(k_Tree), {.object = k_Tree}), Tap::Untie);
	EXPECT_EQ(OnDoubleTap(TiedTo(k_Tree), {.object = k_Creature}), Tap::Untie);
	EXPECT_EQ(OnDoubleTap(TiedTo(k_Tree), {.object = k_Cow}), Tap::Nothing);
	EXPECT_EQ(OnDoubleTap(TiedTo(k_Tree), {}), Tap::Nothing);
	EXPECT_EQ(OnDoubleTap({}, {.object = k_Tree}), Tap::Normal);
}

TEST(LeashOrders, DoubleTapsAreQuickAndStill)
{
	DoubleTaps taps;
	EXPECT_FALSE(taps.OnPress(1000, {10.0f, 10.0f}));
	EXPECT_TRUE(taps.OnPress(1500, {12.0f, 8.0f}));
	// A third press starts again
	EXPECT_FALSE(taps.OnPress(1600, {12.0f, 8.0f}));
	// Too slow
	EXPECT_FALSE(taps.OnPress(2101, {12.0f, 8.0f}));
	// Moved too far
	EXPECT_FALSE(taps.OnPress(2200, {15.0f, 8.0f}));
}

TEST(LeashOrders, TheLeashToolTips)
{
	EXPECT_EQ(ToolTipFor(Held(), {}), k_FocusCreatureTip);
	EXPECT_EQ(ToolTipFor(Held(), {.object = k_Tree, .leashTarget = true, .developmentPhase = 4}), k_AttachLeashTip);
	// Too young to be tied, over itself, a post, or a miracle bubble: no tip
	EXPECT_EQ(ToolTipFor(Held(), {.object = k_Tree, .leashTarget = true, .developmentPhase = 3}), std::nullopt);
	EXPECT_EQ(ToolTipFor(Held(), {.object = k_Creature, .leashTarget = true, .developmentPhase = 4}), std::nullopt);
	EXPECT_EQ(ToolTipFor(Held(), {.object = k_Tree, .leashTarget = false, .developmentPhase = 4}), std::nullopt);
	EXPECT_EQ(ToolTipFor(Held(), {.object = k_Tree, .leashTarget = true, .miracleBubble = true, .developmentPhase = 4}),
	          std::nullopt);
	EXPECT_EQ(ToolTipFor(TiedTo(k_Tree), {.object = k_Tree}), k_DetachLeashTip);
	EXPECT_EQ(ToolTipFor(TiedTo(k_Tree), {}), std::nullopt);
	EXPECT_EQ(ToolTipFor({}, {}), std::nullopt);
}

TEST(LeashOrders, SomeStatesRefuseOrders)
{
	EXPECT_TRUE(TakesOrders({}));
	EXPECT_FALSE(TakesOrders({.leashWorks = false}));
	EXPECT_FALSE(TakesOrders({.life = 0.0f}));
	EXPECT_FALSE(TakesOrders({.exhaustion = 1.0f}));
	EXPECT_TRUE(TakesOrders({.exhaustion = 0.99f}));
}

TEST(LeashOrders, TheLandSendsItThere)
{
	const glm::vec2 place {100.0f, 50.0f};
	const auto walk = OnGround(place, {.reachable = place, .height = 5.0f});
	EXPECT_EQ(walk.kind, GroundOrder::Kind::MoveTo);
	EXPECT_NEAR(walk.arrival, 5.0f, k_Tolerance);
	EXPECT_FALSE(walk.runAndWait);
	EXPECT_TRUE(walk.acknowledged);
	EXPECT_TRUE(walk.marked);
	// No nearer than 8 metres for a big creature
	EXPECT_NEAR(OnGround(place, {.reachable = place, .height = 20.0f}).arrival, 8.0f, k_Tolerance);
	// Told again before it's done, it runs and waits
	EXPECT_TRUE(OnGround(place, {.reachable = place, .orderInForce = true, .height = 5.0f}).runAndWait);
}

TEST(LeashOrders, AnUnreachablePlace)
{
	const glm::vec2 place {100.0f, 50.0f};
	const glm::vec2 nearest {98.0f, 50.0f};
	const auto nearer = OnGround(place, {.reachable = nearest, .placeReachable = false, .height = 5.0f});
	EXPECT_EQ(nearer.kind, GroundOrder::Kind::MoveTo);
	EXPECT_EQ(nearer.point, nearest);
	const auto none = OnGround(place, {.reachable = std::nullopt, .placeReachable = false, .height = 5.0f});
	EXPECT_EQ(none.kind, GroundOrder::Kind::Inaccessible);
	EXPECT_FALSE(none.acknowledged);
	EXPECT_FALSE(none.marked);
}

TEST(LeashOrders, TheSeaAFieldAndHome)
{
	const glm::vec2 place {0.0f};
	EXPECT_EQ(OnGround(place, {.reachable = place, .water = true, .thirsty = true}).kind, GroundOrder::Kind::Drink);
	EXPECT_EQ(OnGround(place, {.reachable = place, .water = true}).kind, GroundOrder::Kind::LookAtReflection);
	EXPECT_EQ(OnGround(place, {.reachable = place, .field = true}).kind, GroundOrder::Kind::ActOnField);
	const Ground home {
	    .reachable = place, .playerHasCitadel = true, .exhaustion = 0.2f, .distanceFromHome = 11.0f, .height = 5.0f};
	EXPECT_EQ(OnGround(place, home).kind, GroundOrder::Kind::SleepAtHome);
	auto far = home;
	far.distanceFromHome = 12.0f;
	EXPECT_EQ(OnGround(place, far).kind, GroundOrder::Kind::MoveTo);
	auto rested = home;
	rested.exhaustion = 0.1f;
	EXPECT_EQ(OnGround(place, rested).kind, GroundOrder::Kind::MoveTo);
	auto noCitadel = home;
	noCitadel.playerHasCitadel = false;
	EXPECT_EQ(OnGround(place, noCitadel).kind, GroundOrder::Kind::MoveTo);
	auto asleep = home;
	asleep.sleeping = true;
	EXPECT_EQ(OnGround(place, asleep).kind, GroundOrder::Kind::MoveTo);
}

TEST(LeashOrders, CarryingItPutsDownOrThrows)
{
	const glm::vec2 place {0.0f};
	const auto put = OnGround(place, {.carrying = true, .height = 5.0f});
	EXPECT_EQ(put.kind, GroundOrder::Kind::PutDownAt);
	EXPECT_NEAR(put.arrival, 5.0f, k_Tolerance);
	const auto angry = OnGround(place, {.carrying = true, .leash = LeashType::Evil, .height = 5.0f});
	EXPECT_EQ(angry.kind, GroundOrder::Kind::ThrowAt);
	EXPECT_NEAR(angry.arrival, 40.0f, k_Tolerance);
	EXPECT_EQ(OnGround(place, {.carrying = true, .placeReachable = false, .height = 5.0f}).kind, GroundOrder::Kind::ThrowAt);
}

TEST(LeashOrders, WhatAThingIsTriedWith)
{
	using enum Attempt;
	EXPECT_EQ(OnThing({.forest = true}), std::vector {GoToForest});
	EXPECT_EQ(OnThing({.fishFarm = true, .hungry = true, .knowsFishing = true, .energy = 0.5f}), std::vector {FishAndEat});
	EXPECT_FALSE(Acknowledges(OnThing({.fishFarm = true, .hungry = true, .knowsFishing = true, .energy = 0.5f})));
	// Full, or not knowing how, a fish farm is like anything else
	EXPECT_EQ(OnThing({.fishFarm = true, .hungry = true, .knowsFishing = true, .energy = 1.0f}), (std::vector {Desires, Look}));
	EXPECT_EQ(OnThing({.liftable = true}), std::vector {Hold});
	EXPECT_EQ(OnThing({}), (std::vector {Desires, Look}));
	EXPECT_EQ(OnThing({.carrying = true}), (std::vector {DesiresUsingCarried, Desires, Look}));
	EXPECT_EQ(OnThing({.carrying = true, .liftable = true}), (std::vector {DesiresUsingCarried, Desires, Hold}));
	EXPECT_TRUE(Acknowledges(OnThing({})));
}

TEST(LeashOrders, TheAggressionLeashFightsCreaturesNearEnough)
{
	using enum Attempt;
	const Thing near {.creature = true, .distance = 39.0f, .leash = LeashType::Evil, .height = 5.0f};
	EXPECT_EQ(OnThing(near), (std::vector {Fight, Look}));
	auto far = near;
	far.distance = 40.0f;
	EXPECT_EQ(OnThing(far), (std::vector {Desires, Look}));
	auto kind = near;
	kind.leash = LeashType::Good;
	EXPECT_EQ(OnThing(kind), (std::vector {Desires, Look}));
	auto carrying = near;
	carrying.carrying = true;
	EXPECT_EQ(OnThing(carrying), (std::vector {DesiresUsingCarried, Fight, Look}));
}

TEST(LeashOrders, Agendas)
{
	const auto walk = MoveTo({10.0f, 20.0f}, 5.0f, false, 0.0f);
	ASSERT_EQ(walk.size(), 1u);
	EXPECT_EQ(walk[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_FALSE(walk[0].movement.run);
	EXPECT_NEAR(walk[0].movement.maxDistance, 5.0f, k_Tolerance);
	const auto run = MoveTo({10.0f, 20.0f}, 5.0f, true, 3.0f);
	ASSERT_EQ(run.size(), 2u);
	EXPECT_TRUE(run[0].movement.run);
	EXPECT_EQ(run[1].kind, Step::Kind::Wait);
	EXPECT_NEAR(run[1].seconds, 3.0f, k_Tolerance);

	// Two to five seconds, in whole turns
	EXPECT_NEAR(RunWaitSeconds(0.0f, 10.0f), 2.0f, k_Tolerance);
	EXPECT_NEAR(RunWaitSeconds(2.97f, 10.0f), 4.9f, k_Tolerance);
	EXPECT_NEAR(HoldSeconds(5.0f, 10.0f), 45.0f, k_Tolerance);

	const auto put = PutDownAt({1.0f, 2.0f}, 5.0f);
	ASSERT_EQ(put.size(), 2u);
	EXPECT_EQ(put[1].order.kind, ObjectOrder::Kind::Discard);
	const auto thrown = ThrowAt({1.0f, 2.0f}, 5.0f);
	ASSERT_EQ(thrown.size(), 2u);
	EXPECT_NEAR(thrown[0].movement.maxDistance, 40.0f, k_Tolerance);
	EXPECT_EQ(thrown[1].order.kind, ObjectOrder::Kind::Throw);

	const auto hold = Hold(k_Cow, false, {0.0f, 0.0f}, 2u, 40.0f);
	ASSERT_EQ(hold.size(), 4u);
	EXPECT_EQ(hold[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ(hold[1].movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(hold[2].order.kind, ObjectOrder::Kind::Keep);
	EXPECT_EQ(hold[2].order.animation, 102u);
	EXPECT_EQ(hold[3].kind, Step::Kind::Wait);
	EXPECT_EQ(Hold(k_Cow, true, {0.0f, 0.0f}, std::nullopt, 40.0f).size(), 2u);

	// Home to sleep: the yawn first, then the walk, then the sleep
	const std::vector<Step> sleep {{.kind = Step::Kind::Action, .animation = 57},
	                               {.kind = Step::Kind::Static},
	                               {.kind = Step::Kind::Action, .animation = 63}};
	const auto home = SleepAtHome({5.0f, 5.0f}, 20.0f, sleep);
	ASSERT_EQ(home.size(), 4u);
	EXPECT_EQ(home[0].animation, 57u);
	EXPECT_EQ(home[1].kind, Step::Kind::Move);
	EXPECT_NEAR(home[1].movement.maxDistance, 5.0f, k_Tolerance);
	EXPECT_EQ(home[2].kind, Step::Kind::Static);
}

TEST(LeashOrders, TheMarker)
{
	EXPECT_NEAR(MarkerOverLand({1.0f, 10.0f, 2.0f}).y, 12.0f, k_Tolerance);
	EXPECT_NEAR(MarkerOverThing({1.0f, 10.0f, 2.0f}, 6.0f).y, 18.0f, k_Tolerance);
	// Widest at the turn of the clock, narrowest a half second on, the height a fifth of a second behind
	const auto start = MarkerSize(0);
	EXPECT_NEAR(start.x, 1.2f, k_Tolerance);
	EXPECT_NEAR(start.y, std::abs(std::cos(0.2f * 3.14159265f)) + 0.2f, 1e-3f);
	EXPECT_NEAR(MarkerSize(500).x, 0.2f, k_Tolerance);
	EXPECT_NEAR(MarkerSize(1000).x, 1.2f, k_Tolerance);
	EXPECT_NEAR(MarkerSize(300).y, 0.2f, k_Tolerance);
	// The ape's hand, the cow's hoof, the horse's shoe, the chicken's smiley, and the first for anything past them
	EXPECT_EQ(FootprintCell(0), 0);
	EXPECT_EQ(FootprintCell(1), 2);
	EXPECT_EQ(FootprintCell(6), 3);
	EXPECT_EQ(FootprintCell(17), 7);
	EXPECT_EQ(FootprintCell(18), 0);
}
