/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Magic/HandMotion.h"

using namespace openblack::magic;

namespace
{
constexpr float k_Epsilon = 1e-5f;

constexpr CreaturePourFacts k_AllTrue {
    .pouredBefore = false, .hasCreature = true, .hasSeed = true, .castOfThisComputer = true, .handHoldsSeed = true};
} // namespace

TEST(PhialPour, TheHandPoursOneTimeWhileItHoldsThePhialOfItsOwnCast)
{
	EXPECT_TRUE(PoursPhial(k_AllTrue));
	auto facts = k_AllTrue;
	facts.pouredBefore = true;
	EXPECT_FALSE(PoursPhial(facts));
	facts = k_AllTrue;
	facts.hasCreature = false;
	EXPECT_FALSE(PoursPhial(facts));
	facts = k_AllTrue;
	facts.hasSeed = false;
	EXPECT_FALSE(PoursPhial(facts));
	facts = k_AllTrue;
	facts.castOfThisComputer = false;
	EXPECT_FALSE(PoursPhial(facts));
	facts = k_AllTrue;
	facts.handHoldsSeed = false;
	EXPECT_FALSE(PoursPhial(facts));
}

TEST(PhialPour, ThePourLastsFourSecondsRisesByFourTenthsOfTheHeightAndTipsOneRadian)
{
	const auto settings = PhialPour(30.0f);
	EXPECT_FLOAT_EQ(settings.totalTime, 4.0f);
	EXPECT_FLOAT_EQ(settings.heightToRaise, 30.0f * 0.4f);
	EXPECT_FLOAT_EQ(settings.angleToRaise, 1.0f);
	EXPECT_FALSE(settings.loops);
	EXPECT_TRUE(settings.clampHand);
}

TEST(PhialPour, TheHandStaysAtTheHeadOfTheCreatureWhileThePourIsActive)
{
	PourState pour;
	StartPour(pour, PhialPour(15.0f), {1.0f, 2.0f, 3.0f});
	StepPour(pour, 0.1f);
	const auto atHead = PourPoseAt(pour, 0.5f, glm::vec3 {10.0f, 20.0f, 30.0f});
	ASSERT_TRUE(atHead.pinned.has_value());
	EXPECT_NEAR(atHead.pinned->y, 20.0f, k_Epsilon);
	// The raise and the tilt are the same as the pour without the creature
	EXPECT_NEAR(atHead.raise, PourPoseAt(pour, 0.5f).raise, k_Epsilon);
	EXPECT_NEAR(atHead.tilt, PourPoseAt(pour, 0.5f).tilt, k_Epsilon);
	// With no head point, the hand stays where the pour began
	const auto noHead = PourPoseAt(pour, 0.5f, std::nullopt);
	ASSERT_TRUE(noHead.pinned.has_value());
	EXPECT_NEAR(noHead.pinned->z, 3.0f, k_Epsilon);
}

TEST(PhialPour, ThePourEndsAfterFourSecondsAndTheHandIsFreeAgain)
{
	PourState pour;
	StartPour(pour, PhialPour(15.0f), {1.0f, 2.0f, 3.0f});
	for (int turn = 0; turn < 41; ++turn)
	{
		StepPour(pour, 0.1f);
	}
	EXPECT_FALSE(pour.active);
	EXPECT_FALSE(PourPoseAt(pour, 1.0f, glm::vec3 {10.0f, 20.0f, 30.0f}).pinned.has_value());
}
