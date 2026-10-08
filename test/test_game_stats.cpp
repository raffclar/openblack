/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Game/GameStats.h"

// GameStats: the data of the in-game menu's Statistics page.

using namespace openblack::game_stats;

TEST(GameStats, HistoryAveragesAndHalves)
{
	History h;
	for (int i = 0; i < 50; ++i)
	{
		h.Add(10.0f); // 50 samples of 10 / 50
	}
	EXPECT_EQ(h.count, 1u);
	EXPECT_NEAR(h.buckets.at(0), 10.0f, 1e-4f);
	// fill the rest: at 500 buckets they are averaged in pairs and the period doubles
	for (uint32_t b = 1; b < History::k_Capacity; ++b)
	{
		for (int i = 0; i < 50; ++i)
		{
			h.Add(static_cast<float>(b % 2));
		}
	}
	EXPECT_EQ(h.count, 250u);
	EXPECT_EQ(h.period, 100u);
	EXPECT_EQ(h.samples, 0u);
	EXPECT_NEAR(h.buckets.at(0), (10.0f + 1.0f) * 0.5f, 1e-4f); // buckets 0 and 1
	EXPECT_NEAR(h.buckets.at(1), 0.5f, 1e-4f);
}

TEST(GameStats, SpellsAndDisciples)
{
	ClearAll();
	openblack::ecs::components::PlayerMagic magic;
	magic.castCount.at(1) = 1;
	magic.castCount.at(35) = 2;
	magic.castCount.at(36) = 3; // the original counts nothing for 36..40
	magic.castCount.at(41) = 4;
	EXPECT_EQ(SpellsCast(magic, 1), 1u);
	EXPECT_EQ(SpellsCast(magic, 35), 2u);
	EXPECT_EQ(SpellsCast(magic, 36), 0u);
	EXPECT_EQ(SpellsCast(magic, 41), 4u);
	DiscipleMade(1, 1);
	DiscipleMade(1, 6); // not counted
	DiscipleMade(1, 8);
	EXPECT_EQ(Of(1).disciples.at(0), 1u);
	EXPECT_EQ(Of(1).disciples.at(5), 0u);
	EXPECT_EQ(Of(1).disciples.at(7), 1u);
}

TEST(GameStats, PopulationTrackerSkipsTheMinimumOnANewMaximum)
{
	ClearAll();
	TrackPopulation(2, 10, 5); // a new maximum: the minimum (-1) stays
	EXPECT_EQ(Of(2).maxPopulation, 15u);
	EXPECT_EQ(Of(2).minPopulation, 0xFFFFFFFFu);
	TrackPopulation(2, 4, 1);
	EXPECT_EQ(Of(2).minPopulation, 5u);
}

TEST(GameStats, DeathsGoToTheTownAndTheKiller)
{
	ClearAll();
	VillagerDied(1, 3);
	EXPECT_EQ(Of(1).deaths, 1u);
	EXPECT_EQ(Of(1).killedVillagers, 0u);
	EXPECT_EQ(Of(3).killedVillagers, 1u);
	BuildingStoppedFunctional(1);
	BuildingStoppedFunctional(1);
	TownTakenOver(1, 7);
	TownTakenOver(1, 3);
	EXPECT_EQ(Of(1).townTakenOverPopulation, 10u);
	EXPECT_EQ(Of(1).buildingStopped, 2u);
}

TEST(GameStats, WorldBeliefSkipsTheMinimumOnANewMaximum)
{
	ClearAll();
	WorldBelief(0, 5.0f); // a new maximum: the minimum (FLT_MAX) stays
	EXPECT_EQ(Of(0).maxWorldBelief, 5.0f);
	EXPECT_EQ(Of(0).minWorldBelief, 3.40282347e+38f);
	WorldBelief(0, 2.0f);
	EXPECT_EQ(Of(0).minWorldBelief, 2.0f);
}

TEST(GameStats, SerializeSize)
{
	// the original's order after the header and the player: 4 + 8 + 0x28 + 0x64 + 2 x 0x7DC + 0x2C + 0x18 (the
	// statics) + 0x90 (the spells) + 4 + 4 + 4
	const auto bytes = Serialize(Stats {}, openblack::ecs::components::PlayerMagic {});
	EXPECT_EQ(bytes.size(), 4u + 8u + 0x28u + 0x64u + 2u * 0x7DCu + 0x2Cu + 0x18u + 0x90u + 12u);
}
