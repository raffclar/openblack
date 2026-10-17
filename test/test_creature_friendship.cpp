/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"

using namespace openblack;
using namespace openblack::creature_friendship;
using creature_desires::Desire;
using creature_learning::CreatureAttitude;
using creature_mind::Step;

namespace
{
constexpr uint32_t k_Asker = 7;
constexpr uint32_t k_Asked = 9;

creature_mind::Random NoRandom()
{
	return [](uint32_t) { return 0U; };
}

/// Desires about the asker, each with its own worth
AnswerFacts Asked(RequestKind kind, std::array<float, 5> worths)
{
	return {.kind = kind, .availability = Availability::Free, .worthAbout = [worths](Desire desire) {
		        switch (desire)
		        {
		        case Desire::Play:
			        return worths[0];
		        case Desire::BeFriends:
			        return worths[1];
		        case Desire::Fear:
			        return worths[2];
		        case Desire::Anger:
			        return worths[3];
		        case Desire::Curiosity:
			        return worths[4];
		        default:
			        return 0.0f;
		        }
	        }};
}
} // namespace

TEST(CreatureFriendship, AvailabilityOfTheAskedCreature)
{
	EXPECT_EQ(AvailabilityOf({.asker = k_Asker}), Availability::Free);
	EXPECT_EQ(AvailabilityOf({.dead = true, .asker = k_Asker}), Availability::Refuses);
	// Script control must be the same for both
	EXPECT_EQ(AvailabilityOf({.scriptControlled = true, .asker = k_Asker}), Availability::Refuses);
	EXPECT_EQ(AvailabilityOf({.scriptControlled = true, .askerScriptControlled = true, .asker = k_Asker}), Availability::Free);
	EXPECT_EQ(AvailabilityOf({.partner = k_Asker, .asker = k_Asker}), Availability::AlreadyPartner);
	EXPECT_EQ(AvailabilityOf({.partner = 3U, .asker = k_Asker}), Availability::Refuses);
}

// A creature in a fight has its opponent as its partner: it refuses a third creature and stays free for its opponent
TEST(CreatureFriendship, ACreatureInAFightIsTakenUpWithItsOpponent)
{
	constexpr uint32_t k_Third = 9U;
	EXPECT_EQ(PartnerNow(std::nullopt, std::nullopt), std::nullopt);
	EXPECT_EQ(PartnerNow(3U, std::nullopt), std::optional(3U));
	EXPECT_EQ(PartnerNow(std::nullopt, k_Asker), std::optional(k_Asker));
	EXPECT_EQ(PartnerNow(3U, k_Asker), std::optional(3U));
	EXPECT_EQ(AvailabilityOf({.partner = PartnerNow(std::nullopt, k_Third), .asker = k_Asker}), Availability::Refuses);
	EXPECT_EQ(AvailabilityOf({.partner = PartnerNow(std::nullopt, k_Asker), .asker = k_Asker}), Availability::AlreadyPartner);
}

TEST(CreatureFriendship, AFightIsAgreedToOnlyNearInHeight)
{
	AnswerFacts fight {.kind = RequestKind::Fight, .availability = Availability::Free, .heightRatio = 0.5f};
	EXPECT_TRUE(AnswerOf(fight).agrees);
	fight.heightRatio = 2.0f;
	EXPECT_TRUE(AnswerOf(fight).agrees);
	fight.heightRatio = 2.01f;
	EXPECT_FALSE(AnswerOf(fight).agrees);
	fight.heightRatio = 0.49f;
	EXPECT_FALSE(AnswerOf(fight).agrees);
	// A busy creature refuses, a partner agrees again, and a fight refusal tells the player nothing
	fight.heightRatio = 1.0f;
	fight.availability = Availability::Refuses;
	EXPECT_FALSE(AnswerOf(fight).agrees);
	EXPECT_FALSE(AnswerOf(fight).explanation.has_value());
	fight.availability = Availability::AlreadyPartner;
	EXPECT_TRUE(AnswerOf(fight).agrees);
	// The rule after a forced action does not change a fight
	fight.alwaysAgree = true;
	fight.heightRatio = 3.0f;
	EXPECT_FALSE(AnswerOf(fight).agrees);
}

TEST(CreatureFriendship, PlayIsAgreedToWhenPlayOrFriendshipIsWantedMost)
{
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::Play, {0.3f, 0.0f, 0.2f, 0.1f, 0.0f})).agrees);
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::Play, {0.1f, 0.3f, 0.2f, 0.0f, 0.0f})).agrees);
	const auto feared = AnswerOf(Asked(RequestKind::Play, {0.1f, 0.2f, 0.3f, 0.0f, 0.0f}));
	EXPECT_FALSE(feared.agrees);
	EXPECT_EQ(feared.explanation, k_RefusedToPlay);
	// With nothing worth more than nothing, it refuses
	EXPECT_FALSE(AnswerOf(Asked(RequestKind::Play, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f})).agrees);
	// A later desire must be worth more than an earlier one, not as much
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::Play, {0.3f, 0.0f, 0.3f, 0.3f, 0.0f})).agrees);
}

TEST(CreatureFriendship, FriendshipAndBeingShownHaveTheirOwnDesire)
{
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::BeFriends, {0.0f, 0.4f, 0.3f, 0.0f, 0.0f})).agrees);
	// Play does not count for friendship
	const auto play = AnswerOf(Asked(RequestKind::BeFriends, {0.9f, 0.0f, 0.0f, 0.0f, 0.0f}));
	EXPECT_FALSE(play.agrees);
	EXPECT_EQ(play.explanation, k_RefusedToBeFriends);
	EXPECT_TRUE(AnswerOf(Asked(RequestKind::BeShown, {0.0f, 0.0f, 0.1f, 0.1f, 0.2f})).agrees);
	const auto angry = AnswerOf(Asked(RequestKind::BeShown, {0.0f, 0.0f, 0.0f, 0.3f, 0.2f}));
	EXPECT_FALSE(angry.agrees);
	EXPECT_EQ(angry.explanation, k_RefusedToBeFriends);
}

TEST(CreatureFriendship, AfterAForcedActionAFreeCreatureAlwaysAgrees)
{
	auto facts = Asked(RequestKind::BeFriends, {0.0f, 0.0f, 0.9f, 0.0f, 0.0f});
	facts.alwaysAgree = true;
	EXPECT_TRUE(AnswerOf(facts).agrees);
	// It must still be free, and it says nothing when it is not
	facts.availability = Availability::Refuses;
	const auto busy = AnswerOf(facts);
	EXPECT_FALSE(busy.agrees);
	EXPECT_FALSE(busy.explanation.has_value());
	facts.availability = Availability::AlreadyPartner;
	EXPECT_TRUE(AnswerOf(facts).agrees);
}

TEST(CreatureFriendship, AttentionRisesForAnAgendaThatUsesTheCreature)
{
	AgendaUse use {.plan = {std::nullopt, k_Asked, std::nullopt}, .steps = {k_Asked}, .running = true};
	EXPECT_TRUE(AgendaUses(use, k_Asked));
	EXPECT_FALSE(AgendaUses(AgendaUse {.plan = {1U, 2U, 3U}, .steps = {4U}, .running = true}, k_Asked));
	use.running = false;
	EXPECT_FALSE(AgendaUses(use, k_Asked));
	// With no belief about the creature, a part of the plan that uses nothing counts, as in the game
	EXPECT_TRUE(AgendaUses(AgendaUse {.plan = {std::nullopt, 2U, 3U}, .running = true}, std::nullopt));
	EXPECT_TRUE(AttentionRises(false, false, true));
	EXPECT_FALSE(AttentionRises(true, false, true));
	EXPECT_FALSE(AttentionRises(false, true, true));
	EXPECT_FALSE(AttentionRises(false, false, false));
	CreatureAttitude attitude {.attention = 0.99f};
	ChangeAttention(attitude, k_AttentionStep);
	EXPECT_FLOAT_EQ(attitude.attention, 1.0f);
}

TEST(CreatureFriendship, FeelingsFadeAndFriendsAreMissed)
{
	std::vector<CreatureAttitude> attitudes {{.creature = 1, .howNice = 0.5f, .howImpressive = 1.0f, .attention = 1.0f}};
	Fade(attitudes);
	EXPECT_FLOAT_EQ(attitudes[0].howImpressive, k_ImpressiveFade);
	EXPECT_FLOAT_EQ(attitudes[0].attention, k_AttentionFade);
	EXPECT_FLOAT_EQ(attitudes[0].howNice, 0.5f);
	// Nice enough and given too little attention, it is missed by one less the attention
	EXPECT_FLOAT_EQ(MissedFriends(std::vector<CreatureAttitude> {{.howNice = 0.5f, .attention = 0.25f}}), 0.75f);
	EXPECT_FLOAT_EQ(MissedFriends(std::vector<CreatureAttitude> {{.howNice = 0.4f, .attention = 0.0f}}), 0.0f);
	EXPECT_FLOAT_EQ(MissedFriends(std::vector<CreatureAttitude> {{.howNice = 0.5f, .attention = 0.4f}}), 0.0f);
	EXPECT_FLOAT_EQ(MissedFriends(std::vector<CreatureAttitude> {{.howNice = 0.9f, .attention = 0.1f},
	                                                             {.howNice = 0.6f, .attention = 0.3f}}),
	                1.0f);
}

TEST(CreatureFriendship, WavingAtAFriendIsAJointAction)
{
	const auto agenda = creature_mind::WaveAtFriend(k_Asker, k_Asked);
	ASSERT_EQ(agenda.size(), 7U);
	EXPECT_EQ(agenda[0].kind, Step::Kind::RequestPartner);
	EXPECT_EQ(agenda[0].request, RequestKind::Play);
	EXPECT_EQ(agenda[0].object, k_Asked);
	EXPECT_EQ(agenda[1].kind, Step::Kind::Demobilise);
	ASSERT_EQ(agenda[2].kind, Step::Kind::Order);
	ASSERT_NE(agenda[2].ordered, nullptr);
	EXPECT_EQ(agenda[2].ordered->movement.object, k_Asker);
	EXPECT_FLOAT_EQ(agenda[2].ordered->seconds, creature_mind::k_FriendFacesWaverSeconds);
	// An order has no face and no gaze of its own
	EXPECT_EQ(agenda[2].ordered->face, creature_face::Cue::None);
	EXPECT_FALSE(agenda[2].ordered->gaze.has_value());
	EXPECT_FLOAT_EQ(agenda[3].seconds, creature_mind::k_FaceFriendToWaveSeconds);
	ASSERT_EQ(agenda[4].kind, Step::Kind::Order);
	EXPECT_EQ(agenda[4].ordered->animation, creature_layers::animations::k_FriendlyWave);
	EXPECT_TRUE(agenda[5].main);
	EXPECT_EQ(agenda[5].animation, creature_layers::animations::k_FriendlyWave);
	EXPECT_EQ(agenda[6].kind, Step::Kind::Remobilise);
}

TEST(CreatureFriendship, MissingAFriend)
{
	const auto pine = creature_mind::PineForFriend(k_Asked);
	ASSERT_EQ(pine.size(), 3U);
	EXPECT_EQ(pine[1].order.kind, creature_mind::ObjectOrder::Kind::PointAt);
	EXPECT_TRUE(pine[2].main);
	EXPECT_EQ(pine[2].animation, creature_layers::animations::k_Sad);
	const auto look = creature_mind::LookForFriend(k_Asked, 10.0f);
	ASSERT_EQ(look.size(), 3U);
	EXPECT_TRUE(look[1].main);
	EXPECT_EQ(look[2].movement.kind, creature_mind::Movement::Kind::GoNearObject);
	EXPECT_FLOAT_EQ(look[2].movement.maxDistance, 30.0f);
}

TEST(CreatureFriendship, TheStepsOfAJointActionAreDoneAsTheyStart)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Planned, creature_mind::WaveAtFriend(k_Asker, k_Asked));
	const creature_mind::Senses senses {.seconds = 0.1f};
	const auto request = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(request.requestPartner.has_value());
	EXPECT_EQ(request.requestPartner->creature, k_Asked);
	EXPECT_EQ(mind.step, 1U);
	const auto demobilise = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(demobilise.demobilise.has_value());
	EXPECT_TRUE(demobilise.demobilise->second);
	const auto order = creature_mind::Think(mind, senses, NoRandom());
	ASSERT_TRUE(order.order.has_value());
	EXPECT_EQ(order.order->partner, k_Asked);
	EXPECT_EQ(mind.step, 3U);
}

TEST(CreatureFriendship, AJointActionThatEndsWithAnOrderStopsOnTheNextTurn)
{
	const JointFacts running {
	    .otherAgenda = false, .hadStepsLeft = true, .hasStepsLeft = true, .endedWithOrder = false, .endSeen = false};
	EXPECT_EQ(JointTurnOf(running), JointTurn::Continues);
	auto other = running;
	other.otherAgenda = true;
	EXPECT_EQ(JointTurnOf(other), JointTurn::Ends);
	// The last step was not an order: the joint action stops at once
	auto done = running;
	done.hasStepsLeft = false;
	EXPECT_EQ(JointTurnOf(done), JointTurn::Ends);
	// The last step gave an order: the partner gets one turn to start it
	done.endedWithOrder = true;
	EXPECT_EQ(JointTurnOf(done), JointTurn::EndsNextTurn);
	done.endSeen = true;
	EXPECT_EQ(JointTurnOf(done), JointTurn::Ends);
	// An agenda that had no steps left when the joint action noted it does not stop the joint action
	auto empty = running;
	empty.hadStepsLeft = false;
	empty.hasStepsLeft = false;
	EXPECT_EQ(JointTurnOf(empty), JointTurn::Continues);
}
