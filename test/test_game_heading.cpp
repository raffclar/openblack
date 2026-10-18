/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <numbers>

#include <gtest/gtest.h>

#include "Common/GameHeading.h"

using openblack::GameHeading;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}
} // namespace

TEST(GameHeading, ZeroFacesMinusZAndAQuarterTurnFacesPlusX)
{
	EXPECT_EQ(GameHeading::Of(0.0f, -1.0f).Radians(), 0.0f);
	EXPECT_EQ(Bits(GameHeading::Of(1.0f, 0.0f).Radians()), Bits(std::numbers::pi_v<float> / 2.0f));
	EXPECT_EQ(Bits(GameHeading::Of(-1.0f, 0.0f).Radians()), Bits(-std::numbers::pi_v<float> / 2.0f));
	// A direction too short to turn to has no heading
	EXPECT_EQ(GameHeading::Of(0.0005f, 0.0005f).Radians(), 0.0f);
}

TEST(GameHeading, TheRendererTurnsTheOtherWay)
{
	EXPECT_EQ(GameHeading::FromYaw(0.5f).Radians(), -0.5f);
	EXPECT_EQ(GameHeading(0.5f).Yaw(), -0.5f);
	EXPECT_EQ(GameHeading::FromYaw(1.25f).Yaw(), 1.25f);
}

TEST(GameHeading, WrapMovesOneFullTurnAtMost)
{
	constexpr float k_Pi = std::numbers::pi_v<float>;
	EXPECT_EQ(GameHeading::Wrap(k_Pi), k_Pi);
	EXPECT_EQ(GameHeading::Wrap(-k_Pi), -k_Pi);
	EXPECT_EQ(Bits(GameHeading::Wrap(4.0f)), Bits(4.0f - 2.0f * k_Pi));
	EXPECT_EQ(Bits(GameHeading::Wrap(-4.0f)), Bits(-4.0f + 2.0f * k_Pi));
	// Three half turns or more stay out of the range after the one turn
	EXPECT_GT(GameHeading::Wrap(10.0f), k_Pi);
}

TEST(GameHeading, TurnFromIsThisHeadingLessTheOtherWrapped)
{
	EXPECT_EQ(Bits(GameHeading(3.0f).TurnFrom(GameHeading(-3.0f))), Bits(GameHeading::Wrap(6.0f)));
	EXPECT_EQ(GameHeading(0.25f).TurnFrom(GameHeading(0.5f)), -0.25f);
}

TEST(GameHeading, GameAngleIsAQuarterTurnOnFromTheHeading)
{
	// Facing -z is game angle 0x600 and facing +x is game angle 0
	EXPECT_EQ(GameHeading(0.0f).GameAngle(), 0x600);
	EXPECT_EQ(GameHeading(std::numbers::pi_v<float> / 2.0f).GameAngle(), 0);
}
