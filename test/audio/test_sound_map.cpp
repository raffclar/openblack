/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Audio/Services/SoundMap.h"

using openblack::audio::sound_map::StratosphereVolume;

TEST(SoundMap, StratosphereVolumeRisesAndFalls)
{
	// the height above the land: none below the atmosphere, full at its top, none again in space
	EXPECT_EQ(StratosphereVolume(50.0f, 100.0f, 200.0f, 400.0f), 0.0f);
	EXPECT_EQ(StratosphereVolume(150.0f, 100.0f, 200.0f, 400.0f), 0.5f);
	EXPECT_EQ(StratosphereVolume(200.0f, 100.0f, 200.0f, 400.0f), 1.0f);
	EXPECT_EQ(StratosphereVolume(300.0f, 100.0f, 200.0f, 400.0f), 0.5f);
	EXPECT_EQ(StratosphereVolume(500.0f, 100.0f, 200.0f, 400.0f), 0.0f);
}
