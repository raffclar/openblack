/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptNameLists.h"

using namespace openblack::script::name_lists;

TEST(ScriptNameLists, AListIsSplitAtSpacesCommasAndTabs)
{
	EXPECT_TRUE(HoldsScript("ScriptA, ScriptB", "ScriptA"));
	EXPECT_TRUE(HoldsScript("ScriptA, ScriptB", "ScriptB"));
	EXPECT_TRUE(HoldsScript("ScriptA,ScriptB\tScriptC", "ScriptC"));
	EXPECT_TRUE(HoldsScript("Lone", "Lone"));
	EXPECT_FALSE(HoldsScript("ScriptA, ScriptB", "Script"));
	EXPECT_FALSE(HoldsScript("ScriptA, ScriptB", "ScriptA,"));
	EXPECT_FALSE(HoldsScript(" , ", ""));
	EXPECT_FALSE(HoldsScript("", "ScriptA"));
}

TEST(ScriptNameLists, ScriptNamesMatchTheirCaseFileNamesDoNot)
{
	EXPECT_FALSE(HoldsScript("scripta", "ScriptA"));
	EXPECT_TRUE(HoldsFile("land1.txt, Land2.txt", "LAND1.TXT"));
	EXPECT_FALSE(HoldsFile("land1.txt", "land1"));
}
