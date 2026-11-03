/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/DeviceRestore.h"

using namespace openblack::graphics;

TEST(DeviceRestore, OnlyALostDeviceLetsTheGameContinue)
{
	EXPECT_TRUE(device_restore::ContinuesAfter(bgfx::Fatal::DeviceLost));
	EXPECT_FALSE(device_restore::ContinuesAfter(bgfx::Fatal::DebugCheck));
	EXPECT_FALSE(device_restore::ContinuesAfter(bgfx::Fatal::InvalidShader));
	EXPECT_FALSE(device_restore::ContinuesAfter(bgfx::Fatal::UnableToInitialize));
	EXPECT_FALSE(device_restore::ContinuesAfter(bgfx::Fatal::UnableToCreateTexture));
}

TEST(DeviceRestore, TheWatchSeesEachNewRestoreOneTime)
{
	device_restore::Watch watch;
	EXPECT_FALSE(watch.Saw(0));
	EXPECT_TRUE(watch.Saw(1));
	EXPECT_FALSE(watch.Saw(1));
	EXPECT_FALSE(watch.Saw(1));
	// Two restores between two looks are one change
	EXPECT_TRUE(watch.Saw(3));
	EXPECT_FALSE(watch.Saw(3));
}
