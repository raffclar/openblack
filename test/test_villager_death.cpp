/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/VillagerDeathRules.h"

using namespace openblack;
using namespace openblack::villager_death;

TEST(VillagerDeath, TheAdvisorsSpeakOnlyOfDeathsByMiracleThrowOrDrowningThePlayerHereCaused)
{
	EXPECT_EQ(WhoHears(DeathReason::Spell, true, true), DeathVoice::KillerAdvisors);
	EXPECT_EQ(WhoHears(DeathReason::PlayerInteraction, true, false), DeathVoice::KillerAdvisors);
	EXPECT_EQ(WhoHears(DeathReason::PlayerInteractionDrown, true, true), DeathVoice::KillerAdvisors);
	// The player here put down for a starving, a predator's kill or old age hears nothing, even of its own villager
	EXPECT_EQ(WhoHears(DeathReason::Starving, true, true), DeathVoice::Nobody);
	EXPECT_EQ(WhoHears(DeathReason::Animal, true, true), DeathVoice::Nobody);
	EXPECT_EQ(WhoHears(DeathReason::OldAge, true, true), DeathVoice::Nobody);
}

TEST(VillagerDeath, TheGuidanceLineIsHeardForTheOwnersVillagerKilledByAnother)
{
	for (const auto reason : {DeathReason::Starving, DeathReason::Spell, DeathReason::Animal, DeathReason::Chant,
	                          DeathReason::Sacrifice, DeathReason::Exhaustion})
	{
		EXPECT_EQ(WhoHears(reason, false, true), DeathVoice::OwnerLine) << static_cast<int>(reason);
		EXPECT_EQ(WhoHears(reason, false, false), DeathVoice::Nobody) << static_cast<int>(reason);
	}
	for (const auto reason :
	     {DeathReason::None, DeathReason::PlayerInteraction, DeathReason::PlayerInteractionDrown, DeathReason::OldAge})
	{
		EXPECT_EQ(WhoHears(reason, false, true), DeathVoice::Nobody) << static_cast<int>(reason);
	}
}

TEST(VillagerDeath, LossesAreWarnedOfForMiraclesPredatorsAndThrows)
{
	EXPECT_TRUE(GuidanceOf(DeathReason::Spell).warnsOfLosses);
	EXPECT_TRUE(GuidanceOf(DeathReason::Animal).warnsOfLosses);
	EXPECT_TRUE(GuidanceOf(DeathReason::PlayerInteraction).warnsOfLosses);
	EXPECT_FALSE(GuidanceOf(DeathReason::PlayerInteractionDrown).warnsOfLosses);
	EXPECT_FALSE(GuidanceOf(DeathReason::Starving).warnsOfLosses);
}

TEST(VillagerDeath, TheDeathLineMayBeHeardAnyTurnButTheOneItWasLastHeard)
{
	EXPECT_EQ(GuidanceGapSpread(0, 0.3f), 0u);
	EXPECT_TRUE(GuidanceMayPlay(k_DeathLineKind, 3, true, 0, 1));
	EXPECT_FALSE(GuidanceMayPlay(k_DeathLineKind, 3, true, 0, 0));
	// None with the help system off
	EXPECT_FALSE(GuidanceMayPlay(k_DeathLineKind, 0, false, 0, 100));
}

TEST(VillagerDeath, GuidanceGapsGrowWithTheLeastGapAndShrinkWithTheDrawCubed)
{
	EXPECT_EQ(GuidanceGapSpread(100, 0.0f), 500u);
	EXPECT_EQ(GuidanceGapSpread(100, 0.5f), 437u);
	EXPECT_EQ(GuidanceGapSpread(100, 1.0f), 0u);
	const GuidanceKind quietOnFirstLand {.leastGap = 500, .helpLevel = 2, .onFirstLand = false};
	EXPECT_FALSE(GuidanceMayPlay(quietOnFirstLand, 3, true, 600, 10000));
	EXPECT_TRUE(GuidanceMayPlay(quietOnFirstLand, 3, false, 600, 601));
	EXPECT_FALSE(GuidanceMayPlay(quietOnFirstLand, 3, false, 600, 600));
	EXPECT_FALSE(GuidanceMayPlay(quietOnFirstLand, 1, false, 600, 10000));
}

TEST(VillagerDeath, AChildsDeathMovesItsPlayersAlignmentTwiceAsMuch)
{
	EXPECT_FLOAT_EQ(AlignmentChange(-0.85f, false), -0.85f);
	EXPECT_FLOAT_EQ(AlignmentChange(-0.85f, true), -1.7f);
	EXPECT_FLOAT_EQ(AlignmentChange(0.0f, true), 0.0f);
}

TEST(VillagerDeath, ATownCountsEachDeathByKillerAndCause)
{
	DeathCounts counts;
	CountDeath(counts, 120, 0, DeathReason::Spell);
	CountDeath(counts, 450, 7, DeathReason::Chant);
	CountDeath(counts, 451, 7, DeathReason::Chant);
	EXPECT_EQ(counts.total, 3u);
	EXPECT_EQ(counts.lastDeathTurn, 451u);
	EXPECT_EQ(counts.byPlayer.at(0), 1u);
	EXPECT_EQ(counts.byPlayer.at(7), 2u);
	EXPECT_EQ(counts.byReason.at(static_cast<size_t>(DeathReason::Chant)), 2u);
	EXPECT_EQ(counts.byReason.at(static_cast<size_t>(DeathReason::Spell)), 1u);
	// A player beyond the table is still counted in the total
	CountDeath(counts, 452, 9, DeathReason::OldAge);
	EXPECT_EQ(counts.total, 4u);
}

TEST(VillagerDeath, UpToTenOfTheLivingHeedAPersonsDeathUnlessTheirTownHasAWorkingGraveyard)
{
	EXPECT_EQ(DeathPriority(false, false, false, 0, 100), 100);
	EXPECT_EQ(DeathPriority(false, false, false, 9, 100), 100);
	EXPECT_EQ(DeathPriority(false, false, false, 10, 100), 0);
	EXPECT_EQ(DeathPriority(false, true, false, 0, 100), 0);
	EXPECT_EQ(DeathPriority(false, false, true, 0, 100), 0);
	// A dead creature is heeded by all, graveyard or not
	EXPECT_EQ(DeathPriority(true, true, false, 50, 100), 100);
}

TEST(VillagerDeath, ADeadPersonIsPointedAtOrGoneToAtEvenOddsAndADeadCreatureWatched)
{
	EXPECT_EQ(FirstNoticeOf(false, 0), FirstNotice::Point);
	EXPECT_EQ(FirstNoticeOf(false, 1), FirstNotice::GoTowards);
	EXPECT_EQ(FirstNoticeOf(true, 0), FirstNotice::LookAtCreature);
	EXPECT_EQ(FirstNoticeOf(true, 1), FirstNotice::LookAtCreature);
}

TEST(VillagerDeath, PointingLastsTwoToTwentyTwoSecondsAndMourningFourToSeven)
{
	EXPECT_EQ(PointingLimit(0.0f, 10), 20);
	EXPECT_EQ(PointingLimit(19.99f, 10), 219);
	EXPECT_EQ(PointingLimit(7.55f, 10), 95);
	EXPECT_EQ(MourningLimit(0.0f, 10), 40);
	EXPECT_EQ(MourningLimit(2.99f, 10), 69);
}

TEST(VillagerDeath, AMournerWalksToAFewMetresOffTheBodyThenLooks)
{
	// Within a fifth past the distance it keeps, it looks from where it is
	EXPECT_TRUE(ApproachToBody(4.8f, 4.0f).looks);
	EXPECT_TRUE(ApproachToBody(2.0f, 4.0f).looks);
	// A walk of a metre or less isn't taken
	EXPECT_TRUE(ApproachToBody(4.9f, 3.95f).looks);
	const auto far = ApproachToBody(20.0f, 4.0f);
	EXPECT_FALSE(far.looks);
	EXPECT_FLOAT_EQ(far.walk, 16.0f);
	const auto near = ApproachToBody(5.5f, 4.0f);
	EXPECT_FALSE(near.looks);
	EXPECT_FLOAT_EQ(near.walk, 1.5f);
}

TEST(VillagerDeath, ASoulGoesToHeavenOrHellAtEvenOddsButAChildsAlwaysToHeaven)
{
	EXPECT_EQ(SoulClip(0.0f, false), AnimId::PDead1GotoHeaven);
	EXPECT_EQ(SoulClip(49.9f, false), AnimId::PDead1GotoHeaven);
	EXPECT_EQ(SoulClip(50.0f, false), AnimId::PDead1GotoHell);
	EXPECT_EQ(SoulClip(99.0f, false), AnimId::PDead1GotoHell);
	EXPECT_EQ(SoulClip(99.0f, true), AnimId::PDead1GotoHeaven);
}

TEST(VillagerDeath, ASoulIsSeeThroughFadesOverItsLastHalfSecondAndGoesJustBeforeItsClipEnds)
{
	constexpr uint32_t k_Clip = 3000;
	EXPECT_EQ(SoulAlphaAt(0, k_Clip), 105);
	EXPECT_EQ(SoulAlphaAt(2500, k_Clip), 105);
	EXPECT_EQ(SoulAlphaAt(2750, k_Clip), 52);
	EXPECT_EQ(SoulAlphaAt(2890, k_Clip), 23);
	EXPECT_FALSE(SoulGone(2890, k_Clip));
	EXPECT_TRUE(SoulGone(2891, k_Clip));
	EXPECT_EQ(SoulAlphaAt(3000, k_Clip), 0);
}
