/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Temple/TempleControlNames.h"

using namespace openblack;
using namespace openblack::temple;

TEST(TempleControls, EachRoomHasTheControlsOfTheGame)
{
	EXPECT_EQ(controls::Names(TempleRoom::Challenge).size(), 106u);
	EXPECT_EQ(controls::Names(TempleRoom::SaveGame).size(), 85u);
	EXPECT_EQ(controls::Names(TempleRoom::CreatureCave).size(), 4u);
	EXPECT_EQ(controls::Names(TempleRoom::Credits).size(), 7u);
	EXPECT_EQ(controls::Names(TempleRoom::Main).size(), 11u);
	EXPECT_TRUE(controls::Names(TempleRoom::Multi).empty());
	EXPECT_TRUE(controls::Names(TempleRoom::Options).empty());
	// The save game room's buttons count from 0 and its pictures from 1
	const auto saveGame = controls::Names(TempleRoom::SaveGame);
	EXPECT_EQ(saveGame.front(), "LH_PICTURE_SAVEGAME_01");
	EXPECT_EQ(saveGame[21], "LH_BUTTON_SAVE00");
	EXPECT_EQ(saveGame.back(), "LH_Scroll_Savegame");
}

TEST(TempleControls, AControlIsTheFirstSubmeshOfItsNameInAnyCase)
{
	const std::vector<std::string> names = {"wall",
	                                        "lh_scroll_lib01",
	                                        "LH_SCROLL_LIB01",
	                                        "LH_Scroll_Lib02",
	                                        "LH_SCROLL_LIB03",
	                                        "LH_SCROLL_LIB04",
	                                        "LH_SCROLL_LIB05",
	                                        "LH_SCROLL_LIB06",
	                                        "LH_SCROLL_LIB07"};
	EXPECT_EQ(controls::SubMeshes(TempleRoom::Credits, names), (std::vector<uint32_t> {1, 3, 4, 5, 6, 7, 8}));
}

TEST(TempleControls, AControlTheRoomDoesNotHaveIsTheFirstSubmesh)
{
	const std::vector<std::string> names = {"wall", "LH_SCROLL_CREATURE", "LH_SCROLL_FIGHT", "LH_SCROLL_LIKES"};
	EXPECT_EQ(controls::SubMeshes(TempleRoom::CreatureCave, names), (std::vector<uint32_t> {0, 1, 2, 3}));
}
