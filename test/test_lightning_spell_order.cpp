/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "Particles/CreatureSpellMaths.h"
#include "Particles/LightningMaths.h"
#include "Particles/ParticleMiracleMaths.h"

using namespace openblack::particles;

// The expected bits come from the game's instructions, worked by hand with each step rounded one time to a float and
// the sines and cosines kept wide until they multiply (generator: bits.py in the lane's scratch folder)

namespace
{
float FromBits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}
} // namespace

TEST(LightningSpellOrder, EachSearchMakesItsGroundPointsWithItsOwnFloatSteps)
{
	const float angle = FromBits(0x4007E6B4u);
	const float distance = FromBits(0x42257AE1u);
	const auto hand = maths::GroundPointOffset(maths::GroundPointSearch::Hand, angle, distance);
	EXPECT_EQ(Bits(hand.x), 0xC1ADBD07u);
	EXPECT_EQ(Bits(hand.y), 0x420CD860u);
	const auto around = maths::GroundPointOffset(maths::GroundPointSearch::Around, angle, distance);
	EXPECT_EQ(Bits(around.x), 0xC1ADBD06u);
	EXPECT_EQ(Bits(around.y), 0x420CD860u);
	const auto spell = maths::GroundPointOffset(maths::GroundPointSearch::SpellTargets, angle, distance);
	EXPECT_EQ(Bits(spell.x), 0xC1ADBD06u);
	EXPECT_EQ(Bits(spell.y), 0x420CD85Fu);
}

TEST(LightningSpellOrder, AForkJointsScaleMultipliesTheChangeByTheJointThenByTheStep)
{
	const float step = 1.0f / (19.0f - 1.0f);
	EXPECT_EQ(Bits(step), 0x3D638E39u);
	EXPECT_EQ(Bits(maths::ForkJointScale(FromBits(0x3FAF5C29u), 2, 7, step)), 0x3ED314DCu);
}

TEST(LightningSpellOrder, TheStrikeConeMakesTheWayUnitByTheInverseOfItsLength)
{
	// The heading's cosine and sine and the half angle's cosine, each kept as a float
	const float cosHeading = static_cast<float>(std::cos(static_cast<double>(0.9f)));
	const float sinHeading = static_cast<float>(std::sin(static_cast<double>(0.9f)));
	const float cosHalf = FromBits(0x3F534932u);
	EXPECT_TRUE(maths::InStrikeCone({100.0f, 0.0f, 200.0f}, cosHeading, sinHeading, cosHalf, {131.25f, 0.0f, 240.5f}));
	EXPECT_FALSE(
	    maths::InStrikeCone({100.0f, 0.0f, 200.0f}, cosHeading, sinHeading, FromBits(0x3F7FF9EFu), {131.25f, 0.0f, 240.5f}));
	EXPECT_TRUE(
	    maths::InStrikeCone({100.0f, 0.0f, 200.0f}, cosHeading, sinHeading, FromBits(0x3F7FF9EEu), {131.25f, 0.0f, 240.5f}));
}

TEST(LightningSpellOrder, TheLandStopsAForkOnlyBeforeItsSplitPoint)
{
	const glm::vec3 from(0.0f, 50.0f, 0.0f);
	const glm::vec3 split(30.0f, 20.0f, 40.0f);
	EXPECT_TRUE(maths::LandHitsFirst(from, split, {15.0f, 0.0f, 20.0f}));
	EXPECT_FALSE(maths::LandHitsFirst(from, split, {45.0f, 0.0f, 60.0f}));
	// A hit as far as the split point does not stop it
	EXPECT_FALSE(maths::LandHitsFirst(from, split, {30.0f, 0.0f, 40.0f}));
}

TEST(LightningSpellOrder, ABoltsMoveSinceItsSearchAddsTheSquaresZFirst)
{
	EXPECT_EQ(Bits(maths::MovedSquared({1234.567f, 55.125f, 4321.9f}, {1201.3f, 49.75f, 4355.06f})), 0x450BB2D8u);
}

TEST(LightningSpellOrder, TheItchFliesWrapAtTheFloatOfTwoPiAndMultiplyTheSineWide)
{
	const auto point = maths::ItchOrbit({1003.25f, 57.5f, 2001.75f}, {1001.5f, 57.25f, 2003.125f}, 13.37f, 4.5f);
	EXPECT_EQ(Bits(point.x), 0x447A6BA6u);
	EXPECT_EQ(Bits(point.y), 0x4264D53Cu);
	EXPECT_EQ(Bits(point.z), 0x44FA7EACu);
}

TEST(LightningSpellOrder, AForksTextureRepeatsAreCutToAWholeNumberAndAtLeastOne)
{
	EXPECT_EQ(maths::TextureRepeats(3, 50.0f, 40.0f), 3);
	EXPECT_EQ(maths::TextureRepeats(2, 10.0f, 40.0f), 1);
	// The middle of the targets at the origin: the division gives no number or an infinite one, which the game makes 1
	EXPECT_EQ(maths::TextureRepeats(3, 50.0f, 0.0f), 1);
	EXPECT_EQ(maths::TextureRepeats(3, 0.0f, 0.0f), 1);
	EXPECT_EQ(maths::TextureRepeats(3, 1e30f, 1e-30f), 1);
}
