/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/LeashRules.h"

using namespace openblack;
using namespace openblack::creature_leash;
using creature_desires::Desire;

namespace
{
constexpr float k_Tolerance = 1e-4f;
}

TEST(LeashRules, HandLengthsGrowWithTheCreature)
{
	const auto one = InHand(1.0f);
	EXPECT_NEAR(one.slack, 32.5f, k_Tolerance);
	EXPECT_NEAR(one.max, 77.0f, k_Tolerance);
	const auto two = InHand(2.0f);
	EXPECT_NEAR(two.slack, 43.0f, k_Tolerance);
	EXPECT_NEAR(two.max, 122.0f, k_Tolerance);
}

TEST(LeashRules, TiedLengths)
{
	EXPECT_NEAR(TiedToStatic(50.0f).max, 180.0f, k_Tolerance);
	EXPECT_NEAR(TiedToStatic(200.0f).max, 300.0f, k_Tolerance);
	EXPECT_NEAR(TiedToStatic(200.0f).slack, 150.0f, k_Tolerance);
	EXPECT_NEAR(TiedToStatic(1000.0f).max, 360.0f, k_Tolerance);
	EXPECT_NEAR(TiedToMobile(4.0f).max, 28.0f, k_Tolerance);
	EXPECT_NEAR(TiedToMobile(15.0f).max, 40.0f, k_Tolerance);
	EXPECT_NEAR(TiedToMobile(15.0f).slack, 20.0f, k_Tolerance);
}

TEST(LeashRules, EachLeashLooksItsOwn)
{
	EXPECT_FLOAT_EQ(LookFor(LeashType::Evil).v0, 0.375f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Evil).v1, 0.5f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Rope).v0, 0.125f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Good).v0, 0.25f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Good).halfWidth, 0.225f);
	EXPECT_FLOAT_EQ(LookFor(LeashType::Rope).halfWidth, 0.15f);
}

TEST(LeashRules, LeashesForceTheirFeelings)
{
	EXPECT_EQ(ForcedDesireFor(LeashType::Evil), Desire::Anger);
	EXPECT_EQ(ForcedDesireFor(LeashType::Good), Desire::Compassion);
	EXPECT_FALSE(ForcedDesireFor(LeashType::Rope).has_value());
	EXPECT_EQ(MiracleSightingWeight(true), 3u);
	EXPECT_EQ(MiracleSightingWeight(false), 1u);
}

TEST(LeashRules, Confinement)
{
	EXPECT_TRUE(FreeOfHome(139.0f, true));
	EXPECT_FALSE(FreeOfHome(141.0f, true));
	EXPECT_FALSE(FreeOfHome(10.0f, false));
	EXPECT_TRUE(IsConfined(k_HomeConfinement, false, true));
	EXPECT_FALSE(IsConfined(0.0f, false, true));
	// On a leash that doesn't work, it isn't kept anywhere
	EXPECT_FALSE(IsConfined(12.0f, true, false));
	EXPECT_TRUE(IsConfined(12.0f, true, true));
}

TEST(LeashRules, WalksBackWhenFurtherThanTheRadius)
{
	constexpr Confinement k_Out {.radius = 77.0f, .leashed = true, .distance = 80.0f};
	EXPECT_TRUE(WalksBack(k_Out));
	// At the radius or nearer, it stays
	EXPECT_FALSE(WalksBack({.radius = 77.0f, .leashed = true, .distance = 77.0f}));
	// No area holds it
	EXPECT_FALSE(WalksBack({.radius = 0.0f, .distance = 80.0f}));
}

TEST(LeashRules, OrdersCopiesScriptsAndBadLandStopTheWalkBack)
{
	constexpr Confinement k_Out {.radius = 77.0f, .leashed = true, .distance = 80.0f};
	auto check = k_Out;
	check.leashWorks = false;
	EXPECT_FALSE(WalksBack(check));
	check = k_Out;
	check.orderInForce = true;
	EXPECT_FALSE(WalksBack(check));
	check = k_Out;
	check.mimicking = true;
	EXPECT_FALSE(WalksBack(check));
	check = k_Out;
	check.scriptControlled = true;
	EXPECT_FALSE(WalksBack(check));
	check = k_Out;
	check.teleporting = true;
	EXPECT_FALSE(WalksBack(check));
	check = k_Out;
	check.centreOpen = false;
	EXPECT_FALSE(WalksBack(check));
}

TEST(LeashRules, WalksBackAgainOnlyWhenTheCentreMoved)
{
	Confinement check {.radius = 77.0f, .leashed = true, .distance = 80.0f};
	// Half the radius is 38.5: the place it walks to must be further than that from the centre
	check.returnToCentre = 38.5f;
	EXPECT_FALSE(WalksBack(check));
	check.returnToCentre = 39.0f;
	EXPECT_TRUE(WalksBack(check));
	// A small area needs a move of more than 5
	check = {.radius = 6.0f, .distance = 7.0f, .returnToCentre = 5.0f};
	EXPECT_FALSE(WalksBack(check));
	check.returnToCentre = 5.5f;
	EXPECT_TRUE(WalksBack(check));
}

TEST(LeashRules, WalksBackFasterFurtherAway)
{
	EXPECT_NEAR(ReturnExtraSpeed(0.0f, 50.0f), 0.0f, k_Tolerance);
	EXPECT_NEAR(ReturnExtraSpeed(50.0f, 50.0f), 0.4f, k_Tolerance);
	EXPECT_NEAR(ReturnExtraSpeed(100.0f, 50.0f), 0.8f, k_Tolerance);
	EXPECT_NEAR(ReturnExtraSpeed(500.0f, 50.0f), 0.8f, k_Tolerance);
	EXPECT_NEAR(ReturnExtraSpeed(10.0f, 0.0f), 0.0f, k_Tolerance);
	EXPECT_NEAR(ReturnArrival(77.0f, 30.0f), 77.0f, k_Tolerance);
	EXPECT_NEAR(ReturnArrival(10.0f, 30.0f), 30.0f, k_Tolerance);
}

TEST(LeashRules, WalkBackEndsWithItsAction)
{
	// The walk back continues while the mind does its agenda
	EXPECT_TRUE(ReturnInForce(7, 7, 0, 1));
	// The creature arrived or could not get there: the agenda has no more steps
	EXPECT_FALSE(ReturnInForce(7, 7, 1, 1));
	// A different action started: an order, a new plan or a script
	EXPECT_FALSE(ReturnInForce(7, 8, 0, 1));
}

TEST(LeashRules, LeashedCreaturesWarmOrCoolToEachOther)
{
	EXPECT_FLOAT_EQ(AttitudeChange(LeashType::Evil), -0.1f);
	EXPECT_FLOAT_EQ(AttitudeChange(LeashType::Good), 0.1f);
	EXPECT_FLOAT_EQ(AttitudeChange(LeashType::Rope), 0.0f);
}

TEST(LeashRules, FeelingsAreTouchedAtOnceThenAfterMoreThanSixHundredTurns)
{
	EXPECT_TRUE(AttitudeDue(std::nullopt, 5));
	EXPECT_FALSE(AttitudeDue(100u, 700));
	EXPECT_TRUE(AttitudeDue(100u, 701));
}

TEST(LeashRules, TiedToAVillageTheCreatureWantsToImpressItUnlessItIsFirmlyItsOwn)
{
	// Someone else's village, on the learning leash
	EXPECT_TRUE(ImpressesTown({.leash = LeashType::Rope, .creatureHasPlayer = true, .ownersTown = false}));
	// Never on the aggression leash, nor when impressing is already what it wants most
	EXPECT_FALSE(ImpressesTown({.leash = LeashType::Evil, .creatureHasPlayer = true}));
	EXPECT_FALSE(ImpressesTown({.leash = LeashType::Rope, .impressing = true, .creatureHasPlayer = true}));
	// Its own village believing in its player more than half as much as in anyone else is firmly its own
	EXPECT_FALSE(ImpressesTown({.leash = LeashType::Good,
	                            .creatureHasPlayer = true,
	                            .ownersTown = true,
	                            .beliefInOwner = 5.1f,
	                            .mostOtherBelief = 10.0f}));
	// Its own village believing in another player more than twice as much isn't
	EXPECT_TRUE(ImpressesTown({.leash = LeashType::Good,
	                           .creatureHasPlayer = true,
	                           .ownersTown = true,
	                           .beliefInOwner = 5.0f,
	                           .mostOtherBelief = 10.0f}));
	// A creature of no player leaves the villages of no player alone
	EXPECT_FALSE(ImpressesTown({.leash = LeashType::Rope, .ownersTown = true}));
	EXPECT_TRUE(ImpressesTown({.leash = LeashType::Rope, .ownersTown = false}));
}

TEST(LeashRules, TypeIndices)
{
	EXPECT_EQ(IndexOf(LeashType::Evil), 0u);
	EXPECT_EQ(IndexOf(LeashType::Good), 2u);
	EXPECT_FALSE(IndexOf(LeashType::None).has_value());
}

TEST(LeashRules, LessonsOfWhatThePlayerShows)
{
	const auto evil = LessonsFor(LeashType::Evil, false);
	ASSERT_EQ(evil.size(), 2u);
	EXPECT_EQ(evil[0].desire, Desire::Anger);
	EXPECT_FLOAT_EQ(evil[0].change, 1.0f);
	EXPECT_EQ(evil[1].desire, Desire::Compassion);
	EXPECT_FLOAT_EQ(evil[1].change, -1.0f);
	const auto good = LessonsFor(LeashType::Good, true);
	ASSERT_EQ(good.size(), 2u);
	EXPECT_EQ(good[1].desire, Desire::BeFriends);
	EXPECT_FLOAT_EQ(good[1].change, 1.0f);
	EXPECT_TRUE(LessonsFor(LeashType::Rope, false).empty());
}
