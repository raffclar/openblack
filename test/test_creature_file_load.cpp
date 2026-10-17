/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureFileLoad.h"

using namespace openblack;
using creature_file_load::Refusal;

namespace
{
creature_file_load::Request Good()
{
	return {.playerInGame = true, .fileOpens = true, .onMap = true, .species = CreatureType::Tortoise};
}
} // namespace

TEST(CreatureFileLoad, APlayerInTheGameWithAFileOnTheMapGetsTheCreature)
{
	EXPECT_EQ(creature_file_load::WhyNot(Good()), Refusal::None);
}

TEST(CreatureFileLoad, APlayerNotInTheGameGetsNoCreatureBeforeAnyOtherCheck)
{
	EXPECT_EQ(creature_file_load::WhyNot({}), Refusal::NoPlayer);
}

TEST(CreatureFileLoad, AFileThatDoesNotOpenGivesNoCreature)
{
	auto request = Good();
	request.fileOpens = false;
	request.onMap = false;
	EXPECT_EQ(creature_file_load::WhyNot(request), Refusal::NoFile);
}

TEST(CreatureFileLoad, APlaceOffTheMapOrAnUnknownSpeciesGivesNoCreature)
{
	auto request = Good();
	request.onMap = false;
	EXPECT_EQ(creature_file_load::WhyNot(request), Refusal::OffMap);
	request = Good();
	request.species.reset();
	EXPECT_EQ(creature_file_load::WhyNot(request), Refusal::UnknownSpecies);
}

TEST(CreatureFileLoad, TheCreatureGrowsUpFullyAtOnce)
{
	EXPECT_EQ(creature_file_load::k_LoadedStage, 13u);
}

TEST(CreatureFileLoad, TheCreatureFacesTheAngleOfANewObject)
{
	EXPECT_EQ(creature_file_load::k_StartYaw, 0.0f);
}
