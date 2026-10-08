/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Camera/CameraHelp.h"
#include "Help/InterfaceInteraction.h"
#include "Input/InterfaceActive.h"

// SET_INTERFACE_INTERACTION's 16 levels against the table read from the original game.

using namespace openblack;
namespace ii = openblack::help::interface_interaction;

namespace
{
struct Expected
{
	int32_t level;
	uint8_t interfaceAfterFrom1; ///< the interface state after the call when it was 0x01 (inactive) before
	int32_t features;
	std::optional<float> reach; ///< the R handed to the hand (HandSystemInterface::SetHandReach); none: untouched
	bool autoPitch;             ///< bit 0x40 after the call
	bool cameraMovesAllowed;
	bool realmZoomsAllowed;
};

const std::array<Expected, 15> k_Levels {{
    {0, 0x00, 0x1BF, 1800.0f, false, true, true},
    {1, 0x07, 0x48, 75.0f, true, true, false},
    {2, 0x07, 0x48, 1800.0f, true, true, false},
    {3, 0x07, 0x02, 1800.0f, false, true, false},
    {4, 0x07, 0x18, 1800.0f, false, true, false},
    {5, 0x07, 0x24, 1800.0f, false, true, false},
    {6, 0x00, 0x02, 1800.0f, false, true, false},
    {7, 0x00, 0x26, 1800.0f, false, true, false},
    {8, 0x07, 0x00, std::nullopt, false, false, false},
    {10, 0x07, 0x4A, 1800.0f, true, true, false},
    {11, 0x07, 0x01, 1800.0f, false, true, false},
    {12, 0x05, 0x00, std::nullopt, false, false, false},
    {13, 0x07, 0x1A, 1800.0f, false, true, false},
    {14, 0x07, 0x1B, 1800.0f, false, true, false},
    {15, 0x07, 0x3F, 1800.0f, false, true, false},
}};

class InterfaceInteractionTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		ii::Reset();
		camera_help::Reset();
		interface_active::SetFlags(0);
	}

	// the level, the camera features and the interface flags a test set do not reach the next test
	void TearDown() override
	{
		ii::Reset();
		camera_help::Reset();
		interface_active::SetFlags(0);
	}
};
} // namespace

TEST_F(InterfaceInteractionTest, EveryLevelAsTheJumpTable)
{
	for (const auto& e : k_Levels)
	{
		ii::Reset();
		camera_help::Reset();
		camera_help::EnableCameraFeatures(0x40, 0); // an auto-pitch already on: the -1 mask clears it
		interface_active::SetFlags(0x01);
		ii::Set(e.level); // no hand system here: Set only asks LevelHandReach
		SCOPED_TRACE(e.level);
		EXPECT_EQ(ii::GetLevel(), e.level);
		EXPECT_EQ(interface_active::GetFlags(), e.interfaceAfterFrom1);
		EXPECT_EQ(camera_help::GetEnabledFeatures(), e.features);
		EXPECT_EQ(ii::LevelHandReach(e.level), e.reach);
		EXPECT_EQ(camera_help::IsFeatureEnabled(camera_help::Feature::AutoPitch), e.autoPitch);
		EXPECT_EQ(ii::CameraMovesAllowed(), e.cameraMovesAllowed);
		EXPECT_EQ(ii::RealmZoomsAllowed(), e.realmZoomsAllowed);
	}
}

TEST_F(InterfaceInteractionTest, AutoPitchParameters)
{
	ii::Set(1);
	EXPECT_FLOAT_EQ(camera_help::GetAutoPitchAngle(), 0.448799f);
	EXPECT_FLOAT_EQ(camera_help::GetAutoPitchDistance(), 15.0f);
	ii::Set(3); // the parameters stay, the bit goes with the -1 mask
	EXPECT_FLOAT_EQ(camera_help::GetAutoPitchAngle(), 0.448799f);
	EXPECT_FALSE(camera_help::IsFeatureEnabled(camera_help::Feature::AutoPitch));
}

TEST_F(InterfaceInteractionTest, InvalidLevelsOnlyStoreTheLevel)
{
	ii::Set(4);
	for (const int32_t level : {9, 16, -1})
	{
		ii::Set(level);
		EXPECT_EQ(ii::GetLevel(), level);
		EXPECT_EQ(interface_active::GetFlags(), 0x06);
		EXPECT_EQ(camera_help::GetEnabledFeatures(), 0x18);
		EXPECT_TRUE(ii::CameraMovesAllowed());
		EXPECT_FALSE(ii::RealmZoomsAllowed());
	}
}

TEST_F(InterfaceInteractionTest, InvalidLevelsLeaveTheReach)
{
	for (const int32_t level : {9, 16, -1, 0x7FFFFFFF})
	{
		EXPECT_FALSE(ii::LevelHandReach(level).has_value()) << level;
	}
}

TEST_F(InterfaceInteractionTest, ControlMapActionGate)
{
	ii::Set(0);
	EXPECT_TRUE(ii::KeyShortcutsEnabled());
	for (int32_t action = 0; action < 33; ++action)
	{
		EXPECT_FALSE(ii::IsActionBlocked(action));
	}
	ii::Set(13); // realm zooms not allowed: 17..19
	EXPECT_FALSE(ii::KeyShortcutsEnabled());
	for (int32_t action = 0; action < 33; ++action)
	{
		EXPECT_EQ(ii::IsActionBlocked(action), action >= 17 && action <= 19) << action;
	}
	ii::Set(8); // camera moves not allowed: 3..19 but 5
	for (int32_t action = 0; action < 33; ++action)
	{
		EXPECT_EQ(ii::IsActionBlocked(action), action >= 3 && action <= 19 && action != 5) << action;
	}
}

TEST_F(InterfaceInteractionTest, EnableCameraFeaturesMask)
{
	camera_help::EnableCameraFeatures(0x08, -1);
	EXPECT_EQ(camera_help::GetEnabledFeatures(), 0x08);
	camera_help::SetAutoPitch(1.0f, 2.0f, true);
	EXPECT_EQ(camera_help::GetEnabledFeatures(), 0x48);
	camera_help::SetAutoPitch(1.0f, 2.0f, false);
	EXPECT_EQ(camera_help::GetEnabledFeatures(), 0x08);
	EXPECT_FLOAT_EQ(camera_help::GetAutoPitchDistance(), 2.0f);
}
