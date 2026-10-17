/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "Graphics/HandReflection.h"
#include "Graphics/ObjectTint.h"

using namespace openblack::graphics;

namespace
{
/// The colour that the object shader reads from a packed value: the whole part of minus the value
uint32_t AddedOf(float packed)
{
	return static_cast<uint32_t>(std::floor(-packed));
}

/// The grain flow that the object shader reads from a packed value: the fraction of minus the value
float FlowOf(float packed)
{
	return -packed - std::floor(-packed);
}
} // namespace

// Poison multiplies the light by 0xE8FFDD and adds 0x001000, the colours the game gives a poisoned pot
TEST(ObjectTint, PoisonColoursAreTheGames)
{
	EXPECT_EQ(object_tint::k_PoisonTint, 0xE8FFDDu);
	EXPECT_EQ(object_tint::k_PoisonAdded, 0x001000u);
	EXPECT_EQ(hand_reflection::k_PoisonTint, object_tint::k_PoisonTint);
	EXPECT_EQ(hand_reflection::k_PoisonAdded, object_tint::k_PoisonAdded);
}

// Only a poisoned pot with no fire on it takes the colours of poison: a fire takes their place
TEST(ObjectTint, PoisonTintsAPotWithNoFire)
{
	const auto poisoned = object_tint::PotColours(true, false);
	ASSERT_TRUE(poisoned.has_value());
	EXPECT_EQ(poisoned->tint, object_tint::k_PoisonTint);
	EXPECT_EQ(poisoned->added, object_tint::k_PoisonAdded);
	EXPECT_FALSE(object_tint::PotColours(true, true).has_value());
	EXPECT_FALSE(object_tint::PotColours(false, false).has_value());
	EXPECT_FALSE(object_tint::PotColours(false, true).has_value());
}

// The poisoned pot in the hand takes the same colours as its mirror in the sea
TEST(ObjectTint, ThePotInTheHandAndItsMirrorAgree)
{
	const auto look = hand_reflection::HeldLookOf({.poisonedPot = true});
	const auto colours = object_tint::PotColours(true, false);
	ASSERT_TRUE(colours.has_value());
	EXPECT_EQ(look.tint, colours->tint);
	EXPECT_EQ(look.added, colours->added);
}

// Two added colours add by channel, each at most 255
TEST(ObjectTint, AddedColoursSaturateByChannel)
{
	EXPECT_EQ(object_tint::SumAdded(0x000000u, 0x001000u), 0x001000u);
	EXPECT_EQ(object_tint::SumAdded(0xF0F8FFu, 0x201010u), 0xFFFFFFu);
	EXPECT_EQ(object_tint::SumAdded(0x400000u, 0x00F810u), 0x40F810u);
}

// The packed value keeps the colour whole and the grain flow as its fraction
TEST(ObjectTint, PackedAddedKeepsTheColourAndTheFlow)
{
	EXPECT_EQ(object_tint::PackAdded(0u, 0.0f), -0.0f);
	EXPECT_FLOAT_EQ(FlowOf(object_tint::PackAdded(0u, 0.125f)), 0.125f);
	EXPECT_EQ(AddedOf(object_tint::PackAdded(0u, 0.125f)), 0u);
	// The poison with a quarter of the texture flowed
	const float poisonFlow = object_tint::PackAdded(object_tint::k_PoisonAdded, 0.25f);
	EXPECT_EQ(AddedOf(poisonFlow), object_tint::k_PoisonAdded);
	EXPECT_EQ(FlowOf(poisonFlow), 0.25f);
	// A bright glow keeps all of its colour whatever the flow
	for (const float flow : {0.0f, 0.1f, 0.2f, 0.25f})
	{
		EXPECT_EQ(AddedOf(object_tint::PackAdded(0xFFFFFFu, flow)), 0xFFFFFFu);
		EXPECT_EQ(AddedOf(object_tint::PackAdded(0xFF7F3Fu, flow)), 0xFF7F3Fu);
	}
	// No more than the most flow
	EXPECT_EQ(FlowOf(object_tint::PackAdded(0x001000u, 1.0f)), object_tint::k_MostGrainFlow);
}

// The sea mirrors a held tree at its whole width, whatever share of it a fire leaves in the hand
TEST(ObjectTint, MirroredTreeTakesItsWholeWidth)
{
	EXPECT_FLOAT_EQ(hand_reflection::MirroredWidthFactor(1.0f), 1.0f);
	EXPECT_FLOAT_EQ(hand_reflection::MirroredWidthFactor(0.5f) * 0.5f, hand_reflection::k_MirroredTreeWidth);
	EXPECT_FLOAT_EQ(hand_reflection::MirroredWidthFactor(0.0f), 1.0f);
}
