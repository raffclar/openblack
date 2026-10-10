/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::script_spirits (src/Help/ScriptSpirits.h): which advisor a script's spirit means, who says a help text and the
// script's screen fractions.

#include <cstdint>

#include <functional>
#include <limits>

#include <gtest/gtest.h>

#include "Help/ScriptSpirits.h"

using namespace openblack::help::script_spirits;

namespace
{
/// A random draw the test can count
struct FakeRand
{
	uint32_t value {0};
	int calls {0};

	[[nodiscard]] std::function<uint32_t()> Fn()
	{
		return [this]() {
			++calls;
			return value;
		};
	}
};
} // namespace

TEST(ScriptSpirits, DiscreteAlignmentHasSevenSteps)
{
	EXPECT_EQ(DiscreteAlignment(-1.0f), 0);
	EXPECT_EQ(DiscreteAlignment(0.0f), 3);
	// Only the most good alignment reaches the last step
	EXPECT_EQ(DiscreteAlignment(1.0f), 6);
	EXPECT_EQ(DiscreteAlignment(0.99f), 6);
	EXPECT_EQ(DiscreteAlignment(0.7f), 5);
	EXPECT_EQ(DiscreteAlignment(-0.72f), 0);
	EXPECT_EQ(DiscreteAlignment(-0.71f), 1);
	// Above the range it is held at the most good
	EXPECT_EQ(DiscreteAlignment(5.0f), 6);
}

TEST(ScriptSpirits, FixedSpiritsDrawNoRandomNumber)
{
	FakeRand rand;
	EXPECT_EQ(HelpSpiritOf(0, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(HelpSpiritOf(1, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(HelpSpiritOf(2, 3, rand.Fn()), k_EvilSpirit);
	EXPECT_EQ(HelpSpiritOf(99, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(rand.calls, 0);
}

TEST(ScriptSpirits, AlignmentSpiritsGoByThePlayer)
{
	FakeRand rand;
	// The player's alignment: evil below neutral
	EXPECT_EQ(HelpSpiritOf(3, 2, rand.Fn()), k_EvilSpirit);
	EXPECT_EQ(HelpSpiritOf(3, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(HelpSpiritOf(3, 6, rand.Fn()), k_GoodSpirit);
	// Its opposite: evil from neutral up
	EXPECT_EQ(HelpSpiritOf(4, 2, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(HelpSpiritOf(4, 3, rand.Fn()), k_EvilSpirit);
	EXPECT_EQ(HelpSpiritOf(4, 0, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(rand.calls, 0);
}

TEST(ScriptSpirits, RandomSpiritIsEvilAboveHalf)
{
	FakeRand rand;
	rand.value = 50;
	EXPECT_EQ(HelpSpiritOf(5, 3, rand.Fn()), k_GoodSpirit);
	rand.value = 51;
	EXPECT_EQ(HelpSpiritOf(5, 3, rand.Fn()), k_EvilSpirit);
	rand.value = 0;
	EXPECT_EQ(HelpSpiritOf(5, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(rand.calls, 3);
	// No random numbers: the good one
	EXPECT_EQ(HelpSpiritOf(5, 3, {}), k_GoodSpirit);
}

TEST(ScriptSpirits, AdvisorNarratorsTalk)
{
	EXPECT_EQ(SpiritWhoTalks(2), k_GoodSpirit);
	EXPECT_EQ(SpiritWhoTalks(3), k_EvilSpirit);
	EXPECT_EQ(SpiritWhoTalks(0), 0);
	EXPECT_EQ(SpiritWhoTalks(1), 0);
	EXPECT_EQ(SpiritWhoTalks(4), 0);
}

TEST(ScriptSpirits, ScreenFractionsAreZeroToOne)
{
	EXPECT_TRUE(IsScreenFraction(0.0f));
	EXPECT_TRUE(IsScreenFraction(0.75f));
	EXPECT_TRUE(IsScreenFraction(1.0f));
	EXPECT_FALSE(IsScreenFraction(-0.01f));
	EXPECT_FALSE(IsScreenFraction(1.01f));
	EXPECT_FALSE(IsScreenFraction(std::numeric_limits<float>::quiet_NaN()));
}
