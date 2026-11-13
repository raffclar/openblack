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

TEST(DeviceRestore, TheKeptBytesAddUpToTheTotal)
{
	constexpr device_restore::Kept k_Kept {
	    .buffers = 1, .dynamicBuffers = 20, .shaders = 300, .textures = 4000, .textureChanges = 50000};
	static_assert(k_Kept.Total() == 54321);
	EXPECT_EQ(device_restore::Kept {}.Total(), 0u);
}

TEST(DeviceRestore, ARecoveryWithAWorkingDeviceContinues)
{
	device_restore::RecoveryWatch watch;
	for (uint32_t ms = 0; ms < 100'000; ms += 16)
	{
		EXPECT_EQ(watch.Step(false, 0, ms), device_restore::Recovery::Continues);
	}
}

TEST(DeviceRestore, AShortWaitForANewDeviceContinues)
{
	device_restore::RecoveryWatch watch;
	EXPECT_EQ(watch.Step(true, 0, 1000), device_restore::Recovery::Continues);
	EXPECT_EQ(watch.Step(true, 0, 1000 + device_restore::k_LongestWaitMs - 1), device_restore::Recovery::Continues);
	// The new device comes and the wait starts again at the next loss
	EXPECT_EQ(watch.Step(false, 1, 12'000), device_restore::Recovery::Continues);
	EXPECT_EQ(watch.Step(true, 1, 13'000), device_restore::Recovery::Continues);
	EXPECT_EQ(watch.Step(true, 1, 13'000 + device_restore::k_LongestWaitMs - 1), device_restore::Recovery::Continues);
}

TEST(DeviceRestore, ALongWaitForANewDeviceFails)
{
	device_restore::RecoveryWatch watch;
	EXPECT_EQ(watch.Step(true, 0, 500), device_restore::Recovery::Continues);
	EXPECT_EQ(watch.Step(true, 0, 500 + device_restore::k_LongestWaitMs), device_restore::Recovery::Failed);
}

TEST(DeviceRestore, RestoresFarApartContinue)
{
	device_restore::RecoveryWatch watch;
	uint32_t restores = 0;
	for (uint32_t ms = 0; ms < 10 * device_restore::k_BurstMs; ms += device_restore::k_BurstMs / 2)
	{
		EXPECT_EQ(watch.Step(false, ++restores, ms), device_restore::Recovery::Continues);
	}
}

TEST(DeviceRestore, ABurstOfRestoresFails)
{
	// Each new device is lost again half a second after the restore
	device_restore::RecoveryWatch watch;
	EXPECT_EQ(watch.Step(false, 1, 1000), device_restore::Recovery::Continues);
	EXPECT_EQ(watch.Step(false, 2, 1500), device_restore::Recovery::Continues);
	EXPECT_EQ(watch.Step(false, 3, 2000), device_restore::Recovery::Failed);
}

TEST(DeviceRestore, ManyRestoresBetweenTwoFramesFail)
{
	device_restore::RecoveryWatch watch;
	EXPECT_EQ(watch.Step(false, 0, 0), device_restore::Recovery::Continues);
	EXPECT_EQ(watch.Step(false, device_restore::k_RestoresInBurst, 100), device_restore::Recovery::Failed);
}
