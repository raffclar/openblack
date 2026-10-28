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

namespace
{
creature_file_load::BroughtRequest GoodBrought()
{
	return {.citadel = true, .file = true, .readable = true, .onMap = true, .species = CreatureType::Lion};
}
} // namespace

TEST(CreatureFileLoad, AGodWithACitadelAndAFileBringsItsCreature)
{
	EXPECT_EQ(creature_file_load::WhyNotBrought(GoodBrought()), Refusal::None);
}

TEST(CreatureFileLoad, AGodWithNoCitadelBringsNoCreatureFirst)
{
	auto request = GoodBrought();
	request.citadel = false;
	request.file = false;
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::NoCitadel);
}

TEST(CreatureFileLoad, AGodWithNoCreatureFilePlaysWithNoCreature)
{
	auto request = GoodBrought();
	request.file = false;
	request.readable = false;
	request.species.reset();
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::NoFile);
	request = GoodBrought();
	request.readable = false;
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::NoFile);
}

TEST(CreatureFileLoad, AGodWithACreatureBringsNoSecondCreature)
{
	// A saved skirmish keeps the creature of the god, and the load brings the profile's creature again
	auto request = GoodBrought();
	request.hasCreature = true;
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::HasCreature);
	// The game reads the file and the species before it examines the creature of the god
	request.species.reset();
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::UnknownSpecies);
	request = GoodBrought();
	request.hasCreature = true;
	request.readable = false;
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::NoFile);
	// The test of the creature comes before the test of the place
	request = GoodBrought();
	request.hasCreature = true;
	request.onMap = false;
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::HasCreature);
}

TEST(CreatureFileLoad, AnUnknownSpeciesComesBeforeAPlaceOffTheMap)
{
	auto request = GoodBrought();
	request.species.reset();
	request.onMap = false;
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::UnknownSpecies);
	request = GoodBrought();
	request.onMap = false;
	EXPECT_EQ(creature_file_load::WhyNotBrought(request), Refusal::OffMap);
}
