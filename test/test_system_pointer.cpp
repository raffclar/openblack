/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Windowing/SystemPointer.h"

using namespace openblack::windowing;

TEST(SystemPointer, ABackgroundRunNeverTouchesThePointerOfTheSystem)
{
	EXPECT_FALSE(system_pointer::MayTouch(true));
	EXPECT_TRUE(system_pointer::MayTouch(false));
	// In a background run each call returns before it reaches the system: the test has no window and no video
	// system, so a call that reached the system would fail or crash
	EXPECT_FALSE(system_pointer::SetRelative(true, true));
	EXPECT_FALSE(system_pointer::SetRelative(false, true));
	system_pointer::Warp(nullptr, {10, 10}, true);
	system_pointer::Show(false, true);
	system_pointer::Grab(nullptr, true, true);
}
