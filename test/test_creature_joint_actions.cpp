/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKiss.h"
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

namespace
{
/// A random source that gives the numbers in turn, and the last one again after them
creature_mind::Random InTurn(std::vector<uint32_t> values)
{
	auto next = std::make_shared<size_t>(0);
	return [values = std::move(values), next](uint32_t) {
		const auto value = values.at(std::min(*next, values.size() - 1));
		++*next;
		return value;
	};
}

std::vector<Step::Kind> KindsOf(const std::vector<Step>& agenda)
{
	std::vector<Step::Kind> kinds;
	for (const auto& step : agenda)
	{
		kinds.push_back(step.kind);
	}
	return kinds;
}

size_t MainStep(const std::vector<Step>& agenda)
{
	for (size_t i = 0; i < agenda.size(); ++i)
	{
		if (agenda[i].main)
		{
			return i;
		}
	}
	return agenda.size();
}
} // namespace

TEST(CreatureJointActions, KissingAFriend)
{
	const auto friends = TwoFriends();
	const auto agenda = creature_mind::KissFriend(friends, 2.0f, 5.0f, Always(1));
	ASSERT_EQ(agenda.size(), 7);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::BeFriends);
	// The friend goes to the far side of the middle, the creature to the near side, each two from the middle
	const auto& friendGoes = Ordered(agenda[1]);
	const auto [selfPlace, friendPlace] = creature_kiss::KissPlaces(friends.selfPoint, friends.friendPoint, 2.0f);
	EXPECT_FLOAT_EQ(friendGoes.movement.point.x, friendPlace.x);
	EXPECT_FLOAT_EQ(friendGoes.movement.maxDistance, creature_mind::k_KissPlaceWithin);
	EXPECT_FLOAT_EQ(agenda[2].movement.point.y, selfPlace.y);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Compassion);
	EXPECT_FLOAT_EQ(agenda[2].gaze->point->y, 5.0f);
	const auto& friendKisses = Ordered(agenda[5]);
	EXPECT_EQ(friendKisses.kind, Step::Kind::Kiss);
	EXPECT_EQ(friendKisses.object, k_Asker);
	EXPECT_EQ(agenda[6].kind, Step::Kind::Kiss);
	EXPECT_EQ(agenda[6].object, k_Friend);
	EXPECT_TRUE(agenda[6].main);
	// Half the time it smiles at the friend and is happy after
	const auto after = creature_mind::KissFriend(friends, 2.0f, 5.0f, Always(0));
	ASSERT_EQ(after.size(), 9);
	EXPECT_FLOAT_EQ(after[7].seconds, creature_mind::k_FaceAfterKissSeconds);
	EXPECT_EQ(after[8].animation, animations::k_Happy);
}

TEST(CreatureJointActions, TalkingAndTellingAJoke)
{
	const auto talk = creature_mind::TalkToFriend(k_Asker, k_Friend, 10.0f, Always(0));
	ASSERT_EQ(talk.size(), 6);
	EXPECT_FLOAT_EQ(talk[0].movement.maxDistance, 30.0f);
	EXPECT_TRUE(talk[0].flutterEyelids);
	EXPECT_TRUE(talk[1].main);
	EXPECT_FLOAT_EQ(talk[1].seconds, 4.0f);
	// The request comes only after it has turned to the friend
	EXPECT_EQ(talk[2].kind, Step::Kind::RequestPartner);
	EXPECT_FLOAT_EQ(Ordered(talk[3]).seconds, creature_mind::k_FriendFacesTalkerSeconds);
	EXPECT_EQ(talk[4].kind, Step::Kind::Overlay);
	EXPECT_EQ(talk[4].animation, animations::k_Talk);
	EXPECT_EQ(Ordered(talk[5]).kind, Step::Kind::Overlay);
	const auto joke = creature_mind::TellFriendAJoke(k_Asker, k_Friend, 10.0f, Always(0));
	EXPECT_EQ(Ordered(joke[5]).kind, Step::Kind::Action);
	EXPECT_EQ(Ordered(joke[5]).animation, creature_mind::k_Laugh);
}

TEST(CreatureJointActions, DancingWithAFriend)
{
	const auto agenda = creature_mind::DanceWithFriend(k_Asker, k_Friend, 10.0f);
	const std::vector expected {
	    Step::Kind::Move,  Step::Kind::Move,         Step::Kind::Action, Step::Kind::RequestPartner, Step::Kind::Demobilise,
	    Step::Kind::Order, Step::Kind::WaitForStill, Step::Kind::Order,  Step::Kind::PracticeDance,  Step::Kind::Remobilise};
	EXPECT_EQ(KindsOf(agenda), expected);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 20.0f);
	EXPECT_EQ(Ordered(agenda[7]).kind, Step::Kind::PracticeDance);
	EXPECT_FLOAT_EQ(agenda[8].seconds, creature_mind::k_DanceTogetherSeconds);
	EXPECT_EQ(MainStep(agenda), 8u);
}

TEST(CreatureJointActions, ArguingMopingAndShowingOff)
{
	const auto argue = creature_mind::ArgueWithFriend(k_Asker, k_Friend, 10.0f, Always(0));
	ASSERT_EQ(argue.size(), 16);
	EXPECT_FLOAT_EQ(Ordered(argue[1]).movement.maxDistance, 30.0f);
	EXPECT_EQ(Ordered(argue[1]).movement.object, k_Asker);
	EXPECT_EQ(argue[6].face, creature_face::Cue::Anger);
	EXPECT_EQ(argue[9].animation, animations::k_Angry);
	EXPECT_FLOAT_EQ(argue[11].seconds, creature_mind::k_ArgueWaitSeconds);
	EXPECT_EQ(MainStep(argue), 13u);
	EXPECT_EQ(argue[15].animation, animations::k_Happy);

	const auto mope = creature_mind::MopeAboutWithFriend(k_Asker, k_Friend, 10.0f, Always(1));
	ASSERT_EQ(mope.size(), 7);
	EXPECT_EQ(mope[2].face, creature_face::Cue::Sad);
	EXPECT_EQ(MainStep(mope), 6u);
	EXPECT_EQ(creature_mind::MopeAboutWithFriend(k_Asker, k_Friend, 10.0f, Always(0)).size(), 9);

	const auto show = creature_mind::ShowOffToFriend(k_Asker, k_Friend, 10.0f, {50.0f, 3.0f, 60.0f}, Always(0));
	ASSERT_EQ(show.size(), 11);
	EXPECT_TRUE(show[5].main);
	EXPECT_FLOAT_EQ(show[5].order.point.x, 50.0f);
	EXPECT_FLOAT_EQ(*show[5].order.pointHeight, 3.0f);
	EXPECT_EQ(show[5].face, creature_face::Cue::Grimace);
	EXPECT_EQ(Ordered(show[6]).movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(Ordered(show[8]).animation, animations::k_Confused);
}

TEST(CreatureJointActions, GettingTheAttentionOfAFriend)
{
	const auto all = creature_mind::GetAttentionFromFriend(k_Friend, Always(0));
	ASSERT_EQ(all.size(), 6);
	EXPECT_EQ(all[1].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_EQ(all[2].animation, animations::k_FriendlyWave);
	EXPECT_EQ(all[3].animation, creature_mind::k_LookAtMe);
	EXPECT_TRUE(all[5].main);
	EXPECT_EQ(all[5].animation, animations::k_Sad);
	EXPECT_EQ(creature_mind::GetAttentionFromFriend(k_Friend, Always(1)).size(), 3);
}

TEST(CreatureJointActions, TellingACreatureToGoAway)
{
	const auto fight = creature_mind::TellCreatureToSodOff(k_Asker, k_Friend, {}, {}, 20.0f, Always(0));
	ASSERT_EQ(fight.size(), 1);
	EXPECT_EQ(fight[0].kind, Step::Kind::Fight);
	const auto agenda =
	    creature_mind::TellCreatureToSodOff(k_Asker, k_Friend, {0.0f, 1.0f, 0.0f}, {10.0f, 3.0f, 0.0f}, 20.0f, Always(1));
	ASSERT_EQ(agenda.size(), 13);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::Fight);
	// It points as far past the creature as the creature is from it
	EXPECT_TRUE(agenda[5].main);
	EXPECT_FLOAT_EQ(agenda[5].order.point.x, 20.0f);
	EXPECT_FLOAT_EQ(*agenda[5].order.pointHeight, 5.0f);
	EXPECT_FLOAT_EQ(Ordered(agenda[8]).movement.maxDistance, 40.0f);
	EXPECT_EQ(agenda[12].kind, Step::Kind::Remobilise);
}

TEST(CreatureJointActions, ExaminingAnotherCreatureWithAFriend)
{
	const auto agenda = creature_mind::ExamineOtherCreatureWithFriend(k_Friend, 3, Always(0));
	ASSERT_EQ(agenda.size(), 8);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::Play);
	EXPECT_EQ(Ordered(agenda[4]).order.object, 3U);
	// It never lets the mind of the friend go free
	for (const auto& step : agenda)
	{
		EXPECT_NE(step.kind, Step::Kind::Remobilise);
	}
	EXPECT_EQ(creature_mind::ExamineOtherCreatureWithFriend(k_Friend, 3, Always(1)).size(), 6);
}

TEST(CreatureJointActions, SittingOnTopOfAHillWithAFriend)
{
	const auto friends = TwoFriends();
	const float radius = creature_mind::HillTogetherRadius(4.0f, 2.0f);
	EXPECT_FLOAT_EQ(radius, 24.0f);
	const auto agenda = creature_mind::SitOnTopOfHillWithFriend(friends, {100.0f, 100.0f}, radius, true, Always(1));
	ASSERT_EQ(agenda.size(), 9);
	EXPECT_FLOAT_EQ(Ordered(agenda[2]).movement.point.x, 94.0f);
	EXPECT_FLOAT_EQ(agenda[3].movement.point.x, 106.0f);
	EXPECT_FLOAT_EQ(agenda[3].movement.maxDistance, 2.5f * friends.height);
	EXPECT_EQ(Ordered(agenda[5]).movement.kind, creature_mind::Movement::Kind::FaceDownSlope);
	EXPECT_EQ(agenda[7].movement.kind, creature_mind::Movement::Kind::FaceDownSlope);
	EXPECT_TRUE(agenda[8].main);
	EXPECT_EQ(agenda[8].kind, Step::Kind::Static);
	// When it cannot see the friend it goes near it first, and half the time it is happy first
	EXPECT_EQ(creature_mind::SitOnTopOfHillWithFriend(friends, {}, radius, false, Always(0)).size(), 11);
}

TEST(CreatureJointActions, EatingFromAFieldWithAFriend)
{
	const auto friends = TwoFriends();
	const auto agenda = creature_mind::EatFromFieldWithFriend(friends, 5, {300.0f, 400.0f});
	ASSERT_EQ(agenda.size(), 14);
	EXPECT_FLOAT_EQ(Ordered(agenda[1]).movement.point.x, 290.0f);
	EXPECT_FLOAT_EQ(agenda[2].movement.point.x, 310.0f);
	EXPECT_EQ(Ordered(agenda[5]).order.kind, creature_mind::ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ(agenda[6].order.object, 5U);
	EXPECT_TRUE(agenda[8].main);
	EXPECT_EQ(agenda[8].order.kind, creature_mind::ObjectOrder::Kind::Eat);
	// Both are told only to eat, with no thing given: neither swallows its handful
	EXPECT_FALSE(agenda[8].order.swallows);
	EXPECT_EQ(Ordered(agenda[7]).order.kind, creature_mind::ObjectOrder::Kind::Eat);
	EXPECT_FALSE(Ordered(agenda[7]).order.swallows);
	EXPECT_EQ(agenda[11].kind, Step::Kind::Remobilise);
}

TEST(CreatureJointActions, PlayingAThrowingGameWithAFriend)
{
	const auto friends = TwoFriends();
	const auto agenda = creature_mind::PlayThrowingGameWithFriend(friends, 4, {0.0f, 1.0f, 0.0f}, {10.0f, 2.0f, 0.0f}, false);
	ASSERT_EQ(agenda.size(), 15);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 150.0f);
	EXPECT_EQ(agenda[2].request, creature_friendship::RequestKind::Play);
	EXPECT_EQ(agenda[7].order.kind, creature_mind::ObjectOrder::Kind::PickUp);
	EXPECT_TRUE(agenda[8].main);
	EXPECT_FLOAT_EQ(*agenda[8].order.pointHeight, 2.0f + 0.82f * friends.friendHeight);
	EXPECT_FLOAT_EQ(*Ordered(agenda[12]).order.pointHeight, 1.0f + 0.82f * friends.height);
	// It throws what it holds
	EXPECT_EQ(creature_mind::PlayThrowingGameWithFriend(friends, 4, {}, {}, true).size(), 14);
}

TEST(CreatureJointActions, GivingAToyToAFriend)
{
	const auto agenda = creature_mind::GiveFriendToy(k_Asker, k_Friend, 4, false, Always(2));
	ASSERT_EQ(agenda.size(), 12);
	EXPECT_EQ(Ordered(agenda[1]).order.object, k_Asker);
	EXPECT_TRUE(agenda[5].main);
	EXPECT_EQ(agenda[5].order.kind, creature_mind::ObjectOrder::Kind::Discard);
	EXPECT_EQ(agenda[6].kind, Step::Kind::WaitInMap);
	EXPECT_EQ(agenda[10].kind, Step::Kind::WaitForFree);
	EXPECT_EQ(agenda[10].turns, creature_mind::k_WaitForFreeTurns);
	EXPECT_EQ(Ordered(agenda[11]).order.animation, 102u);
}

TEST(CreatureJointActions, ShowingACitadelToAFriend)
{
	const auto friends = TwoFriends();
	const auto agenda = creature_mind::ShowFriendCitadel(friends, {.point = {50.0f, 2.0f, 50.0f}, .radius = 10.0f}, 3.0f, 2.0f,
	                                                     {1.0f, 2.0f, 3.0f}, false, InTurn({0, 0}));
	ASSERT_EQ(agenda.size(), 12);
	EXPECT_EQ(agenda[2].request, creature_friendship::RequestKind::BeShown);
	EXPECT_FLOAT_EQ(Ordered(agenda[3]).movement.maxDistance, 2.0f * 5.0f + 10.0f * 1.2f);
	EXPECT_FLOAT_EQ(agenda[4].movement.maxDistance, 3.0f * 5.0f + 10.0f * 1.2f);
	EXPECT_TRUE(agenda[6].main);
	EXPECT_EQ(agenda[8].kind, Step::Kind::WaitForFree);
	EXPECT_FLOAT_EQ(agenda[11].seconds, creature_mind::k_ShowCitadelWaitSeconds);
	EXPECT_TRUE(agenda[11].gaze->feet);
}

TEST(CreatureJointActions, SeeingAnObject)
{
	EXPECT_TRUE(creature_mind::CanSee(9.0f, 0.0f));
	EXPECT_FALSE(creature_mind::CanSee(10.0f, 0.0f));
	EXPECT_TRUE(creature_mind::CanSee(400.0f, 10.0f));
	EXPECT_FALSE(creature_mind::CanSee(500.0f, 10.0f));
}

TEST(CreatureJointActions, TheOtherJointActionsHaveNoCondition)
{
	creature_plan_actions::Situation situation;
	for (const auto action : {"KissFriend", "TalkToFriend", "TellFriendAJoke", "DanceWithFriend", "ArgueWithFriend",
	                          "MopeAboutWithFriend", "ShowOffToFriend", "GetAttentionFromFriend", "TellCreatureToSodOff",
	                          "ExamineOtherCreatureWithFriend", "SitOnTopOfHillWithFriend", "EatFromFieldWithFriend",
	                          "PlayThrowingGameWithFriend", "GiveFriendToy", "ShowFriendCitadel"})
	{
		const auto* executor = creature_plan_actions::For(action);
		ASSERT_NE(executor, nullptr) << action;
		EXPECT_TRUE(creature_plan_actions::Possible(*executor, situation)) << action;
	}
	EXPECT_EQ(creature_plan_actions::For("GetAttentionFromFriend")->target,
	          creature_plan_actions::Target::CreatureNotFreeForJoint);
	EXPECT_EQ(creature_plan_actions::For("GiveFriendToy")->use, creature_plan_actions::Target::Toy);
}

namespace
{
/// The faces that the creature which asks pulls on the turns of its orders, in agenda order
std::vector<creature_face::Cue> FacesOfOrders(const std::vector<Step>& agenda)
{
	std::vector<creature_face::Cue> faces;
	for (const auto& step : agenda)
	{
		if (step.kind == Step::Kind::Order)
		{
			faces.push_back(step.face);
		}
	}
	return faces;
}
} // namespace

TEST(CreatureJointActions, TheAskerPullsTheFaceOfEachOrder)
{
	using creature_face::Cue;
	using Faces = std::vector<Cue>;
	const auto friends = TwoFriends();
	EXPECT_EQ(FacesOfOrders(creature_mind::WaveAtFriend(k_Asker, k_Friend)), (Faces {Cue::Grimace, Cue::None}));
	EXPECT_EQ(FacesOfOrders(creature_mind::PooWithFriend(friends, Always(0))), (Faces {Cue::None, Cue::Smile, Cue::Smile}));
	EXPECT_EQ(FacesOfOrders(creature_mind::KissFriend(friends, 2.0f, 5.0f, Always(1))),
	          (Faces {Cue::Puzzled, Cue::None, Cue::None}));
	EXPECT_EQ(FacesOfOrders(creature_mind::DanceWithFriend(k_Asker, k_Friend, 10.0f)), (Faces {Cue::None, Cue::Playfulness}));
	EXPECT_EQ(FacesOfOrders(creature_mind::ArgueWithFriend(k_Asker, k_Friend, 10.0f, Always(0))),
	          (Faces {Cue::Smile, Cue::Smile, Cue::Anger, Cue::Anger, Cue::Anger, Cue::Smile}));
	EXPECT_EQ(FacesOfOrders(creature_mind::MopeAboutWithFriend(k_Asker, k_Friend, 10.0f, Always(0))),
	          (Faces {Cue::Sad, Cue::Sad, Cue::Sad}));
	EXPECT_EQ(FacesOfOrders(creature_mind::ShowOffToFriend(k_Asker, k_Friend, 10.0f, {}, Always(0))),
	          (Faces {Cue::Smile, Cue::Smile, Cue::Puzzled, Cue::Puzzled}));
	EXPECT_EQ(FacesOfOrders(creature_mind::TellCreatureToSodOff(k_Asker, k_Friend, {}, {10.0f, 0.0f, 0.0f}, 20.0f, Always(1))),
	          (Faces {Cue::Grimace, Cue::None, Cue::None, Cue::None}));
	EXPECT_EQ(FacesOfOrders(creature_mind::ExamineOtherCreatureWithFriend(k_Friend, 3, Always(0))),
	          (Faces {Cue::Playfulness, Cue::Playfulness, Cue::None}));
	EXPECT_EQ(FacesOfOrders(creature_mind::PlayThrowingGameWithFriend(friends, 4, {}, {}, false)),
	          (Faces {Cue::Grimace, Cue::None, Cue::None}));
	EXPECT_EQ(FacesOfOrders(creature_mind::GiveFriendToy(k_Asker, k_Friend, 4, false, Always(2))),
	          (Faces {Cue::Smile, Cue::Smile, Cue::Smile}));
	EXPECT_EQ(FacesOfOrders(creature_mind::ShowFriendCitadel(friends, {.point = {50.0f, 2.0f, 50.0f}, .radius = 10.0f}, 3.0f,
	                                                         2.0f, {1.0f, 2.0f, 3.0f}, false, InTurn({0, 0}))),
	          (Faces {Cue::Puzzled, Cue::None, Cue::None, Cue::Puzzled}));
	// The other joint actions give their orders with no face
	for (const auto& agenda :
	     {creature_mind::TalkToFriend(k_Asker, k_Friend, 10.0f, Always(0)),
	      creature_mind::SitWithFriend(friends, {10.0f, 20.0f}, 40.0f, Always(0)), creature_mind::BeHappyWithFriend(friends)})
	{
		for (const auto face : FacesOfOrders(agenda))
		{
			EXPECT_EQ(face, Cue::None);
		}
	}
}

namespace
{
/// The looks that the creature which asks takes while its orders are its steps, in agenda order: "-" for no look,
/// "ahead" for a look at itself, "thing" for a look at another thing, "going" for the look on the way to a place or a
/// thing, "ground" for the look at the ground below a place. A look that flutters the eyelids ends in "+flutter".
std::vector<std::string> LooksOfOrders(const std::vector<Step>& agenda)
{
	std::vector<std::string> looks;
	for (const auto& step : agenda)
	{
		if (step.kind != Step::Kind::Order)
		{
			continue;
		}
		std::string look = "-";
		if (step.gaze.has_value())
		{
			look = step.gaze->whileGoing          ? "going"
			       : step.gaze->pointOnGround     ? "ground"
			       : step.gaze->object == k_Asker ? "ahead"
			                                      : "thing";
		}
		if (step.flutterEyelids)
		{
			look += "+flutter";
		}
		looks.push_back(look);
	}
	return looks;
}
} // namespace

TEST(CreatureJointActions, TheAskerTakesTheLookOfEachOrder)
{
	using Looks = std::vector<std::string>;
	const auto friends = TwoFriends();
	EXPECT_EQ(LooksOfOrders(creature_mind::WaveAtFriend(k_Asker, k_Friend)), (Looks {"ahead", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::BeHappyWithFriend(friends)), (Looks {"going", "ahead+flutter", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::PooWithFriend(friends, Always(0))), (Looks {"going", "-", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::SitWithFriend(friends, {10.0f, 20.0f}, 40.0f, Always(0))),
	          (Looks {"going", "ahead", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::SleepWithFriend(friends, {10.0f, 20.0f}, 40.0f, Always(1))),
	          (Looks {"going", "ahead+flutter", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::DrinkWithFriend(friends, {10.0f, 20.0f}, {30.0f, 20.0f}, Always(0))),
	          (Looks {"going", "going", "-", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::KissFriend(friends, 2.0f, 5.0f, Always(1))),
	          (Looks {"ground", "ahead+flutter", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::TalkToFriend(k_Asker, k_Friend, 10.0f, Always(0))),
	          (Looks {"ahead", "ahead+flutter"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::TellFriendAJoke(k_Asker, k_Friend, 10.0f, Always(0))), (Looks {"ahead", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::DanceWithFriend(k_Asker, k_Friend, 10.0f)), (Looks {"ahead", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::TellCreatureToSodOff(k_Asker, k_Friend, {}, {10.0f, 0.0f, 0.0f}, 20.0f, Always(1))),
	          (Looks {"ahead", "-", "going", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::ExamineOtherCreatureWithFriend(k_Friend, 3, Always(0))),
	          (Looks {"thing", "thing", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::PlayThrowingGameWithFriend(friends, 4, {}, {}, false)),
	          (Looks {"ahead", "going", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::GiveFriendToy(k_Asker, k_Friend, 4, false, Always(2))),
	          (Looks {"ahead+flutter", "-", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::ShowFriendCitadel(friends, {.point = {50.0f, 2.0f, 50.0f}, .radius = 10.0f}, 3.0f,
	                                                         2.0f, {1.0f, 2.0f, 3.0f}, false, InTurn({0, 0}))),
	          (Looks {"going", "ground", "ground", "-"}));
	// Arguing, moping and showing off give their orders with no look
	for (const auto& agenda : {creature_mind::ArgueWithFriend(k_Asker, k_Friend, 10.0f, Always(0)),
	                           creature_mind::MopeAboutWithFriend(k_Asker, k_Friend, 10.0f, Always(0)),
	                           creature_mind::ShowOffToFriend(k_Asker, k_Friend, 10.0f, {}, Always(0))})
	{
		for (const auto& look : LooksOfOrders(agenda))
		{
			EXPECT_EQ(look, "-");
		}
	}
}

TEST(CreatureJointActions, ShowingItsHomeToAFriend)
{
	const auto friends = TwoFriends();
	const glm::vec3 home {50.0f, 2.0f, 60.0f};
	const auto agenda = creature_mind::ShowFriendHome(friends, home, {1.0f, 2.0f, 3.0f}, false, InTurn({0, 0}));
	ASSERT_EQ(agenda.size(), 12);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Compassion);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 3.0f * 30.0f);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[2].request, creature_friendship::RequestKind::BeShown);
	EXPECT_FLOAT_EQ(Ordered(agenda[3]).movement.maxDistance, 3.0f * 20.0f);
	EXPECT_FLOAT_EQ(agenda[4].movement.maxDistance, 3.0f * 30.0f);
	EXPECT_EQ(agenda[5].face, creature_face::Cue::Grimace);
	EXPECT_EQ(MainStep(agenda), 6u);
	EXPECT_FLOAT_EQ(*agenda[6].order.seconds, creature_mind::k_PointAtHomeToShowSeconds);
	EXPECT_EQ(agenda[8].kind, Step::Kind::WaitForFree);
	EXPECT_FLOAT_EQ(Ordered(agenda[10]).seconds, creature_mind::k_ShowCitadelWaitSeconds);
	EXPECT_FLOAT_EQ(agenda[11].seconds, creature_mind::k_ShowCitadelWaitSeconds);
	EXPECT_EQ(agenda[11].face, creature_face::Cue::Compassion);
	EXPECT_TRUE(agenda[11].gaze->feet);
	// It does not go near a friend that it can see
	EXPECT_EQ(creature_mind::ShowFriendHome(friends, home, {}, true, InTurn({0, 0})).size(), 11);
}

TEST(CreatureJointActions, ACreatureNeverHasBuiltItsHome)
{
	EXPECT_FALSE(creature_mind::HasBuiltHome(0));
	EXPECT_FALSE(creature_mind::HasBuiltHome(6));
	EXPECT_TRUE(creature_mind::HasBuiltHome(7));
	creature_plan_actions::Situation situation;
	const auto* executor = creature_plan_actions::For("ShowFriendHome");
	ASSERT_NE(executor, nullptr);
	EXPECT_FALSE(creature_plan_actions::Possible(*executor, situation));
	EXPECT_TRUE(creature_plan_actions::Buildable(*executor, situation));
}

TEST(CreatureJointActions, ShowingAFriendAMiracleThatAttacks)
{
	const auto friends = TwoFriends();
	constexpr uint32_t k_Rock = 40;
	const auto agenda = creature_mind::ShowFriendAttackMiracle(friends, 1, k_Rock, {1.0f, 2.0f, 3.0f}, false);
	ASSERT_EQ(agenda.size(), 13);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 1.5f * 30.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::None);
	EXPECT_EQ(agenda[1].movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_EQ(agenda[2].request, creature_friendship::RequestKind::BeShown);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::Playfulness);
	// The friend goes near the thing, to a distance from the height of the creature that shows it
	EXPECT_FLOAT_EQ(Ordered(agenda[3]).movement.maxDistance, 2.5f * 30.0f);
	EXPECT_EQ(*Ordered(agenda[3]).movement.object, k_Rock);
	EXPECT_FLOAT_EQ(agenda[4].movement.maxDistance, 5.0f * 30.0f);
	EXPECT_EQ(agenda[5].movement.kind, creature_mind::Movement::Kind::GetAwayFromObject);
	EXPECT_FLOAT_EQ(agenda[5].movement.maxDistance, 2.0f * 30.0f);
	EXPECT_EQ(agenda[6].face, creature_face::Cue::Fear);
	EXPECT_EQ(agenda[7].face, creature_face::Cue::Grimace);
	EXPECT_FLOAT_EQ(*agenda[7].order.seconds, 2.0f);
	EXPECT_EQ(agenda[9].kind, Step::Kind::WaitForFree);
	EXPECT_EQ(MainStep(agenda), 11u);
	EXPECT_EQ(agenda[11].kind, Step::Kind::Cast);
	EXPECT_EQ(agenda[11].cast.magicType, 1u);
	EXPECT_EQ(*agenda[11].cast.object, k_Rock);
	EXPECT_EQ(agenda[11].face, creature_face::Cue::Grimace);
	EXPECT_FALSE(agenda[11].gaze.has_value());
	EXPECT_EQ(*Ordered(agenda[12]).movement.object, k_Asker);
	// With no thing to show the miracle on, it only turns to the friend
	EXPECT_EQ(creature_mind::ShowFriendAttackMiracle(friends, 1, std::nullopt, {}, true).size(), 1);
}

TEST(CreatureJointActions, ShowingAFriendAMiracleThatMakesSomething)
{
	const auto friends = TwoFriends();
	constexpr uint32_t k_Villager = 41;
	const auto agenda = creature_mind::ShowFriendMakingMiracle(friends, 14, k_Villager, {1.0f, 2.0f, 3.0f}, false);
	ASSERT_EQ(agenda.size(), 14);
	EXPECT_FLOAT_EQ(agenda[0].movement.maxDistance, 3.5f * 30.0f);
	EXPECT_EQ(agenda[0].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[1].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[2].face, creature_face::Cue::None);
	EXPECT_FLOAT_EQ(Ordered(agenda[3]).movement.maxDistance, 5.0f * 30.0f);
	EXPECT_EQ(agenda[5].face, creature_face::Cue::Grimace);
	EXPECT_EQ(*Ordered(agenda[6]).movement.object, k_Villager);
	EXPECT_EQ(agenda[7].face, creature_face::Cue::Compassion);
	EXPECT_EQ(agenda[8].face, creature_face::Cue::Grimace);
	EXPECT_EQ(MainStep(agenda), 12u);
	EXPECT_EQ(agenda[12].cast.magicType, 14u);
	EXPECT_EQ(agenda[12].face, creature_face::Cue::Compassion);
	EXPECT_EQ(creature_mind::ShowFriendMakingMiracle(friends, 14, std::nullopt, {}, true).size(), 1);
}

TEST(CreatureJointActions, TheThingsThatAMiracleCanBeShownOn)
{
	using creature_mind::ShownThingKind;
	EXPECT_TRUE(creature_mind::CanBeSetOnFire(ShownThingKind::SetIntoLand, false));
	EXPECT_FALSE(creature_mind::CanBeSetOnFire(ShownThingKind::SetIntoLand, true));
	EXPECT_FALSE(creature_mind::CanBeSetOnFire(ShownThingKind::SetIntoLandNeverBurns, false));
	EXPECT_FALSE(creature_mind::CanBeSetOnFire(ShownThingKind::LooseObject, false));
	EXPECT_FALSE(creature_mind::CanBeSetOnFire(ShownThingKind::Villager, false));
	EXPECT_FALSE(creature_mind::CanBeSetOnFire(ShownThingKind::NotAnObject, false));

	EXPECT_TRUE(creature_mind::CanBeHelpedByCreature(ShownThingKind::Villager, true));
	EXPECT_FALSE(creature_mind::CanBeHelpedByCreature(ShownThingKind::Villager, false));
	EXPECT_TRUE(creature_mind::CanBeHelpedByCreature(ShownThingKind::LooseObject, false));
	EXPECT_TRUE(creature_mind::CanBeHelpedByCreature(ShownThingKind::SetIntoLand, false));
	EXPECT_TRUE(creature_mind::CanBeHelpedByCreature(ShownThingKind::SetIntoLandNeverBurns, false));
	EXPECT_FALSE(creature_mind::CanBeHelpedByCreature(ShownThingKind::NotAnObject, true));
}

TEST(CreatureJointActions, ConfusingAFriend)
{
	const auto agenda = creature_mind::ConfuseFriend(k_Asker, k_Friend, 10.0f, {50.0f, 3.0f, 60.0f}, Always(0));
	ASSERT_EQ(agenda.size(), 9);
	EXPECT_EQ(agenda[0].request, creature_friendship::RequestKind::BeFriends);
	EXPECT_EQ(agenda[4].face, creature_face::Cue::Smile);
	EXPECT_FLOAT_EQ(agenda[4].seconds, creature_mind::k_FaceTogetherSeconds);
	EXPECT_FALSE(agenda[4].flutterEyelids);
	EXPECT_EQ(MainStep(agenda), 5u);
	EXPECT_EQ(agenda[5].face, creature_face::Cue::Grimace);
	EXPECT_EQ(Ordered(agenda[6]).movement.kind, creature_mind::Movement::Kind::TurnToFace);
	EXPECT_EQ(agenda[7].face, creature_face::Cue::Sad);
	EXPECT_TRUE(agenda[7].flutterEyelids);
	EXPECT_EQ(Ordered(agenda[8]).animation, animations::k_Confused);
}

TEST(CreatureJointActions, ShowingAnotherCreatureHowNiceItIs)
{
	const auto nice = creature_mind::ShowCreatureHowNice(k_Friend, 0.5f, Always(0));
	ASSERT_EQ(nice.size(), 2);
	EXPECT_FLOAT_EQ(nice[0].seconds, 2.0f);
	EXPECT_EQ(*nice[0].gaze->object, k_Friend);
	EXPECT_TRUE(nice[1].main);
	EXPECT_EQ(nice[1].animation, animations::k_Happy);
	EXPECT_EQ(creature_mind::ShowCreatureHowNice(k_Friend, -0.5f, Always(1))[1].animation, creature_mind::k_HateTaunt);
	EXPECT_EQ(creature_mind::ShowCreatureHowNice(k_Friend, -0.5f, Always(0))[1].animation, animations::k_Taunt);
	const auto neither = creature_mind::ShowCreatureHowNice(k_Friend, 0.0f, Always(0));
	EXPECT_EQ(neither[1].kind, Step::Kind::Face);
	EXPECT_EQ(neither[1].animation, animations::k_FirstFace);
	EXPECT_EQ(*neither[1].gaze->object, k_Friend);
	EXPECT_EQ(creature_mind::ShowCreatureHowNice(k_Friend, std::numeric_limits<float>::quiet_NaN(), Always(0))[1].kind,
	          Step::Kind::Face);
}

TEST(CreatureJointActions, TheMiraclesShownToAFriendAreCasts)
{
	for (const auto action : {"ShowFriendSpellFireball", "ShowFriendSpellLightningBolt", "ShowFriendSpellLightningStorm",
	                          "ShowFriendSpellMagicFood", "ShowFriendSpellMagicWood"})
	{
		const auto* executor = creature_plan_actions::For(action);
		ASSERT_NE(executor, nullptr) << action;
		EXPECT_TRUE(creature_plan_actions::IsCast(*executor)) << action;
	}
	EXPECT_EQ(creature_plan_actions::For("ShowFriendObject")->build, creature_plan_actions::Build::Never);
}

TEST(CreatureJointActions, TheFacesOfTheOrdersThatShowAFriendSomething)
{
	using creature_face::Cue;
	using Faces = std::vector<Cue>;
	const auto friends = TwoFriends();
	EXPECT_EQ(FacesOfOrders(creature_mind::ShowFriendHome(friends, {}, {}, true, InTurn({0, 0}))),
	          (Faces {Cue::Puzzled, Cue::None, Cue::None, Cue::Puzzled}));
	EXPECT_EQ(FacesOfOrders(creature_mind::ShowFriendAttackMiracle(friends, 1, 40, {}, true)),
	          (Faces {Cue::None, Cue::None, Cue::None, Cue::Amazed}));
	EXPECT_EQ(FacesOfOrders(creature_mind::ShowFriendMakingMiracle(friends, 14, 40, {}, true)),
	          (Faces {Cue::None, Cue::None, Cue::None, Cue::None, Cue::Amazed}));
	EXPECT_EQ(FacesOfOrders(creature_mind::ConfuseFriend(k_Asker, k_Friend, 10.0f, {}, Always(0))),
	          (Faces {Cue::Smile, Cue::Smile, Cue::Puzzled, Cue::Puzzled}));
}

TEST(CreatureJointActions, TheLooksOfTheOrdersThatShowAFriendSomething)
{
	using Looks = std::vector<std::string>;
	const auto friends = TwoFriends();
	EXPECT_EQ(LooksOfOrders(creature_mind::ShowFriendHome(friends, {}, {}, true, InTurn({0, 0}))),
	          (Looks {"going", "ground", "ground", "-"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::ShowFriendAttackMiracle(friends, 1, 40, {}, true)),
	          (Looks {"going", "ground", "thing", "ahead"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::ShowFriendMakingMiracle(friends, 14, 40, {}, true)),
	          (Looks {"going", "thing", "ground", "thing", "ahead"}));
	EXPECT_EQ(LooksOfOrders(creature_mind::ConfuseFriend(k_Asker, k_Friend, 10.0f, {}, Always(0))),
	          (Looks {"-", "-", "-", "-"}));
}
