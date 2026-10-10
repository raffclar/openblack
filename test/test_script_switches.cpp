/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptSwitchChanges.h"

using namespace openblack::script::switches;

TEST(ScriptSwitches, TakingTheCameraOutsideHidesTheLeashesAndScrolls)
{
	const auto changes = CameraTaken(false);
	EXPECT_EQ(changes.leashesDrawn, false);
	EXPECT_EQ(changes.highlightsDrawn, false);
	// The creatures' voices are the script's own choice
	EXPECT_FALSE(changes.otherCreatureVoices.has_value());
}

TEST(ScriptSwitches, TakingTheCameraInTheTempleChangesNothing)
{
	EXPECT_EQ(CameraTaken(true), SwitchChanges {});
}

TEST(ScriptSwitches, GivingTheCameraBackTurnsEverythingBackOn)
{
	const auto changes = CameraReleased();
	EXPECT_EQ(changes.leashesDrawn, true);
	EXPECT_EQ(changes.highlightsDrawn, true);
	EXPECT_EQ(changes.otherCreatureVoices, true);
}

TEST(ScriptSwitches, EndingTheDialogueBringsBackTheCreaturesVoicesOnly)
{
	const auto changes = DialogueEnded();
	EXPECT_EQ(changes.otherCreatureVoices, true);
	EXPECT_FALSE(changes.leashesDrawn.has_value());
	EXPECT_FALSE(changes.highlightsDrawn.has_value());
}
