/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureDevelopment.h"

using namespace openblack;
using namespace openblack::creature_development;

namespace
{
YoungCreature YoungOnFirstLand()
{
	return {.stage = 0, .leashed = false, .localPlayers = true, .land = k_TrainingLand};
}
} // namespace

TEST(CreatureDevelopment, ScriptsSetOnlyTheFourteenStages)
{
	EXPECT_EQ(StageFromScript(0), 0u);
	EXPECT_EQ(StageFromScript(12), 12u);
	EXPECT_EQ(StageFromScript(13), 13u);
	EXPECT_EQ(StageFromScript(14), std::nullopt);
	EXPECT_EQ(StageFromScript(-1), std::nullopt);
}

TEST(CreatureDevelopment, ReachingTheLeavingStageLetsAKeptCreatureGo)
{
	EXPECT_FALSE(ReleasedFromHome(4, k_YoungHomeRadius, false));
	EXPECT_TRUE(ReleasedFromHome(5, k_YoungHomeRadius, false));
	EXPECT_TRUE(ReleasedFromHome(12, k_YoungHomeRadius, false));
	EXPECT_TRUE(ReleasedFromHome(13, k_YoungHomeRadius, false));
}

TEST(CreatureDevelopment, ALeashedOrFreeCreatureIsNotLetGo)
{
	EXPECT_FALSE(ReleasedFromHome(13, k_YoungHomeRadius, true));
	EXPECT_FALSE(ReleasedFromHome(13, 0.0f, false));
}

TEST(CreatureDevelopment, AYoungCreatureOfThePlayersIsKeptAtHomeOnTheFirstLand)
{
	auto creature = YoungOnFirstLand();
	EXPECT_TRUE(KeptAtHome(creature));
	creature.stage = 4;
	EXPECT_TRUE(KeptAtHome(creature));
	creature.stage = k_LeavesHomeStage;
	EXPECT_FALSE(KeptAtHome(creature));
}

TEST(CreatureDevelopment, OnlyOffTheLeashOnTheFirstLandAndThePlayersOwn)
{
	auto leashed = YoungOnFirstLand();
	leashed.leashed = true;
	EXPECT_FALSE(KeptAtHome(leashed));
	auto otherLand = YoungOnFirstLand();
	otherLand.land = 2;
	EXPECT_FALSE(KeptAtHome(otherLand));
	auto someoneElses = YoungOnFirstLand();
	someoneElses.localPlayers = false;
	EXPECT_FALSE(KeptAtHome(someoneElses));
}

TEST(CreatureDevelopment, ACreatureIsMadeFullyGrownUp)
{
	EXPECT_FALSE(KeptAtHome({.localPlayers = true, .land = k_TrainingLand}));
	EXPECT_EQ(k_FullyGrownUp, 13u);
	EXPECT_FLOAT_EQ(k_YoungHomeRadius, 10.0f);
}
