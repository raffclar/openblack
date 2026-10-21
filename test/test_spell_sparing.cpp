/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Magic/SpellSparing.h"

using openblack::magic::spell_sparing::CutsceneSpares;

TEST(SpellSparing, AScriptsCutSceneKeepsTheThingsItHolds)
{
	static_assert(CutsceneSpares(true, true, false));
	EXPECT_TRUE(CutsceneSpares(true, true, false));
}

TEST(SpellSparing, AMiracleThatAScriptControlsStillDestroysThem)
{
	EXPECT_FALSE(CutsceneSpares(true, true, true));
}

TEST(SpellSparing, WithoutACutSceneOrAScriptNothingIsKept)
{
	EXPECT_FALSE(CutsceneSpares(true, false, false));
	EXPECT_FALSE(CutsceneSpares(false, true, false));
	EXPECT_FALSE(CutsceneSpares(false, false, false));
}
