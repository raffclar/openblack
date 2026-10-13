/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <utility>
#include <vector>

#include <Creature/CreatureReactionRules.h>
#include <gtest/gtest.h>

using namespace openblack;
namespace rules = openblack::creature_reaction;
using creature_desires::Desire;

namespace
{
/// A creature of a person's player that sees the thing, with a table urgency of 40
rules::Facts Seeing()
{
	return {.tablePriority = 40, .humanPlayer = true, .seesThing = true, .turn = 5000};
}

/// A draw that is never asked for
uint32_t NoDraw(uint32_t /*range*/)
{
	ADD_FAILURE() << "no draw expected";
	return 0;
}

/// Draws that always give the same value, counting how many were made
struct FixedDraw
{
	uint32_t value;
	int* draws;
	uint32_t operator()(uint32_t range) const
	{
		EXPECT_EQ(range, rules::k_OtherPlayersDraw);
		++*draws;
		return value;
	}
};
} // namespace

// A creature sees what is within about a third of a turn either side of the way it looks, or in its own map cell
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, SightIsAThirdOfATurnEitherSide)
{
	EXPECT_TRUE(rules::CanSee(0, 0x2aa, false));
	EXPECT_FALSE(rules::CanSee(0, 0x2ab, false));
	EXPECT_TRUE(rules::CanSee(0, 0x800 - 0x2aa, false));
	EXPECT_FALSE(rules::CanSee(0, 0x400, false));
	EXPECT_TRUE(rules::CanSee(0, 0x400, true));
	// Across the wrap from the end of the circle to its start
	EXPECT_TRUE(rules::CanSee(0x7f0, 0x100, false));
}

// The way a creature looks is a game angle: +x is 0, +z a quarter turn, -x a half turn, -z three quarters; no way at
// all counts as the heading 0, three quarters round
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, LookAngleFollowsTheGameAngles)
{
	EXPECT_EQ(rules::LookAngle({1.0f, 0.0f}), 0);
	EXPECT_EQ(rules::LookAngle({0.0f, 1.0f}), 0x200);
	EXPECT_EQ(rules::LookAngle({-1.0f, 0.0f}), 0x400);
	EXPECT_EQ(rules::LookAngle({0.0f, -1.0f}), 0x600);
	// Single floats truncated: an eighth of a turn comes a step short
	EXPECT_EQ(rules::LookAngle({1.0f, 1.0f}), 0xff);
	EXPECT_EQ(rules::LookAngle({-1.0f, -1.0f}), 0x500);
	EXPECT_EQ(rules::LookAngle({0.0f, 0.0f}), 0x600);
}

// Food and a totem worked by the hand catch its eye only when it sees them, and only a person's creature heeds them
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, FoodAndTotemNeedSightAndAPerson)
{
	auto facts = Seeing();
	EXPECT_EQ(rules::Priority(Reaction::ReactToFood, facts, NoDraw), 40);
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandUsingTotem, facts, NoDraw), 40);
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandPuttingStuffInStoragePit, facts, NoDraw), 40);
	facts.seesThing = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFood, facts, NoDraw), 0);
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandUsingTotem, facts, NoDraw), 0);
	// A storage pit filled is heeded unseen
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandPuttingStuffInStoragePit, facts, NoDraw), 40);
	facts = Seeing();
	facts.humanPlayer = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFood, facts, NoDraw), 0);
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandPuttingStuffInStoragePit, facts, NoDraw), 0);
	EXPECT_EQ(rules::Priority(Reaction::ReactToMagicTree, facts, NoDraw), 0);
}

// The hand picking something up is three times as urgent, kept to a byte, when it is its own player's hand holding
// something it sees
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, HandPickUpIsThriceAsUrgent)
{
	auto facts = Seeing();
	facts.ownPlayersHand = true;
	facts.handHolds = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandPickUp, facts, NoDraw), 120);
	facts.tablePriority = 100;
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandPickUp, facts, NoDraw), 44);
	facts.handHolds = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandPickUp, facts, NoDraw), 0);
	facts.handHolds = true;
	facts.ownPlayersHand = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToHandPickUp, facts, NoDraw), 0);
}

// A flying thing thrown by a hand catches its eye while it is in the air, once 900 turns have gone since the last, and
// not while it is catching something; another player's it heeds only on three draws in a hundred
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, FlyingThings)
{
	auto facts = Seeing();
	facts.thrownFromHand = true;
	facts.inTheAir = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFlyingObject, facts, NoDraw), 40);
	facts.sawFlying = facts.turn - rules::k_SightingTurns;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFlyingObject, facts, NoDraw), 0);
	facts.sawFlying = facts.turn - rules::k_SightingTurns - 1;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFlyingObject, facts, NoDraw), 40);
	facts.catching = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFlyingObject, facts, NoDraw), 0);
	facts.catching = false;
	facts.thrownFromHand = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFlyingObject, facts, NoDraw), 0);
	facts.thrownFromHand = true;
	facts.inTheAir = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFlyingObject, facts, NoDraw), 0);
	facts.inTheAir = true;
	facts.otherPlayers = true;
	int draws = 0;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFlyingObject, facts, FixedDraw {2, &draws}), 40);
	EXPECT_EQ(rules::Priority(Reaction::ReactToFlyingObject, facts, FixedDraw {3, &draws}), 0);
	EXPECT_EQ(draws, 2);
}

// A thing crushed catches its eye as a flying thing does, except while it acts out of anger or it is the thing
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, CrushedThings)
{
	auto facts = Seeing();
	EXPECT_EQ(rules::Priority(Reaction::ReactToObjectCrushed, facts, NoDraw), 40);
	facts.angry = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToObjectCrushed, facts, NoDraw), 0);
	facts.angry = false;
	facts.thingIsSelf = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToObjectCrushed, facts, NoDraw), 0);
	facts.thingIsSelf = false;
	facts.sawCrushed = facts.turn - 10;
	EXPECT_EQ(rules::Priority(Reaction::ReactToObjectCrushed, facts, NoDraw), 0);
}

// It watches another creature's fight it sees, unless it is in that fight or fighting itself; it heeds only a creature's
// death
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, FightsAndDeaths)
{
	auto facts = Seeing();
	facts.thingIsCreature = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFight, facts, NoDraw), 40);
	facts.thingFightsSelf = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFight, facts, NoDraw), 0);
	facts.thingFightsSelf = false;
	facts.fighting = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToFight, facts, NoDraw), 0);
	facts = Seeing();
	facts.seesThing = false;
	facts.thingIsCreature = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToDeath, facts, NoDraw), 40);
	facts.thingIsCreature = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToDeath, facts, NoDraw), 0);
	EXPECT_EQ(rules::Priority(Reaction::ReactToDroppedByHand, Seeing(), NoDraw), 0);
	// Kinds it weighs as everything alive does
	EXPECT_EQ(rules::Priority(Reaction::Fainting, Seeing(), NoDraw), std::nullopt);
	EXPECT_EQ(rules::Priority(Reaction::ReactToFire, Seeing(), NoDraw), std::nullopt);
}

// What it does on taking each kind up
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, WhatItDoes)
{
	const auto hand = rules::SetupFor(Reaction::ReactToHandPickUp, false);
	ASSERT_TRUE(hand.has_value() && hand->plan.has_value());
	EXPECT_TRUE(hand->keeps);
	EXPECT_EQ(hand->plan->action, "LookAtHand");
	EXPECT_EQ(hand->plan->desire, Desire::Curiosity);
	EXPECT_TRUE(hand->plan->onSelf);
	// Its watch of the hand is set to a minute and up to another
	EXPECT_TRUE(hand->longHandWatch);

	const auto totem = rules::SetupFor(Reaction::ReactToHandUsingTotem, false);
	ASSERT_TRUE(totem.has_value() && totem->plan.has_value());
	EXPECT_EQ(totem->plan->action, "ExamineByLooking");
	EXPECT_FALSE(totem->plan->onSelf);
	EXPECT_EQ(totem->skill, rules::k_UseTotemSkill);
	EXPECT_TRUE(totem->comesAcross);

	const auto pit = rules::SetupFor(Reaction::ReactToHandPuttingStuffInStoragePit, false);
	ASSERT_TRUE(pit.has_value());
	EXPECT_FALSE(pit->plan.has_value());
	EXPECT_EQ(pit->skill, rules::k_UseStoragePitSkill);

	const auto crushed = rules::SetupFor(Reaction::ReactToObjectCrushed, false);
	ASSERT_TRUE(crushed.has_value() && crushed->plan.has_value());
	EXPECT_TRUE(crushed->notesCrushed);
	EXPECT_EQ(crushed->plan->action, "ExamineByLooking");

	const auto faint = rules::SetupFor(Reaction::Fainting, false);
	ASSERT_TRUE(faint.has_value() && faint->plan.has_value());
	EXPECT_EQ(faint->plan->desire, Desire::Tiredness);
	EXPECT_EQ(faint->plan->action, "SleepAtHome");
	EXPECT_TRUE(faint->plan->onSelf);

	const auto confused = rules::SetupFor(Reaction::Confused, false);
	ASSERT_TRUE(confused.has_value() && confused->plan.has_value());
	EXPECT_EQ(confused->plan->desire, Desire::Tiredness);
	EXPECT_EQ(confused->plan->action, "ExamineByLooking");
	EXPECT_FALSE(confused->plan->onSelf);

	const auto death = rules::SetupFor(Reaction::ReactToDeath, false);
	ASSERT_TRUE(death.has_value() && death->desireSource.has_value());
	EXPECT_EQ(death->desireSource->first, creature_desires::sources::k_Sadness);
	EXPECT_FLOAT_EQ(death->desireSource->second, 1.0f);
	EXPECT_FALSE(death->plan.has_value());
	EXPECT_TRUE(death->keeps);

	const auto food = rules::SetupFor(Reaction::ReactToFood, false);
	ASSERT_TRUE(food.has_value());
	EXPECT_TRUE(food->keeps);
	EXPECT_TRUE(food->comesAcross);
	EXPECT_FALSE(food->plan.has_value());

	// A flying thing it looks up at only while it flies, noting the turn either way
	const auto flying = rules::SetupFor(Reaction::ReactToFlyingObject, true);
	ASSERT_TRUE(flying.has_value() && flying->plan.has_value());
	EXPECT_TRUE(flying->keeps);
	EXPECT_EQ(flying->plan->action, "LookAtFlyingObject");
	const auto landed = rules::SetupFor(Reaction::ReactToFlyingObject, false);
	ASSERT_TRUE(landed.has_value());
	EXPECT_FALSE(landed->keeps);
	EXPECT_TRUE(landed->notesFlying);

	// Kinds it heeds without keeping, and kinds it simply keeps
	for (const auto type :
	     {Reaction::ReactToFight, Reaction::ReactToNewBuilding, Reaction::ReactToDroppedByHand, Reaction::CrowdAround,
	      Reaction::ReactToTownCelebration, Reaction::ReactToMagicShieldStruck, Reaction::ReactToMagicShieldDestroyed})
	{
		const auto setup = rules::SetupFor(type, false);
		ASSERT_TRUE(setup.has_value());
		EXPECT_FALSE(setup->keeps);
	}
	for (const auto type : {Reaction::LookAtObject, Reaction::FollowObject, Reaction::ReactToScaffold})
	{
		const auto setup = rules::SetupFor(type, false);
		ASSERT_TRUE(setup.has_value());
		EXPECT_TRUE(setup->keeps);
		EXPECT_FALSE(setup->plan.has_value());
	}
	// Fire, teleport stones and miracles have rules of their own
	for (const auto type : {Reaction::ReactToFire, Reaction::ReactToTeleport, Reaction::FleeFromSpell,
	                        Reaction::LookAtNiceSpell, Reaction::ReactToMagicTree})
	{
		EXPECT_FALSE(rules::SetupFor(type, false).has_value());
	}
}

// Another creature is as urgent as can be once it is due another look over, unless the creature is busy with its own
// reaction, its player's order, a fight or that creature already
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, AnotherCreatureIsUrgentOnceDueALookOver)
{
	auto facts = Seeing();
	facts.thingIsCreature = true;
	facts.inspectDue = true;
	EXPECT_EQ(rules::Priority(Reaction::ReactToCreature, facts, NoDraw), std::optional<uint8_t>(0xff));
	facts.inspectDue = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToCreature, facts, NoDraw), std::optional<uint8_t>(0));
	facts.inspectDue = true;
	for (auto busy : {&rules::Facts::thingIsSelf, &rules::Facts::orderedByPlayer, &rules::Facts::hasOpponent,
	                  &rules::Facts::planActsOnThing, &rules::Facts::thingGone})
	{
		auto blocked = facts;
		blocked.*busy = true;
		EXPECT_TRUE(rules::BlocksInspecting(blocked));
		EXPECT_EQ(rules::Priority(Reaction::ReactToCreature, blocked, NoDraw), std::optional<uint8_t>(0));
	}
	EXPECT_FALSE(rules::BlocksInspecting(facts));
	// Neither its player nor whether it sees the creature counts
	facts.humanPlayer = false;
	facts.seesThing = false;
	EXPECT_EQ(rules::Priority(Reaction::ReactToCreature, facts, NoDraw), std::optional<uint8_t>(0xff));
}

// A creature never looked over is due at once; one looked over only once more turns than the wait have gone by
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, ACreatureIsDueALookOverOnlyAfterTheWait)
{
	EXPECT_TRUE(rules::InspectDue(std::nullopt, 0, 600));
	EXPECT_FALSE(rules::InspectDue(1000, 1600, 600));
	EXPECT_TRUE(rules::InspectDue(1000, 1601, 600));
	// The game's wait of a minute, at ten turns a second
	EXPECT_EQ(rules::InspectWaitTurns(60.0f, 10), 600u);
	EXPECT_EQ(rules::InspectWaitTurns(1.25f, 10), 12u);
}

// Taking up another creature's reaction, it looks it over, if that is of any use and isn't what it does already
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureReactionRules, TakingUpAnotherCreatureLooksItOverWhenOfUse)
{
	const auto setup = rules::SetupFor(Reaction::ReactToCreature, false);
	ASSERT_TRUE(setup.has_value());
	EXPECT_TRUE(setup->keeps);
	EXPECT_FALSE(setup->comesAcross);
	ASSERT_TRUE(setup->plan.has_value());
	EXPECT_EQ(setup->plan->desire, Desire::Curiosity);
	EXPECT_EQ(setup->plan->action, "InspectCreature");
	EXPECT_TRUE(setup->plan->onlyIfUseful);
	EXPECT_FALSE(setup->plan->onSelf);
}

TEST(CreatureReactionRules, WatchVerdict)
{
	const auto worths = [](float anger, float fear) { return [anger, fear]() { return std::pair {anger, fear}; }; };
	const auto dominant = [](bool value) { return [value]() { return value; }; };
	// No desire about the other creature: the creature continues to watch
	EXPECT_FALSE(creature_reaction::WeighWatch(std::nullopt, Desire::Anger, worths(1.0f, 0.0f), dominant(true)).act);
	// Anger of the creature always wins
	EXPECT_EQ(creature_reaction::WeighWatch(Desire::Anger, Desire::Fear, worths(0.0f, 1.0f), dominant(false)).act,
	          Desire::Anger);
	// The other creature is angry: anger when it has more value than fear, if not fear
	EXPECT_EQ(creature_reaction::WeighWatch(Desire::BeFriends, Desire::Anger, worths(0.6f, 0.5f), dominant(false)).act,
	          Desire::Anger);
	EXPECT_EQ(creature_reaction::WeighWatch(Desire::BeFriends, Desire::Anger, worths(0.5f, 0.5f), dominant(false)).act,
	          Desire::Fear);
	// A desire that is not fear
	EXPECT_EQ(creature_reaction::WeighWatch(Desire::BeFriends, Desire::Fear, worths(0.0f, 0.0f), dominant(false)).act,
	          Desire::BeFriends);
	// Both afraid: the dominant creature examines again without fear, the other creature acts on fear
	EXPECT_TRUE(
	    creature_reaction::WeighWatch(Desire::Fear, Desire::Fear, worths(0.0f, 0.0f), dominant(true)).weighAgainWithoutFear);
	EXPECT_EQ(creature_reaction::WeighWatch(Desire::Fear, Desire::Fear, worths(0.0f, 0.0f), dominant(false)).act, Desire::Fear);
	EXPECT_EQ(creature_reaction::WeighWatch(Desire::Fear, Desire::BeFriends, worths(0.0f, 0.0f), dominant(true)).act,
	          Desire::Fear);
}

TEST(CreatureReactionRules, WorthAndDominance)
{
	EXPECT_FLOAT_EQ(creature_reaction::WorthAbout(0.5f, 0.1f, 0.9f), 0.045f);
	EXPECT_FLOAT_EQ(creature_reaction::WorthAbout(2.0f, 0.9f, 1.6f), 1.0f);
	EXPECT_FLOAT_EQ(creature_reaction::WorthAbout(-1.0f, 0.1f, 1.0f), 0.0f);
	// More than ten years older is dominant; with ten years or less, the order decides
	EXPECT_TRUE(creature_reaction::IsDominant(25, 14, false));
	EXPECT_FALSE(creature_reaction::IsDominant(14, 25, true));
	EXPECT_TRUE(creature_reaction::IsDominant(14, 24, true));
	EXPECT_FALSE(creature_reaction::IsDominant(24, 14, false));
}
