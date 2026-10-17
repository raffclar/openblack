/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using creature_desires::Desire;
using creature_mind::Step;
namespace animations = creature_layers::animations;

namespace
{
constexpr uint32_t k_Asker = 7;
constexpr uint32_t k_Friend = 9;

creature_mind::Friends TwoFriends()
{
	return {.self = k_Asker,
	        .friendly = k_Friend,
	        .selfPoint = {100.0f, 200.0f},
	        .friendPoint = {140.0f, 260.0f},
	        .height = 30.0f,
	        .friendHeight = 20.0f};
}

/// A random source that always gives the same number
creature_mind::Random Always(uint32_t value)
{
	return [value](uint32_t) { return value; };
}

const Step& Ordered(const Step& step)
{
	EXPECT_EQ(step.kind, Step::Kind::Order);
	EXPECT_EQ(step.object, k_Friend);
	EXPECT_NE(step.ordered, nullptr);
	return *step.ordered;
}

/// The first steps of each joint action side by side: ask, send the friend to its place, go to its own, wait
void ExpectMeeting(const std::vector<Step>& agenda, size_t first, glm::vec2 selfPlace, glm::vec2 friendPlace)
{
	const auto& friends = TwoFriends();
	EXPECT_EQ(agenda[first].kind, Step::Kind::RequestPartner);
	EXPECT_EQ(agenda[first].request, creature_friendship::RequestKind::BeFriends);
	EXPECT_EQ(agenda[first].object, k_Friend);
	const auto& go = Ordered(agenda[first + 1]);
	EXPECT_EQ(go.movement.kind, creature_mind::Movement::Kind::ToPoint);
	EXPECT_FLOAT_EQ(go.movement.point.x, friendPlace.x);
	EXPECT_FLOAT_EQ(go.movement.point.y, friendPlace.y);
	EXPECT_FLOAT_EQ(go.movement.maxDistance, friends.friendHeight);
	EXPECT_FALSE(go.gaze.has_value());
	EXPECT_FLOAT_EQ(agenda[first + 2].movement.point.x, selfPlace.x);
	EXPECT_FLOAT_EQ(agenda[first + 2].movement.point.y, selfPlace.y);
	EXPECT_FLOAT_EQ(agenda[first + 2].movement.maxDistance, friends.height);
	EXPECT_EQ(agenda[first + 3].kind, Step::Kind::WaitForStill);
	EXPECT_EQ(agenda[first + 3].object, k_Friend);
}
} // namespace

TEST(CreatureJointActions, PlacesOfTwoFriends)
{
	const auto [self, other] = creature_mind::SideBySide(TwoFriends());
	// Halfway is (120, 230): the creature its own height along the first axis, the friend its own height back
	EXPECT_FLOAT_EQ(self.x, 150.0f);
	EXPECT_FLOAT_EQ(self.y, 230.0f);
	EXPECT_FLOAT_EQ(other.x, 100.0f);
	EXPECT_FLOAT_EQ(other.y, 230.0f);
	const auto [first, second] = creature_mind::AboutPlace({10.0f, 20.0f}, 40.0f);
	EXPECT_FLOAT_EQ(first.x, 20.0f);
	EXPECT_FLOAT_EQ(first.y, 30.0f);
	EXPECT_FLOAT_EQ(second.x, 0.0f);
	EXPECT_FLOAT_EQ(second.y, 10.0f);
	EXPECT_FLOAT_EQ(creature_mind::SitTogetherRadius(30.0f, 20.0f), 85.0f);
	EXPECT_FLOAT_EQ(creature_mind::SleepTogetherRadius(30.0f, 20.0f), 100.0f);
}

TEST(CreatureJointActions, BeingHappyWithAFriend)
{
	const auto agenda = creature_mind::BeHappyWithFriend(TwoFriends());
	ASSERT_EQ(agenda.size(), 8U);
	ExpectMeeting(agenda, 0, {150.0f, 230.0f}, {100.0f, 230.0f});
	EXPECT_EQ(Ordered(agenda[4]).movement.object, k_Asker);
	EXPECT_TRUE(agenda[5].flutterEyelids);
	EXPECT_EQ(Ordered(agenda[6]).animation, animations::k_Happy);
	EXPECT_EQ(agenda[7].animation, animations::k_Happy);
	// No step is marked as the main step, so the last one is
	EXPECT_EQ(creature_mind::MainStep(agenda), 7U);
}

TEST(CreatureJointActions, HavingAPooWithAFriend)
{
	const auto agenda = creature_mind::PooWithFriend(TwoFriends(), Always(0));
	ASSERT_EQ(agenda.size(), 10U);
	ExpectMeeting(agenda, 0, {150.0f, 230.0f}, {100.0f, 230.0f});
	EXPECT_EQ(Ordered(agenda[4]).sequence[1], animations::k_Poo);
	EXPECT_FLOAT_EQ(agenda[5].seconds, creature_mind::k_PooTogetherSeconds);
	EXPECT_TRUE(agenda[5].main);
	EXPECT_EQ(agenda[5].effect, creature_mind::Effect::None);
	EXPECT_EQ(Ordered(agenda[6]).effect, creature_mind::Effect::Poo);
	EXPECT_EQ(agenda[7].kind, Step::Kind::Apply);
	EXPECT_EQ(agenda[7].effect, creature_mind::Effect::Poo);
	EXPECT_FLOAT_EQ(agenda[8].seconds, creature_mind::k_FaceAfterPooSeconds);
	EXPECT_EQ(agenda[9].animation, animations::k_Happy);
}

TEST(CreatureJointActions, SittingWithAFriend)
{
	const auto agenda = creature_mind::SitWithFriend(TwoFriends(), {10.0f, 20.0f}, 40.0f, Always(0));
	ASSERT_EQ(agenda.size(), 10U);
	ExpectMeeting(agenda, 0, {20.0f, 30.0f}, {0.0f, 10.0f});
	EXPECT_TRUE(agenda[3].flutterEyelids);
	EXPECT_FALSE(agenda[5].flutterEyelids);
	EXPECT_FLOAT_EQ(Ordered(agenda[6]).seconds, creature_mind::k_SitTogetherSeconds);
	EXPECT_EQ(agenda[7].sequence[1], animations::k_Sit);
	EXPECT_TRUE(agenda[7].main);
	EXPECT_EQ(agenda[9].animation, animations::k_Happy);
}

TEST(CreatureJointActions, SleepingWithAFriendYawnsHalfTheTime)
{
	const auto yawn = creature_mind::SleepWithFriend(TwoFriends(), {10.0f, 20.0f}, 40.0f, Always(0));
	const auto noYawn = creature_mind::SleepWithFriend(TwoFriends(), {10.0f, 20.0f}, 40.0f, Always(1));
	ASSERT_EQ(yawn.size(), 10U);
	ASSERT_EQ(noYawn.size(), 9U);
	EXPECT_EQ(yawn[0].animation, animations::k_Tired);
	EXPECT_TRUE(yawn[0].sleepyEyes);
	ExpectMeeting(noYawn, 0, {20.0f, 30.0f}, {0.0f, 10.0f});
	EXPECT_EQ(Ordered(noYawn[6]).sequence[1], animations::k_Sleep);
	EXPECT_FLOAT_EQ(noYawn[7].seconds, creature_mind::k_SleepTogetherSeconds);
	EXPECT_TRUE(noYawn[7].main);
	// This sleep is a plain hold of the sleep animation: it does not rest the creature
	EXPECT_FALSE(noYawn[7].untilRested);
	EXPECT_EQ(noYawn[7].effect, creature_mind::Effect::None);
	EXPECT_EQ(noYawn[8].animation, animations::k_Confused);
}

TEST(CreatureJointActions, DrinkingWithAFriend)
{
	const auto agenda = creature_mind::DrinkWithFriend(TwoFriends(), {50.0f, 60.0f}, {0.0f, 60.0f}, Always(0));
	ASSERT_EQ(agenda.size(), 11U);
	// The friend stands its own height along the first axis from the coast
	ExpectMeeting(agenda, 0, {50.0f, 60.0f}, {70.0f, 60.0f});
	EXPECT_EQ(Ordered(agenda[4]).movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[7].animation, animations::k_Drink);
	EXPECT_TRUE(agenda[7].main);
	EXPECT_EQ(Ordered(agenda[8]).effect, creature_mind::Effect::Drink);
	EXPECT_EQ(agenda[9].kind, Step::Kind::Apply);
	EXPECT_TRUE(agenda[10].flutterEyelids);
	EXPECT_EQ(agenda[10].face, creature_face::Cue::Smile);
}

TEST(CreatureJointActions, HelpingOrAttackingATownWithAFriend)
{
	const creature_mind::FriendsTown town {.town = 3, .pit = 4, .townPoint = {500.0f, 600.0f}, .pitRadius = 10.0f};
	const auto agenda = creature_mind::TownWithFriend(TwoFriends(), town, Desire::Anger);
	ASSERT_EQ(agenda.size(), 10U);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::BeFriends);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Playfulness);
	EXPECT_EQ(agenda[3].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_TRUE(agenda[3].main);
	// Both go near the pit, to twice its radius and the asker's height
	EXPECT_FLOAT_EQ(Ordered(agenda[4]).movement.maxDistance, 80.0f);
	EXPECT_EQ(agenda[5].movement.object, 4U);
	const auto& told = Ordered(agenda[8]);
	EXPECT_EQ(told.kind, Step::Kind::ApplyDesire);
	EXPECT_EQ(told.desire, Desire::Anger);
	EXPECT_EQ(agenda[9].object, 3U);
}

TEST(CreatureJointActions, WhatLetsEachJointActionStart)
{
	using creature_plan_actions::Situation;
	const auto possible = [](std::string_view action, const Situation& situation) {
		const auto* executor = creature_plan_actions::For(action);
		EXPECT_NE(executor, nullptr);
		return executor != nullptr && creature_plan_actions::Possible(*executor, situation);
	};
	Situation situation;
	EXPECT_FALSE(possible("DrinkWithFriend", situation));
	EXPECT_FALSE(possible("PooWithFriend", situation));
	EXPECT_FALSE(possible("SleepWithFriend", situation));
	EXPECT_TRUE(possible("BeHappyWithFriend", situation));
	EXPECT_TRUE(possible("SitWithFriend", situation));
	situation.water = 0.11f;
	situation.pooDesire = 0.11f;
	situation.tiredness = 0.11f;
	situation.sadness = 0.1f;
	EXPECT_TRUE(possible("DrinkWithFriend", situation));
	EXPECT_TRUE(possible("PooWithFriend", situation));
	EXPECT_TRUE(possible("SleepWithFriend", situation));
	EXPECT_FALSE(possible("BeHappyWithFriend", situation));
	// Without a belief about the nearest town, neither town action starts
	EXPECT_FALSE(possible("HelpTownWithFriend", situation));
	EXPECT_FALSE(possible("AttackWithFriend", situation));
	situation.attackNearestTown = false;
	EXPECT_TRUE(possible("HelpTownWithFriend", situation));
	EXPECT_FALSE(possible("AttackWithFriend", situation));
	situation.attackNearestTown = true;
	EXPECT_FALSE(possible("HelpTownWithFriend", situation));
	EXPECT_TRUE(possible("AttackWithFriend", situation));
}
