/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "EngineConfig.h"

using openblack::EngineConfig;

namespace
{

// The debug overlays that write text over things in the world are off when the game starts. The game shows no such
// text, and these overlays show also when the debug menu is hidden.
TEST(EngineConfigOverlays, NoDispenserLabelsByDefault)
{
	EXPECT_FALSE(EngineConfig {}.showDispenserNames);
}

TEST(EngineConfigOverlays, NoVillagerLabelsByDefault)
{
	const EngineConfig config {};
	EXPECT_FALSE(config.showVillagerNames);
	EXPECT_FALSE(config.showVillagerDetails);
}

} // namespace
