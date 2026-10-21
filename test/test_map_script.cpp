/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>
#include <vector>

#include <LHScriptX/MapScript.h>
#include <gtest/gtest.h>

namespace map_script = openblack::lhscriptx::map_script;

TEST(MapScript, TheShippedMapScriptNamesTheFirstLand)
{
	// The whole of the game's own map script
	const std::string source = "LOAD_FEATURE_SCRIPT(\".\scripts\land1.txt\")\r\n";
	EXPECT_EQ(map_script::FeatureScripts(source), std::vector<std::string> {".\scripts\land1.txt"});
}

TEST(MapScript, OtherCommandsAndRemarksGiveNoLand)
{
	const std::string source = "REM LOAD_FEATURE_SCRIPT(\"rem.txt\")\n"
	                           "SET_DATE(1, 2, 3)\n"
	                           "LOAD_LANDSCAPE(\".\data\landscape\land1.lnd\")\n"
	                           "PAUSE_GAME()\n";
	EXPECT_TRUE(map_script::FeatureScripts(source).empty());
}

TEST(MapScript, EachLandScriptComesInTheOrderOfTheSource)
{
	const std::string source = "LOAD_FEATURE_SCRIPT(\"a.txt\")\nSET_TIME(1, 2, 3)\nLOAD_FEATURE_SCRIPT(\"b.txt\")\n";
	EXPECT_EQ(map_script::FeatureScripts(source), (std::vector<std::string> {"a.txt", "b.txt"}));
}
