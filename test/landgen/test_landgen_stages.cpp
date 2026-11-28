/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <set>

#include <gtest/gtest.h>
#include <landgen/Players.h>
#include <landgen/Stages.h>

using namespace openblack::landgen;

TEST(LandgenStages, NoTwoStagesHaveTheSameNumber)
{
	EXPECT_TRUE(AllDifferent(k_Stages));
}

TEST(LandgenStages, TheCheckFindsTwoStagesWithTheSameNumber)
{
	constexpr std::array k_Clash {Stage::Hills, Stage::Wind, Stage::Hills};
	EXPECT_FALSE(AllDifferent(k_Clash));
}

TEST(LandgenStages, EachStageOfALandHasItsOwnSeed)
{
	for (const uint64_t landSeed : {1ull, 7ull, 42ull, 77ull})
	{
		std::set<uint64_t> seeds {landSeed};
		for (const auto stage : k_Stages)
		{
			EXPECT_TRUE(seeds.insert(StageSeed(landSeed, stage)).second);
		}
		// The towns of the other players start from seeds of their own
		for (uint32_t player = 1; player < players::k_MostPlayers; ++player)
		{
			const auto playerSeed = players::SeedOfPlayer(landSeed, player);
			EXPECT_TRUE(seeds.insert(playerSeed).second);
			EXPECT_TRUE(seeds.insert(StageSeed(playerSeed, Stage::StartTown)).second);
		}
	}
}

TEST(LandgenStages, TheSeedOfAStageComesOnlyFromTheLandSeedAndTheStage)
{
	EXPECT_EQ(StageSeed(7, Stage::Temperature), StageSeed(7, Stage::Temperature));
	EXPECT_NE(StageSeed(7, Stage::Temperature), StageSeed(8, Stage::Temperature));
	// The climate and the mountains used one seed before the stages had their own numbers
	EXPECT_NE(StageSeed(7, Stage::Temperature), StageSeed(7, Stage::MassifOutline));
	// The cursed ground and the towns used one seed before the stages had their own numbers
	EXPECT_NE(StageSeed(7, Stage::CursedCount), StageSeed(7, Stage::StartTown));
}
