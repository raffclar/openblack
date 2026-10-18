/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <numbers>

#include <gtest/gtest.h>

#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureMeals.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureStomp.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace plan_actions = openblack::creature_plan_actions;
namespace stomp = openblack::creature_stomp;

namespace
{
uint32_t StompFirst(uint32_t /*range*/)
{
	return 0;
}
uint32_t StompLast(uint32_t range)
{
	return range - 1;
}
} // namespace

// One time in five the creature is angry first. Then it walks up to the thing and stomps on it with an angry face.
TEST(CreatureStomp, AStompIsSometimesAngryFirst)
{
	const auto* action = plan_actions::For("Stomp");
	ASSERT_NE(action, nullptr);
	EXPECT_EQ(action->target, plan_actions::Target::Stompable);
	const auto angry = plan_actions::Agenda(*action, 7, {1.0f, 2.0f}, {.height = 15.0f}, StompFirst);
	ASSERT_TRUE(angry.has_value());
	ASSERT_EQ(angry->size(), 2);
	EXPECT_EQ((*angry)[0].kind, Step::Kind::Action);
	EXPECT_EQ((*angry)[0].animation, 53u);
	EXPECT_EQ((*angry)[0].face, creature_face::Cue::Anger);
	EXPECT_EQ((*angry)[1].order.kind, ObjectOrder::Kind::Destroy);
	EXPECT_EQ((*angry)[1].order.object, 7u);
	EXPECT_EQ((*angry)[1].effect, Effect::Stomped);
	EXPECT_EQ((*angry)[1].face, creature_face::Cue::Anger);
	const auto calm = plan_actions::Agenda(*action, 7, {1.0f, 2.0f}, {.height = 15.0f}, StompLast);
	ASSERT_TRUE(calm.has_value());
	ASSERT_EQ(calm->size(), 1);
	EXPECT_EQ((*calm)[0].order.kind, ObjectOrder::Kind::Destroy);
}

// Stomping a stone in half is the stomp alone
TEST(CreatureStomp, SmashingAStoneIsAStomp)
{
	const auto* action = plan_actions::For("SmashStoneInHalf");
	ASSERT_NE(action, nullptr);
	const auto agenda = plan_actions::Agenda(*action, 4, {0.0f, 0.0f}, {.height = 15.0f}, StompFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 1);
	EXPECT_EQ((*agenda)[0].order.kind, ObjectOrder::Kind::Destroy);
}

// With nothing to eat in its hand, the creature stomps on the thing, picks it up and eats it. With food in its hand it
// only eats.
TEST(CreatureStomp, StompAndEatStompsPicksUpAndEats)
{
	const auto* action = plan_actions::For("StompAndEat");
	ASSERT_NE(action, nullptr);
	EXPECT_EQ(action->target, plan_actions::Target::Stompable);
	EXPECT_EQ(action->activity, Activity::Eat);
	const auto agenda = plan_actions::Agenda(*action, 12, {0.0f, 0.0f}, {.height = 15.0f}, StompFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 3);
	EXPECT_EQ((*agenda)[0].order.kind, ObjectOrder::Kind::Destroy);
	EXPECT_EQ((*agenda)[0].effect, Effect::Stomped);
	EXPECT_EQ((*agenda)[1].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ((*agenda)[1].order.object, 12u);
	EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::Eat);
	EXPECT_EQ((*agenda)[2].effect, Effect::Eat);
	const auto holding = plan_actions::Agenda(*action, 12, {0.0f, 0.0f}, {.holdingEatable = true}, StompFirst);
	ASSERT_TRUE(holding.has_value());
	ASSERT_EQ(holding->size(), 1);
	EXPECT_EQ((*holding)[0].order.kind, ObjectOrder::Kind::Eat);
}

// The creature picks up a rock, walks to where it throws from, throws the rock at the thing, then picks the thing up
// and eats it
TEST(CreatureStomp, StoneAndEatThrowsARockThenEats)
{
	const auto* action = plan_actions::For("StoneAndEat");
	ASSERT_NE(action, nullptr);
	EXPECT_EQ(action->target, plan_actions::Target::StonableAndPickable);
	EXPECT_EQ(action->use, plan_actions::Target::ThrowingDamage);
	plan_actions::Situation situation {.height = 15.0f};
	situation.instrument = 30;
	const auto agenda = plan_actions::Agenda(*action, 8, {3.0f, 4.0f}, situation, StompFirst);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 5);
	EXPECT_EQ((*agenda)[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ((*agenda)[0].order.object, 30u);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_FLOAT_EQ((*agenda)[1].movement.maxDistance, 15.0f);
	EXPECT_EQ((*agenda)[1].face, creature_face::Cue::Anger);
	EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::Throw);
	// The game aims the throw at the rock in the hand, not at the thing
	EXPECT_EQ((*agenda)[2].order.object, 30u);
	EXPECT_TRUE((*agenda)[2].order.atObject);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ((*agenda)[3].order.object, 8u);
	EXPECT_EQ((*agenda)[4].order.kind, ObjectOrder::Kind::Eat);
	// Holding a rock already, it does not pick one up
	situation.holdingRock = true;
	const auto holding = plan_actions::Agenda(*action, 8, {3.0f, 4.0f}, situation, StompFirst);
	ASSERT_TRUE(holding.has_value());
	EXPECT_EQ(holding->size(), 4);
	// With nothing to throw it can not
	EXPECT_FALSE(plan_actions::Agenda(*action, 8, {3.0f, 4.0f}, {.height = 15.0f}, StompFirst).has_value());
}

// A kick and a kick at a tree: go near, face the thing a tenth of a second, kick it
TEST(CreatureStomp, AKickGoesNearFacesAndKicks)
{
	for (const auto* name : {"Kick", "KickTree"})
	{
		const auto* action = plan_actions::For(name);
		ASSERT_NE(action, nullptr) << name;
		const auto agenda = plan_actions::Agenda(*action, 5, {0.0f, 0.0f}, {.height = 15.0f}, StompFirst);
		ASSERT_TRUE(agenda.has_value()) << name;
		ASSERT_EQ(agenda->size(), 3) << name;
		EXPECT_EQ((*agenda)[0].movement.kind, Movement::Kind::GoNearObject);
		EXPECT_FLOAT_EQ((*agenda)[0].movement.maxDistance, 0.0f);
		EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::TurnToFaceObject);
		EXPECT_FLOAT_EQ((*agenda)[1].seconds, 0.1f);
		EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::Kick);
		EXPECT_EQ((*agenda)[2].order.object, 5u);
	}
	EXPECT_EQ(plan_actions::For("Kick")->target, plan_actions::Target::Kickable);
	EXPECT_EQ(plan_actions::For("KickTree")->target, plan_actions::Target::Tree);
}

// What a creature of size 1 (15 metres tall) can stomp on
TEST(CreatureStomp, TheStompRules)
{
	using stomp::StompRule;
	EXPECT_FALSE(stomp::CanBeStompedOn({.rule = StompRule::Never}, 1.0f));
	EXPECT_TRUE(stomp::CanBeStompedOn({.rule = StompRule::Always, .height = 100.0f}, 1.0f));
	EXPECT_TRUE(stomp::CanBeStompedOn({.rule = StompRule::WhileAlive}, 1.0f));
	EXPECT_FALSE(stomp::CanBeStompedOn({.rule = StompRule::WhileAlive, .dead = true}, 1.0f));
	// A bird lower than 3.75 metres over the land
	EXPECT_TRUE(stomp::CanBeStompedOn({.rule = StompRule::LowBird, .heightAboveLand = 3.7f}, 1.0f));
	EXPECT_FALSE(stomp::CanBeStompedOn({.rule = StompRule::LowBird, .heightAboveLand = 3.75f}, 1.0f));
	// A tree on the map shorter than 3 metres
	EXPECT_TRUE(stomp::CanBeStompedOn({.rule = StompRule::Short, .height = 2.9f}, 1.0f));
	EXPECT_FALSE(stomp::CanBeStompedOn({.rule = StompRule::Short, .height = 3.0f}, 1.0f));
	EXPECT_FALSE(stomp::CanBeStompedOn({.rule = StompRule::Short, .onMap = false, .height = 1.0f}, 1.0f));
	// A building lower than 3.75 metres
	EXPECT_TRUE(stomp::CanBeStompedOn({.rule = StompRule::LowBuilding, .height = 3.7f}, 1.0f));
	EXPECT_FALSE(stomp::CanBeStompedOn({.rule = StompRule::LowBuilding, .height = 3.8f}, 1.0f));
	// A bigger creature reaches taller things
	EXPECT_TRUE(stomp::CanBeStompedOn({.rule = StompRule::Short, .height = 5.9f}, 2.0f));
}

TEST(CreatureStomp, TheKickRules)
{
	using stomp::KickRule;
	EXPECT_FALSE(stomp::CanBeKicked({.rule = KickRule::Never}, 100.0f));
	EXPECT_TRUE(stomp::CanBeKicked({.rule = KickRule::Building}, 100.0f));
	EXPECT_FALSE(stomp::CanBeKicked({.rule = KickRule::Building, .built = false}, 100.0f));
	EXPECT_FALSE(stomp::CanBeKicked({.rule = KickRule::Building, .onFire = true}, 100.0f));
	// Another creature can always be kicked, unless the leash ties the kicker to it
	EXPECT_TRUE(stomp::CanBeKicked({.rule = KickRule::Thing, .creature = true, .weight = 1000.0f}, 1.0f));
	EXPECT_FALSE(stomp::CanBeKicked({.rule = KickRule::Thing, .creature = true, .leashTied = true}, 1.0f));
	// A thing must fly, move, be lighter than twice the kicker, not be indestructible, and not be in the kicker's hand
	const stomp::KickTarget thing {.rule = KickRule::Thing, .canFly = true, .weight = 199.0f};
	EXPECT_TRUE(stomp::CanBeKicked(thing, 100.0f));
	auto heavy = thing;
	heavy.weight = 200.0f;
	EXPECT_FALSE(stomp::CanBeKicked(heavy, 100.0f));
	auto still = thing;
	still.immovable = true;
	EXPECT_FALSE(stomp::CanBeKicked(still, 100.0f));
	auto held = thing;
	held.heldByKicker = true;
	EXPECT_FALSE(stomp::CanBeKicked(held, 100.0f));
	auto kept = thing;
	kept.indestructible = true;
	EXPECT_FALSE(stomp::CanBeKicked(kept, 100.0f));
	auto grounded = thing;
	grounded.canFly = false;
	EXPECT_FALSE(stomp::CanBeKicked(grounded, 100.0f));
}

TEST(CreatureStomp, AKickHitsAnotherCreatureByTheirHeights)
{
	EXPECT_FLOAT_EQ(stomp::KickHitStrength(10.0f, 20.0f), 0.15f);
	EXPECT_FLOAT_EQ(stomp::KickHitStrength(30.0f, 20.0f), 0.2f);
}

// Two blows at the middle between the creature and its foot, each at a random height up to the reach of the blow
TEST(CreatureStomp, AKickBreaksABuildingWithTwoBlows)
{
	const stomp::BuildingKick kick {
	    .creature = {0.0f, 10.0f, 0.0f},
	    .foot = {0.0f, 14.0f, -8.0f},
	    .direction = {0.0f, 0.6f, -0.8f},
	    .heading = openblack::GameHeading(0.0f),
	    .creatureSize = 1.0f,
	    .buildingRadius = 10.0f,
	    .sideDraw = 10,
	    .firstHeightDraw = 50,
	    .secondHeightDraw = 0,
	};
	const auto blows = stomp::KickBlows(kick, 10.0f);
	// The middle is 2 metres over the land; the reach is the smaller of 0.6 x 10 and 3.75 x 1
	EXPECT_FLOAT_EQ(blows[0].point.x, 0.0f);
	EXPECT_FLOAT_EQ(blows[0].point.z, -4.0f);
	EXPECT_FLOAT_EQ(blows[0].point.y, 11.0f);
	EXPECT_FLOAT_EQ(blows[1].point.y, 10.0f);
	EXPECT_FLOAT_EQ(blows[0].reach, 3.75f);
	EXPECT_FLOAT_EQ(blows[0].velocity.y, 1.2f);
	EXPECT_FLOAT_EQ(blows[0].velocity.z, -1.6f);
	// The second goes a quarter turn to the side of the way the creature faces
	EXPECT_NEAR(blows[1].velocity.x, 2.0f * std::sin(std::numbers::pi_v<float> / 4.0f), 1e-5f);
	EXPECT_NEAR(blows[1].velocity.z, -2.0f * std::cos(std::numbers::pi_v<float> / 4.0f), 1e-5f);
}

TEST(CreatureStomp, TheSpheresOfTheBody)
{
	using stomp::Part;
	// At size 1 the head is 1.3 x 15 / 8, the hands and feet 1.25 x 15 / 8, the groin 15 / 6, the armpits 15 / 8
	EXPECT_FLOAT_EQ(stomp::SphereRadius(Part::Head, 1.0f), 1.3f * 15.0f * 0.125f);
	EXPECT_FLOAT_EQ(stomp::SphereRadius(Part::LeftFoot, 1.0f), 1.25f * 15.0f * 0.125f);
	EXPECT_FLOAT_EQ(stomp::SphereRadius(Part::Groin, 1.0f), 2.5f);
	EXPECT_FLOAT_EQ(stomp::SphereRadius(Part::RightArmpit, 1.0f), 1.875f);
	// The head and the limbs stop shrinking at size 2
	EXPECT_FLOAT_EQ(stomp::SphereRadius(Part::Head, 3.0f), 0.8f * 45.0f * 0.125f);
}

TEST(CreatureStomp, TheSpheresComeOnAndGoOffWithTheBody)
{
	using stomp::BodyStart;
	using stomp::Switch;
	using Phase = BodyStart::Phase;
	EXPECT_EQ(stomp::SwitchFor({.stands = true}), Switch::Off);
	EXPECT_EQ(stomp::SwitchFor({.animation = 28, .phase = Phase::Start}), Switch::On);
	EXPECT_EQ(stomp::SwitchFor({.animation = 29, .phase = Phase::Loop}), Switch::Off);
	EXPECT_EQ(stomp::SwitchFor({.animation = 30, .phase = Phase::End}), Switch::On);
	EXPECT_EQ(stomp::SwitchFor({.animation = 106, .phase = Phase::Start}), Switch::On);
	EXPECT_EQ(stomp::SwitchFor({.animation = 106, .phase = Phase::Loop}), Switch::Off);
	EXPECT_EQ(stomp::SwitchFor({.animation = 107, .phase = Phase::End}), Switch::Keep);
	EXPECT_EQ(stomp::SwitchFor({.animation = 122}), Switch::On);
	EXPECT_EQ(stomp::SwitchFor({.animation = 110}), Switch::On);
	EXPECT_EQ(stomp::SwitchFor({.animation = 123}), Switch::Off);
	EXPECT_EQ(stomp::SwitchFor({.animation = 53}), Switch::Keep);
}

TEST(CreatureStomp, WhatTheSpheresKnockAbout)
{
	using stomp::Victim;
	EXPECT_EQ(stomp::VictimOf({.hasModel = true, .villager = true, .living = true}), Victim::Living);
	EXPECT_EQ(stomp::VictimOf({.hasModel = true, .immovable = true, .villager = true, .living = true}), Victim::None);
	// Birds and creatures are passed by
	EXPECT_EQ(stomp::VictimOf({.hasModel = true, .living = true}), Victim::None);
	EXPECT_EQ(stomp::VictimOf({.hasModel = true, .passedBy = true, .canFly = true}), Victim::None);
	EXPECT_EQ(stomp::VictimOf({.hasModel = true, .building = true}), Victim::Building);
	EXPECT_EQ(stomp::VictimOf({.hasModel = true, .canFly = true}), Victim::Thing);
	EXPECT_EQ(stomp::VictimOf({.hasModel = true, .indestructible = true, .canFly = true}), Victim::None);
	EXPECT_EQ(stomp::VictimOf({.canFly = true}), Victim::None);
	// Only a part that moved further than three quarters of the size of the creature knocks
	EXPECT_FALSE(stomp::MovedEnough({0.0f, 0.0f, 0.0f}, {0.75f, 0.0f, 0.0f}, 1.0f));
	EXPECT_TRUE(stomp::MovedEnough({0.0f, 0.0f, 0.0f}, {0.76f, 0.0f, 0.0f}, 1.0f));
	// The living and things within 0.3 of the sphere and their radius; buildings within both radii. The distance comes
	// squared.
	EXPECT_TRUE(stomp::InReach(Victim::Living, 1.9f * 1.9f, 3.0f, 1.0f));
	EXPECT_FALSE(stomp::InReach(Victim::Living, 1.9f * 1.9f, 2.0f, 1.0f));
	EXPECT_TRUE(stomp::InReach(Victim::Building, 3.9f * 3.9f, 3.0f, 1.0f));
	EXPECT_EQ(stomp::KnockVelocity({1.0f, 0.0f, -2.0f}), glm::vec3(7.0f, 0.0f, -14.0f));
	const auto blow = stomp::TrampleBlow({1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, -2.0f}, 1.5f);
	// The way of the move at its length times 10 times 0.4
	EXPECT_EQ(blow.velocity, glm::vec3(0.0f, 0.0f, -8.0f));
	EXPECT_FLOAT_EQ(blow.reach, 1.5f);
}

TEST(CreatureStomp, AStompKillCounts)
{
	creature_meals::Tallies tallies;
	creature_meals::CountKill(tallies, creature_meals::k_VillagerSort);
	creature_meals::CountKill(tallies, creature_meals::k_AnimalSort);
	creature_meals::CountKill(tallies, 0);
	EXPECT_EQ(tallies.peopleKilled, 1u);
	EXPECT_EQ(tallies.animalsKilled, 1u);
	EXPECT_EQ(tallies.creaturesKilled, 0u);
	EXPECT_TRUE(tallies.keptDown.empty());
}

// The creature walks near the other creature, faces it, points one time in two, taunts, and is angry one time in two
TEST(CreatureStomp, ShowingAnotherCreatureHate)
{
	const auto* action = plan_actions::For("ShowCreatureYouHateHim");
	ASSERT_NE(action, nullptr);
	EXPECT_EQ(action->target, plan_actions::Target::Creature);
	const auto all = plan_actions::Agenda(*action, 3, {0.0f, 0.0f}, {.height = 15.0f, .thingHeight = 20.0f}, StompFirst);
	ASSERT_TRUE(all.has_value());
	ASSERT_EQ(all->size(), 5);
	EXPECT_EQ((*all)[0].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ((*all)[0].movement.maxDistance, 50.0f);
	EXPECT_FLOAT_EQ((*all)[1].seconds, 2.0f);
	EXPECT_EQ((*all)[2].order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ((*all)[3].animation, 70u);
	EXPECT_EQ((*all)[4].animation, 53u);
	const auto least = plan_actions::Agenda(*action, 3, {0.0f, 0.0f}, {.height = 15.0f, .thingHeight = 2.0f}, StompLast);
	ASSERT_TRUE(least.has_value());
	ASSERT_EQ(least->size(), 3);
	EXPECT_FLOAT_EQ((*least)[0].movement.maxDistance, 37.5f);
	EXPECT_EQ((*least)[2].animation, 222u);
}
