/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "ECS/ArtefactRules.h"

using namespace openblack;
using namespace openblack::ecs::artefact_rules;

TEST(ArtefactRules, TheDanceIsChosenByTheObjectsWidth)
{
	// Under 7 m wide the small object dance
	EXPECT_EQ(DanceForRadius(3.4f), DanceInfo::DanceAroundObjectDiameterLess_7);
	// From 7 m the medium one, up to 10 m
	EXPECT_EQ(DanceForRadius(3.5f), DanceInfo::DanceAroundObjectDiameterLess_10);
	EXPECT_EQ(DanceForRadius(4.9f), DanceInfo::DanceAroundObjectDiameterLess_10);
	// From 10 m the dance for objects wider than 20 m: the one for objects under 20 m is never chosen
	EXPECT_EQ(DanceForRadius(5.0f), DanceInfo::DanceAroundObjectDiameterGreater_20);
	EXPECT_EQ(DanceForRadius(8.0f), DanceInfo::DanceAroundObjectDiameterGreater_20);
}

TEST(ArtefactRules, GlowAndSymbolComeAtWorthOneAndTwo)
{
	EXPECT_FALSE(Glows(1.0f));
	EXPECT_TRUE(Glows(1.01f));
	EXPECT_FALSE(ShowsSymbol(2.0f));
	EXPECT_TRUE(ShowsSymbol(2.01f));
	// Impressiveness is multiplied by 1 + worth only once it glows
	EXPECT_FLOAT_EQ(ImpressiveModifier(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(ImpressiveModifier(1.0f), 1.0f);
	EXPECT_FLOAT_EQ(ImpressiveModifier(3.0f), 4.0f);
}

TEST(ArtefactRules, AnArtefactImpressesOnlyATownItDidntLastBelongTo)
{
	EXPECT_TRUE(WillImpress(1.5f, false));
	EXPECT_FALSE(WillImpress(1.5f, true));
	EXPECT_FALSE(WillImpress(1.0f, false));
}

TEST(ArtefactRules, VillagersWantADanceLessTheFartherAndFullerItIs)
{
	// Not at all while it is dancing
	EXPECT_FLOAT_EQ(JoiningDesire({.dancing = true, .maximumDancers = 20}, 1.0f), 0.0f);
	// No dance yet: only the distance counts
	EXPECT_FLOAT_EQ(JoiningDesire({.maximumDancers = 20}, 0.75f), 0.75f);
	// A quarter full: three quarters as much
	EXPECT_FLOAT_EQ(JoiningDesire({.dancers = 5u, .maximumDancers = 20}, 1.0f), 0.75f);
	// Full or over: none
	EXPECT_FLOAT_EQ(JoiningDesire({.dancers = 25u, .maximumDancers = 20}, 1.0f), 0.0f);
	// A table with no most counts no fullness
	EXPECT_FLOAT_EQ(JoiningDesire({.dancers = 25u, .maximumDancers = 0}, 0.5f), 0.5f);
	// A dance under way past its length is wanted more, by how far past
	EXPECT_FLOAT_EQ(
	    JoiningDesire({.dancers = 0u, .maximumDancers = 20, .running = true, .duration = 100, .sinceStart = 50}, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(
	    JoiningDesire({.dancers = 0u, .maximumDancers = 20, .running = true, .duration = 100, .sinceStart = 300}, 1.0f), 3.0f);
}

TEST(ArtefactRules, AWaitingVillagerWalksCloserMakesRoomThenStartsTheDance)
{
	EXPECT_EQ(WaitingTurn(12.5f, 0, true, 200), Waiting::WalkCloser);
	EXPECT_EQ(WaitingTurn(12.0f, 0, false, 200), Waiting::MakeRoom);
	EXPECT_EQ(WaitingTurn(12.0f, 0, true, 200), Waiting::Wait);
	// Room is asked for only on its first turn
	EXPECT_EQ(WaitingTurn(5.0f, 3, false, 200), Waiting::Wait);
	EXPECT_EQ(WaitingTurn(5.0f, 199, true, 200), Waiting::Wait);
	EXPECT_EQ(WaitingTurn(5.0f, 200, true, 200), Waiting::StartDance);
}

TEST(ArtefactRules, ADanceIsOverOnceItsLengthHasPassed)
{
	EXPECT_EQ(DanceTimeLeft(900, 1000, 500), 400u);
	EXPECT_EQ(DanceTimeLeft(900, 1400, 500), 0u);
	EXPECT_EQ(DanceTimeLeft(900, 2000, 500), 0u);
}

TEST(ArtefactRules, ATownsBeliefInItsOwnerCountsFullyToTwoAThirdBeyondAndFourAtMost)
{
	EXPECT_FLOAT_EQ(TownModifier(std::nullopt), 1.0f);
	EXPECT_FLOAT_EQ(TownModifier(0.5f), 0.5f);
	EXPECT_FLOAT_EQ(TownModifier(2.0f), 2.0f);
	EXPECT_NEAR(TownModifier(5.0f), 3.0f, 1e-6f);
	EXPECT_FLOAT_EQ(TownModifier(8.0f), 4.0f);
	EXPECT_FLOAT_EQ(TownModifier(10.0f), 4.0f);
}

TEST(ArtefactRules, ADanceInTheOwnersTownRaisesWorthEverMoreSlowly)
{
	// A full dance of its whole length, a town believing 1 in its owner, a rock's multiplier
	const float gain = DanceGain({.townModifier = 1.0f,
	                              .artefactMultiplier = 0.0002f,
	                              .dancers = 20,
	                              .maximumDancers = 20,
	                              .danced = 900,
	                              .duration = 900,
	                              .artefactSpeed = 1.0f});
	EXPECT_FLOAT_EQ(gain, 0.0002f);
	// Half the dancers for half the length, at twice the land's speed
	EXPECT_FLOAT_EQ(DanceGain({.townModifier = 1.0f,
	                           .artefactMultiplier = 0.0002f,
	                           .dancers = 10,
	                           .maximumDancers = 20,
	                           .danced = 450,
	                           .duration = 900,
	                           .artefactSpeed = 2.0f}),
	                0.0001f);
	// Times 300, added in full while worth no more than 1
	EXPECT_NEAR(AddToWorth(0.01f, gain), 0.07f, 1e-6f);
	// Divided by its worth beyond 1
	EXPECT_NEAR(AddToWorth(2.0f, gain), 2.03f, 1e-6f);
}

TEST(ArtefactRules, ATownGivesBeliefForADanceByWorthImpressivenessAndDancersPerHead)
{
	// Worth 1 or less gives none
	EXPECT_FLOAT_EQ(
	    DancingBelief({.worth = 1.0f, .impressiveness = 2.0f, .dancers = 10, .population = 20, .beliefMultiplier = 5.0f}),
	    0.0f);
	// Worth 3: 3 x impressiveness x 10/20 x 5
	EXPECT_FLOAT_EQ(
	    DancingBelief({.worth = 3.0f, .impressiveness = 2.0f, .dancers = 10, .population = 20, .beliefMultiplier = 5.0f}),
	    15.0f);
	// A town without people counts 1 for its dancers
	EXPECT_FLOAT_EQ(
	    DancingBelief({.worth = 3.0f, .impressiveness = 2.0f, .dancers = 10, .population = 0, .beliefMultiplier = 5.0f}), 6.0f);
}

TEST(ArtefactRules, BeliefComesEveryHundredTurnsWithARandomPartOfAQuarter)
{
	EXPECT_EQ(GiftRandomLimit(100), 25u);
	EXPECT_FALSE(BeliefGiftDue(100, 0, 100));
	EXPECT_TRUE(BeliefGiftDue(101, 0, 100));
	EXPECT_TRUE(BeliefGiftDue(80, 24, 100));
	EXPECT_FALSE(BeliefGiftDue(76, 24, 100));
}

TEST(ArtefactRules, AWorshipSitePowersUpItsArtefactsByItsShareOfThePlayersPeople)
{
	EXPECT_FLOAT_EQ(SitePowerUp(1.0f, 0.00001f, 10, 100), 0.000001f);
	EXPECT_FLOAT_EQ(SitePowerUp(2.0f, 0.00001f, 10, 100), 0.000002f);
	EXPECT_FLOAT_EQ(SitePowerUp(1.0f, 0.00001f, 10, 0), 0.0f);
}

TEST(ArtefactRules, AWondersPowerCountsTheThingsInItsPlacesCellOnceForEveryCellOfItsReach)
{
	EXPECT_FLOAT_EQ(WonderReach(0.5f), 15.0f);
	EXPECT_FLOAT_EQ(WonderReach(2.0f), 30.0f);
	const std::array<WonderNeighbour, 2> things {
	    WonderNeighbour {.distance = 5.0f, .artefactWorth = 1.5f, .impressiveness = 1.0f},
	    WonderNeighbour {.distance = 20.0f, .artefactWorth = 9.0f, .impressiveness = 9.0f},
	};
	// The far one is out of reach; the near one counts its worth and a tenth of its impressiveness each pass
	EXPECT_FLOAT_EQ(WonderPower(0.5f, things, 1), 0.5f + 1.6f);
	EXPECT_FLOAT_EQ(WonderPower(0.5f, things, 9), 0.5f + 9.0f * 1.6f);
	// At least a quarter
	EXPECT_FLOAT_EQ(WonderPower(0.0f, {}, 9), 0.25f);
	// Its scale: power at most 5, times one less a tenth of it
	EXPECT_FLOAT_EQ(WonderScale(1.0f), 0.9f);
	EXPECT_FLOAT_EQ(WonderScale(5.0f), 2.5f);
	EXPECT_FLOAT_EQ(WonderScale(12.0f), 2.5f);
}
