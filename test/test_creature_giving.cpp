/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <numbers>

#include <gtest/gtest.h>

#include "Creature/CreatureGiving.h"

using namespace openblack;
using namespace openblack::creature_giving;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;

GiveFacts Friendly()
{
	return {.creatureHasPlayer = true, .giverHasPlayer = true, .friendly = true, .spellSeed = false, .creatureCanPickUp = true};
}
} // namespace

TEST(CreatureGiving, TheOfferLastsElevenTenthsOfASecondAfterLeavingTheCreature)
{
	Offer offer;
	EXPECT_FALSE(offer.Active());
	offer.Renew();
	EXPECT_EQ(offer.ms, 1100);
	EXPECT_FALSE(offer.RunDown(1000, false));
	EXPECT_TRUE(offer.Active());
	EXPECT_FALSE(offer.RunDown(100, false));
	// At exactly nothing left it is over, but runs out only once below nothing
	EXPECT_FALSE(offer.Active());
	offer.Renew();
	EXPECT_TRUE(offer.RunDown(1200, false));
	EXPECT_EQ(offer.ms, 0);
}

TEST(CreatureGiving, TheOfferWaitsWhileTheCreatureReaches)
{
	Offer offer;
	offer.Renew();
	EXPECT_FALSE(offer.RunDown(5000, true));
	EXPECT_EQ(offer.ms, 1100);
}

TEST(CreatureGiving, ThePalmTurnsOverAndASideHoldAQuarterTurnTheOtherWayForTheRightHand)
{
	Offer offer;
	offer.Renew();
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, false, offer, false), k_Pi);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, true, offer, false), -k_Pi);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Side, false, offer, false), k_Pi / 2.0f);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Villager, true, offer, false), -k_Pi / 2.0f);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Tree, false, offer, false), k_Pi / 2.0f);
	EXPECT_FALSE(OfferTurn(HoldType::Magic, false, offer, false).has_value());
}

TEST(CreatureGiving, NearTheEndOfTheOfferItTurnsBackUnlessTheCreatureReaches)
{
	Offer offer {.ms = 299};
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, false, offer, false), 0.0f);
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, false, offer, true), k_Pi);
	offer.ms = 300;
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Above, false, offer, false), k_Pi);
	// Nearly over, even a hold that keeps its turn turns back
	offer.ms = 0;
	EXPECT_FLOAT_EQ(*OfferTurn(HoldType::Magic, false, offer, false), 0.0f);
}

TEST(CreatureGiving, OnlyItsOwnOrAnAlliedPlayerMayGiveAndNeverASeed)
{
	EXPECT_EQ(MayGive(Friendly()), Verdict::Give);
	auto facts = Friendly();
	facts.friendly = false;
	EXPECT_EQ(MayGive(facts), Verdict::No);
	facts = Friendly();
	facts.creatureHasPlayer = false;
	EXPECT_EQ(MayGive(facts), Verdict::No);
	facts = Friendly();
	facts.spellSeed = true;
	EXPECT_EQ(MayGive(facts), Verdict::No);
	facts = Friendly();
	facts.creatureCanPickUp = false;
	EXPECT_EQ(MayGive(facts), Verdict::CantPickUp);
	// Another god's creature says nothing of what it can't pick up
	facts.friendly = false;
	EXPECT_EQ(MayGive(facts), Verdict::No);
}

TEST(CreatureGiving, TheHandMeetsTheCreaturesHandAtTheMomentOfTaking)
{
	const glm::vec3 creatureHand {0.0f, 10.0f, 0.0f};
	const glm::vec3 target {10.0f, 20.0f, 0.0f};
	const auto atTake = DrawToCreature(creatureHand, target, 0.0f, 2.0f);
	ASSERT_TRUE(atTake.has_value());
	EXPECT_FLOAT_EQ(atTake->x, 0.0f);
	EXPECT_FLOAT_EQ(atTake->y, 8.0f);
	const auto halfway = DrawToCreature(creatureHand, target, -250.0f, 2.0f);
	ASSERT_TRUE(halfway.has_value());
	EXPECT_FLOAT_EQ(halfway->x, 5.0f);
	EXPECT_FLOAT_EQ(halfway->y, 14.0f);
	EXPECT_FALSE(DrawToCreature(creatureHand, target, 500.0f, 2.0f).has_value());
}

TEST(CreatureGiving, FifteenTimesTheCreaturesSizeAwayCallsTheGivingOff)
{
	EXPECT_FALSE(CallsOff({0.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f}, 2.0f));
	EXPECT_TRUE(CallsOff({0.0f, 0.0f, 0.0f}, {30.1f, 0.0f, 0.0f}, 2.0f));
}

TEST(CreatureGiving, TheBestScoringDesireDecides)
{
	const GiftFacts facts {.curiosity = {.desire = 0.5f, .usefulness = 0.2f},
	                       .hunger = {.desire = 0.4f, .usefulness = 0.5f},
	                       .playWithPlayer = {.desire = 0.3f, .usefulness = 0.9f}};
	EXPECT_EQ(ChooseUse(facts).use, Use::Throw);
	auto hungry = facts;
	hungry.hunger.usefulness = 1.0f;
	EXPECT_EQ(ChooseUse(hungry).use, Use::Eat);
	EXPECT_FALSE(ChooseUse(hungry).remembersKind);
}

TEST(CreatureGiving, HungerDoesntCountForAKindItCantEat)
{
	const GiftFacts facts {
	    .curiosity = {.desire = 0.2f, .usefulness = 0.5f}, .hunger = {.desire = 0.9f, .usefulness = 1.0f}, .cantEatKind = true};
	EXPECT_EQ(ChooseUse(facts).use, Use::Examine);
}

TEST(CreatureGiving, WithNoDesireATenthStrongTheFirstOfAKindIsExaminedAndLaterOnesActedOnAsWhenLed)
{
	GiftFacts facts {.curiosity = {.desire = 0.09f, .usefulness = 1.0f}, .hunger = {.desire = 0.05f, .usefulness = 1.0f}};
	const auto first = ChooseUse(facts);
	EXPECT_EQ(first.use, Use::Examine);
	EXPECT_TRUE(first.remembersKind);
	facts.givenKindBefore = true;
	EXPECT_EQ(ChooseUse(facts).use, Use::AsLed);
	// A strong desire for which the thing is of no use at all finds no winner either
	const GiftFacts useless {.curiosity = {.desire = 0.9f, .usefulness = 0.0f}, .givenKindBefore = true};
	EXPECT_EQ(ChooseUse(useless).use, Use::AsLed);
}
