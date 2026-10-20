/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/TemporaryStoreRules.h"

using namespace openblack;
using namespace openblack::ecs::temporary_store;

TEST(TemporaryStore, ATownKeepsOnePileForFoodAndOneForWood)
{
	EXPECT_EQ(SlotOf(ResourceType::Food), 0u);
	EXPECT_EQ(SlotOf(ResourceType::Wood), 1u);
	EXPECT_FALSE(SlotOf(ResourceType::None).has_value());
	EXPECT_FALSE(SlotOf(ResourceType::Any).has_value());
	EXPECT_EQ(PileOf(ResourceType::Food), PotInfo::MagicFood);
	EXPECT_EQ(PileOf(ResourceType::Wood), PotInfo::MagicWood);
}

TEST(TemporaryStore, TheWoodPileIsFiveMetresFromTheGatheringPlaceAndTheFoodPileOnIt)
{
	EXPECT_EQ(OffsetOf(ResourceType::Wood), 5.0f);
	EXPECT_EQ(OffsetOf(ResourceType::Food), 0.0f);
	EXPECT_EQ(k_ClearSearch, 45.0f);
	EXPECT_EQ(k_ClearStep, 1.5f);
	EXPECT_EQ(k_ClearRadius, 2.0f);
}

TEST(TemporaryStore, AtItsTurnTheTownRemovesAnEmptyPileOnlyOnceItsStoragePitWorks)
{
	EXPECT_EQ(AtTownTurn(false, 0, false), TurnStep::Forget);
	EXPECT_EQ(AtTownTurn(false, 0, true), TurnStep::Forget);
	EXPECT_EQ(AtTownTurn(true, 0, false), TurnStep::Keep);
	EXPECT_EQ(AtTownTurn(true, 0, true), TurnStep::Remove);
	EXPECT_EQ(AtTownTurn(true, 30, true), TurnStep::Keep);
}

TEST(TemporaryStore, ANewStoragePitRemovesAnEmptyPileAndReleasesAFullOne)
{
	EXPECT_EQ(OnNewStoragePit(false, 0), NewPitStep::None);
	EXPECT_EQ(OnNewStoragePit(true, 0), NewPitStep::Remove);
	EXPECT_EQ(OnNewStoragePit(true, 1), NewPitStep::Release);
}
