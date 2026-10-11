/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/DiscipleWorkRules.h"

using namespace openblack;
using namespace openblack::disciple_work;

namespace
{
/// A random source that always gives the same number, and remembers the limit it was asked for
struct FixedRandom
{
	uint32_t value {0};
	uint32_t* askedLimit {nullptr};
	uint32_t operator()(uint32_t limit) const
	{
		if (askedLimit != nullptr)
		{
			*askedLimit = limit;
		}
		return value;
	}
};
} // namespace

TEST(DiscipleWork, TraderCarriesWhatTheOtherTownWantsMore)
{
	EXPECT_EQ(TradeResource(0.6f, 0.2f), ResourceType::Food);
	EXPECT_EQ(TradeResource(0.2f, 0.6f), ResourceType::Wood);
	// Wanted as much: wood, unless neither is wanted at all
	EXPECT_EQ(TradeResource(0.4f, 0.4f), ResourceType::Wood);
	EXPECT_FALSE(TradeResource(0.0f, 0.0f).has_value());
	// Food wanted a little and wood not at all is still food
	EXPECT_EQ(TradeResource(0.01f, 0.0f), ResourceType::Food);
}

TEST(DiscipleWork, EmptyTraderFetchesFullTraderDelivers)
{
	EXPECT_EQ(ChooseTradeLeg(0, 500, 10.0f, 10.0f), TradeLeg::Fetch);
	EXPECT_EQ(ChooseTradeLeg(500, 500, 10.0f, 10.0f), TradeLeg::Deliver);
	// Half full, equally far: deliver wins ties
	EXPECT_EQ(ChooseTradeLeg(250, 500, 100.0f, 100.0f), TradeLeg::Deliver);
	// Half full but its own store at hand and the other town far off: it fetches
	EXPECT_EQ(ChooseTradeLeg(250, 500, 0.0f, 4000.0f), TradeLeg::Fetch);
	// Empty, both stores beyond the falloff's reach: the falloff never reaches nothing, so it still fetches
	EXPECT_EQ(ChooseTradeLeg(0, 500, 6000.0f, 6000.0f), TradeLeg::Fetch);
}

TEST(DiscipleWork, EmptyTraderFetchesTheWantedResource)
{
	EXPECT_EQ(TradeFetchResource(ResourceType::None, ResourceType::Wood), ResourceType::Wood);
	EXPECT_EQ(TradeFetchResource(ResourceType::Food, ResourceType::Wood), ResourceType::Food);
}

TEST(DiscipleWork, TraderFetchesUpToItsLimit)
{
	EXPECT_EQ(TradeFetchAmount(500, 0), 500u);
	EXPECT_EQ(TradeFetchAmount(500, 120), 380u);
	EXPECT_EQ(TradeFetchAmount(500, 500), 0u);
	EXPECT_EQ(TradeFetchAmount(500, 600), 0u);
}

TEST(DiscipleWork, ChangedWantsLastTheRestOfTheTurnAndTheNext)
{
	bool changed = false;
	bool lastTurn = false;
	MarkDesiresChanged(changed, lastTurn);
	EXPECT_TRUE(changed);
	EndTownTurn(changed, lastTurn);
	EXPECT_TRUE(changed);
	EndTownTurn(changed, lastTurn);
	EXPECT_FALSE(changed);
	EndTownTurn(changed, lastTurn);
	EXPECT_FALSE(changed);
	// Raised again while up, it lasts a whole turn more
	MarkDesiresChanged(changed, lastTurn);
	EndTownTurn(changed, lastTurn);
	MarkDesiresChanged(changed, lastTurn);
	EndTownTurn(changed, lastTurn);
	EXPECT_TRUE(changed);
	EndTownTurn(changed, lastTurn);
	EXPECT_FALSE(changed);
}

TEST(DiscipleWork, WaitingDiscipleLooksAgainSoonWhenWantsChange)
{
	uint32_t asked = 0;
	EXPECT_EQ(IdleTurnsLeft(300, false, FixedRandom {.value = 7, .askedLimit = &asked}), 299);
	EXPECT_EQ(asked, 0u);
	EXPECT_EQ(IdleTurnsLeft(300, true, FixedRandom {.value = 7, .askedLimit = &asked}), 6);
	EXPECT_EQ(asked, 10u);
	// Drawing nothing it looks this very turn (the count runs below nothing)
	EXPECT_EQ(IdleTurnsLeft(300, true, FixedRandom {.value = 0}), -1);
	EXPECT_EQ(IdleTurnsLeft(1, false, FixedRandom {}), 0);
}

TEST(DiscipleWork, WorkshopRoom)
{
	EXPECT_EQ(WorkshopRoom(false, 0), 3u);
	EXPECT_EQ(WorkshopRoom(true, 0), 2u);
	EXPECT_EQ(WorkshopRoom(true, 2), 0u);
	EXPECT_EQ(WorkshopRoom(false, 3), 0u);
}

TEST(DiscipleWork, WorkshopWantsWoodUntilFullAndStocked)
{
	EXPECT_FLOAT_EQ(WorkshopWoodShortfall(0, 1000), 1.0f);
	EXPECT_NEAR(WorkshopWoodShortfall(250, 1000), 0.75f, 1e-5f);
	EXPECT_FLOAT_EQ(WorkshopWoodShortfall(1000, 1000), 0.0f);
	EXPECT_FLOAT_EQ(WorkshopWoodShortfall(5000, 1000), 0.0f);
	// With room it always wants wood
	EXPECT_FLOAT_EQ(WorkshopWantsWood(1, 5000, 1000), 1.0f);
	// Full, it wants wood only until it has enough for another scaffold
	EXPECT_FLOAT_EQ(WorkshopWantsWood(0, 999, 1000), 1.0f);
	EXPECT_FLOAT_EQ(WorkshopWantsWood(0, 1000, 1000), 0.0f);
}

TEST(DiscipleWork, BestWorkshopIsTheNearestThatWantsWoodAndWorks)
{
	const std::array<WorkshopFacts, 4> workshops {{
	    {.metresAway = 50.0f, .wantsWood = 1.0f, .functional = true},
	    {.metresAway = 10.0f, .wantsWood = 0.0f, .functional = true},
	    {.metresAway = 5.0f, .wantsWood = 1.0f, .functional = false},
	    {.metresAway = 20.0f, .wantsWood = 1.0f, .functional = true},
	}};
	EXPECT_EQ(BestWorkshop(workshops), 3u);
	// Of equals the first in the town's order
	const std::array<WorkshopFacts, 2> equals {
	    {{.metresAway = 20.0f, .wantsWood = 1.0f}, {.metresAway = 20.0f, .wantsWood = 1.0f}}};
	EXPECT_EQ(BestWorkshop(equals), 0u);
	// Wanting nothing, none; far beyond the falloff's reach still counts a little
	const std::array<WorkshopFacts, 2> none {
	    {{.metresAway = 1.0f, .wantsWood = 0.0f}, {.metresAway = 1.0f, .wantsWood = 0.0f}}};
	EXPECT_FALSE(BestWorkshop(none).has_value());
	const std::array<WorkshopFacts, 1> far {{{.metresAway = 900.0f, .wantsWood = 1.0f}}};
	EXPECT_EQ(BestWorkshop(far), 0u);
	EXPECT_FALSE(BestWorkshop({}).has_value());
}

TEST(DiscipleWork, CarryRoomIsASignedCount)
{
	EXPECT_EQ(CarryRoom(300, 0), 300);
	EXPECT_EQ(CarryRoom(300, 120), 180);
	EXPECT_EQ(CarryRoom(300, 300), 0);
	// Carrying more than it may, the room runs below nothing
	EXPECT_EQ(CarryRoom(300, 350), -50);
}
