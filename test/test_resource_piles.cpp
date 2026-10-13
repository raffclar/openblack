/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Magic/ResourcePiles.h"

using namespace openblack;
using namespace openblack::magic::piles;

namespace
{
constexpr float k_Epsilon = 1e-4f;
}

TEST(ResourcePiles, FoodRisesFastAtFirstAndNeverBeyondFull)
{
	EXPECT_NEAR(ProportionRaised(ResourceType::Food, 0, 1000), 0.0f, k_Epsilon);
	// Anything at all shows a twentieth of its fill and more, eased up to nearly a tenth
	const float least = 0.05f + 0.95f * 0.001f;
	EXPECT_NEAR(ProportionRaised(ResourceType::Food, 1, 1000), 1.0f - (1.0f - least) * (1.0f - least), k_Epsilon);
	const float half = 0.05f + 0.95f * 0.5f;
	EXPECT_NEAR(ProportionRaised(ResourceType::Food, 500, 1000), 1.0f - (1.0f - half) * (1.0f - half), k_Epsilon);
	EXPECT_NEAR(ProportionRaised(ResourceType::Food, 1000, 1000), 1.0f, k_Epsilon);
	EXPECT_NEAR(ProportionRaised(ResourceType::Food, 5000, 1000), 1.0f, k_Epsilon);
}

TEST(ResourcePiles, WoodRisesEvenlyUpToFull)
{
	EXPECT_NEAR(ProportionRaised(ResourceType::Wood, 0, 1000), 0.0f, k_Epsilon);
	EXPECT_NEAR(ProportionRaised(ResourceType::Wood, 500, 1000), 0.05f + 0.95f * 0.5f, k_Epsilon);
	EXPECT_NEAR(ProportionRaised(ResourceType::Wood, 1000, 1000), 1.0f, k_Epsilon);
	EXPECT_NEAR(ProportionRaised(ResourceType::Wood, 2000, 1000), 1.0f, k_Epsilon);
	// However much it holds, it never stands above the ground
	EXPECT_FLOAT_EQ(SunkOffset(ProportionRaised(ResourceType::Wood, 2000, 1000), 3.0f), 0.0f);
	EXPECT_NEAR(SunkOffset(0.25f, 4.0f), -3.0f, k_Epsilon);
}

TEST(ResourcePiles, APileRisesOverASecondAndEndsStill)
{
	auto rise = SunkRise(2.0f);
	EXPECT_FALSE(Shown(rise, 2.0f));
	RiseTo(rise, 0.0f);
	StepRise(rise, 0.5f);
	// It starts off accelerating and eases in: past half way at half time (24 t^2/2 - 96 t^3/6 + 144 t^4/24 of 2)
	EXPECT_NEAR(rise.offset, -2.0f + 1.375f, k_Epsilon);
	EXPECT_GT(rise.currentSpeed, 0.0f);
	EXPECT_TRUE(Shown(rise, 2.0f));
	StepRise(rise, 0.25f);
	EXPECT_GT(rise.offset, -0.625f);
	EXPECT_LT(rise.offset, 0.0f);
	StepRise(rise, 0.25f);
	EXPECT_NEAR(rise.offset, 0.0f, k_Epsilon);
	EXPECT_NEAR(rise.currentSpeed, 0.0f, k_Epsilon);
}

TEST(ResourcePiles, ARiseTurnedMidwayKeepsItsSpeed)
{
	auto rise = SunkRise(2.0f);
	RiseTo(rise, 0.0f);
	StepRise(rise, 0.5f);
	const float offset = rise.offset;
	const float speed = rise.currentSpeed;
	RiseTo(rise, -2.0f);
	// Just after turning back it still moves up a little, at its speed, slowing
	EXPECT_GT(speed, 0.0f);
	StepRise(rise, 0.01f);
	EXPECT_GT(rise.offset, offset);
	EXPECT_LT(rise.offset, offset + speed * 0.01f);
	StepRise(rise, 1.0f);
	EXPECT_NEAR(rise.offset, -2.0f, k_Epsilon);
}

TEST(ResourcePiles, GrainFlowsAQuarterWhenSunk)
{
	EXPECT_NEAR(GrainFlow(-2.0f, 2.0f), 0.25f, k_Epsilon);
	EXPECT_NEAR(GrainFlow(-1.0f, 2.0f), 0.125f, k_Epsilon);
	EXPECT_NEAR(GrainFlow(0.0f, 2.0f), 0.0f, k_Epsilon);
	EXPECT_NEAR(GrainFlow(1.0f, 2.0f), 0.0f, k_Epsilon);
	EXPECT_TRUE(GrainFlows(PotInfo::MagicFood));
	EXPECT_TRUE(GrainFlows(PotInfo::StoragePitFoodPile));
	EXPECT_FALSE(GrainFlows(PotInfo::FoodPile));
	EXPECT_FALSE(GrainFlows(PotInfo::MagicWood));
}

TEST(ResourcePiles, ThudsAreSmallUnderTwoHundred)
{
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 199, 7), 0x4Du + 1u);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 199, 7), 0x5Cu + 1u);
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 200, 7), 0x4Bu + 1u);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 200, 7), 0x56u + 1u);
	EXPECT_FALSE(Thuds(PotInfo::HandFood));
	EXPECT_TRUE(Thuds(PotInfo::MagicFood));
}

TEST(ResourcePiles, PouringSpiralsOutFromItsCell)
{
	EXPECT_EQ(CellOf({25.0f, 37.0f}), glm::ivec2(2, 3));
	EXPECT_EQ(k_SearchCells.front(), glm::ivec2(0, 0));
	EXPECT_EQ(k_SearchCells[1], glm::ivec2(-1, 0));
	EXPECT_EQ(k_SearchCells.back(), glm::ivec2(-1, 1));
	EXPECT_NEAR(RadiusOnGround({2.0f, 3.0f}, 0.5f), 1.5f, k_Epsilon);
}

TEST(ResourcePiles, APourReachesAStoreWithinItsRadiusTimesItsMultiplier)
{
	// A store 20 m across reaches 12 m out from the point it is measured from, the edge itself included
	EXPECT_TRUE(WithinReach({112.0f, 50.0f}, {100.0f, 50.0f}, 10.0f, k_StoreReachMultiplier));
	EXPECT_FALSE(WithinReach({112.5f, 50.0f}, {100.0f, 50.0f}, 10.0f, k_StoreReachMultiplier));
	// A pile reaches twice its radius
	EXPECT_TRUE(WithinReach({0.0f, 3.9f}, {0.0f, 0.0f}, 2.0f, k_PotReachMultiplier));
	EXPECT_FALSE(WithinReach({0.0f, 4.1f}, {0.0f, 0.0f}, 2.0f, k_PotReachMultiplier));
}

TEST(ResourcePiles, OnlyPilesLeadingOnHoldNoMoreThanFull)
{
	EXPECT_TRUE(IsCapped(PotInfo::WoodPile_2));
	EXPECT_FALSE(IsCapped(PotInfo::_COUNT));
	EXPECT_EQ(AmountTaken(900, 300, 1000, true), 100u);
	EXPECT_EQ(AmountTaken(900, 300, 1000, false), 300u);
	EXPECT_EQ(AmountTaken(1000, 300, 1000, true), 0u);
}

TEST(ResourcePiles, AStoresAmountIsSharedOutAmongItsPilesInTurn)
{
	// Five wood piles drawn full at 5000, only the last holding more: Land 1's pit with 30000 wood
	const std::array<PileRoom, 5> wood {{{5000, true}, {5000, true}, {5000, true}, {5000, true}, {5000, false}}};
	EXPECT_EQ(ShareAmongPiles(30000, wood), (std::vector<uint32_t> {5000, 5000, 5000, 5000, 10000}));
	EXPECT_EQ(ShareAmongPiles(7500, wood), (std::vector<uint32_t> {5000, 2500, 0, 0, 0}));
	EXPECT_EQ(ShareAmongPiles(0, wood), (std::vector<uint32_t> {0, 0, 0, 0, 0}));
	// The one food pile holds all it is given
	const std::array<PileRoom, 1> food {{{15000, false}}};
	EXPECT_EQ(ShareAmongPiles(20000, food), (std::vector<uint32_t> {20000}));
	// Piles that are all capped leave the rest unshared
	const std::array<PileRoom, 2> capped {{{100, true}, {100, true}}};
	EXPECT_EQ(ShareAmongPiles(500, capped), (std::vector<uint32_t> {100, 100}));
}

TEST(ResourcePiles, APotIsDrawnAtTheSizeOfWhatItHolds)
{
	// A quarter, and one more for every so much its kind says, up to five times
	EXPECT_FLOAT_EQ(PotScale(0, 20), 0.25f);
	EXPECT_FLOAT_EQ(PotScale(30, 20), 1.75f);
	EXPECT_FLOAT_EQ(PotScale(1000, 20), 5.0f);
}

TEST(ResourcePiles, AHandfulInTheHandIsDrawnWholeWhereItIsHeld)
{
	auto rise = SunkRise(3.6f);
	// On the land a pile wholly under the ground isn't drawn, and part way up it is drawn as far down as it has sunk
	EXPECT_FALSE(DrawnSink(rise, 3.6f, false).has_value());
	RiseTo(rise, -1.0f);
	StepRise(rise, 1.0f);
	ASSERT_TRUE(DrawnSink(rise, 3.6f, false).has_value());
	EXPECT_NEAR(*DrawnSink(rise, 3.6f, false), -1.0f, k_Epsilon);
	// In the hand it doesn't sink at all, however little it holds
	EXPECT_EQ(DrawnSink(SunkRise(3.6f), 3.6f, true), std::optional(0.0f));
	EXPECT_EQ(DrawnSink(rise, 3.6f, true), std::optional(0.0f));
}

TEST(ResourcePiles, LifeIsWhatAPileHoldsOfItsFill)
{
	EXPECT_FLOAT_EQ(Life(500, 1000), 0.5f);
	EXPECT_FLOAT_EQ(Life(0, 1000), 0.0f);
	// A pile holding more than it is drawn full at has more than whole life
	EXPECT_FLOAT_EQ(Life(3000, 1000), 3.0f);
	EXPECT_FLOAT_EQ(Life(10, 0), 0.0f);
}

TEST(ResourcePiles, DamageTakesItsShareOfTheFillAndAtLeastOne)
{
	// A tenth of its life is a tenth of its fill
	EXPECT_EQ(ChangeForDamage(1000, 1000, 0.1f).removed, 100u);
	EXPECT_EQ(ChangeForDamage(1000, 1000, 0.1f).added, 0u);
	// The share is cut to a whole number
	EXPECT_EQ(ChangeForDamage(1000, 1000, 0.00995f).removed, 9u);
	// Any damage at all takes one
	EXPECT_EQ(ChangeForDamage(1000, 1000, 0.0001f).removed, 1u);
	EXPECT_EQ(ChangeForDamage(3, 1000, 0.0001f).removed, 1u);
	// Damage beyond its life takes all it holds
	EXPECT_EQ(ChangeForDamage(40, 1000, 0.5f).removed, 40u);
	// No damage takes nothing
	EXPECT_EQ(ChangeForDamage(40, 1000, 0.0f).removed, 0u);
}

TEST(ResourcePiles, SettingLifeAddsOrRemovesItsShare)
{
	const auto lower = ChangeForLife(500, 1000, 0.25f);
	EXPECT_EQ(lower.removed, 250u);
	EXPECT_EQ(lower.added, 0u);
	const auto higher = ChangeForLife(500, 1000, 0.75f);
	EXPECT_EQ(higher.added, 250u);
	EXPECT_EQ(higher.removed, 0u);
	// The same life changes nothing; the least change is one
	EXPECT_EQ(ChangeForLife(500, 1000, 0.5f).removed, 0u);
	EXPECT_EQ(ChangeForLife(500, 1000, 0.5f).added, 0u);
	EXPECT_EQ(ChangeForLife(500, 1000, 0.5004f).added, 1u);
	// None to all
	EXPECT_EQ(ChangeForLife(500, 1000, 0.0f).removed, 500u);
}
