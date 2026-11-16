/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <numeric>
#include <vector>

#include <SaveSlots.h>
#include <gtest/gtest.h>

#include "Editor/LandGenerator.h"

using namespace openblack;
using namespace openblack::editor;

TEST(EditorLandGenerator, TheSaveGoesIntoTheFirstEmptySlotOfThePlayer)
{
	EXPECT_EQ(land_generator::FreeSlot({}), 0u);
	const std::vector<uint32_t> used {0, 1, 3};
	EXPECT_EQ(land_generator::FreeSlot(used), 2u);
	// The slots of the autosaves, the quick save and the save on exit are not slots of the player
	const std::vector<uint32_t> others {0, gamesave::k_FirstAutosaveSlot, gamesave::k_QuickSaveSlot};
	EXPECT_EQ(land_generator::FreeSlot(others), 1u);
}

TEST(EditorLandGenerator, NoSaveIsWrittenOverWhenEachSlotOfThePlayerHasASave)
{
	std::vector<uint32_t> used(gamesave::k_LastPlayerSlot + 1);
	std::iota(used.begin(), used.end(), 0u);
	EXPECT_FALSE(land_generator::FreeSlot(used).has_value());
}

TEST(EditorLandGenerator, TheGeneratorIsOnOnlyWithASaveAndAGeneratedLand)
{
	land_generator::Session session;
	EXPECT_FALSE(land_generator::IsOn(session, "landgen:7"));
	session.returnSlot = 4;
	EXPECT_TRUE(land_generator::IsOn(session, "landgen:7"));
	EXPECT_TRUE(land_generator::IsOn(session, "landgen:7;ranges=5"));
	// Another land that the player loads stops the generator
	EXPECT_FALSE(land_generator::IsOn(session, "./Data/Landscape/Land1.lnd"));
	EXPECT_FALSE(land_generator::IsOn(session, "testbed"));
}

TEST(EditorLandGenerator, TheSaveHasANameThatTheSaveGameRoomKeeps)
{
	// The save game room keeps 31 characters of a name and its end mark
	EXPECT_LT(land_generator::k_SaveName.size(), 32u);
}
