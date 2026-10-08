/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// sky_type:: (src/3D/SkyType.h) against the original's rules: the sky type of an hour, the visual time cycle, the
// frame sampling, the dome blend, IsVisualNight, and the column and haze factor

#include <cmath>

#include <limits>

#include <gtest/gtest.h>

#include "3D/DayNightClock.h"
#include "3D/SkyType.h"

using namespace openblack;

namespace
{
/// The tests set the sky thresholds (directly or through a DayNightClock): each test gives back the ones it found
class SkyType: public ::testing::Test
{
protected:
	void SetUp() override { _thresholds = sky_type::GetThresholds(); }
	void TearDown() override
	{
		sky_type::SetThresholds(_thresholds.at(0), _thresholds.at(1), _thresholds.at(2), _thresholds.at(3));
	}

private:
	sky_type::Thresholds _thresholds {};
};
} // namespace

TEST_F(SkyType, DefaultCycleThresholds)
{
	// 1700 / 0.083 / 0.07: N = 0.996, E = 1.836, c = 0.21
	DayNightClock clock;
	clock.SetCycle(DayNightClock::k_DefaultDuration, DayNightClock::k_DefaultNight, DayNightClock::k_DefaultChange);
	const auto& t = sky_type::GetThresholds();
	EXPECT_NEAR(t[0], 0.786f, 1e-5f);
	EXPECT_NEAR(t[1], 1.206f, 1e-5f);
	EXPECT_NEAR(t[2], 1.626f, 1e-5f);
	EXPECT_NEAR(t[3], 2.046f, 1e-5f);
	EXPECT_EQ(t, clock.GetVisualTimes());
}

TEST_F(SkyType, At)
{
	sky_type::SetThresholds(0.786f, 1.206f, 1.626f, 2.046f);
	EXPECT_EQ(sky_type::At(12.0f), 0.0f);
	EXPECT_EQ(sky_type::At(0.0f), 2.0f);
	EXPECT_EQ(sky_type::At(23.9f), 2.0f); // folds to 0.1
	EXPECT_EQ(sky_type::At(1.5f), 1.0f);
	EXPECT_FLOAT_EQ(sky_type::At(1.0f), 2.0f - (1.0f - 0.786f) / (1.206f - 0.786f));
	EXPECT_FLOAT_EQ(sky_type::At(1.8f), 1.0f - (1.8f - 1.626f) / (2.046f - 1.626f));

	// Demon God (1143, 1, 0): all four at 12, no division by zero; 12 itself does not fold and is day
	sky_type::SetThresholds(12.0f, 12.0f, 12.0f, 12.0f);
	EXPECT_EQ(sky_type::At(5.0f), 2.0f);
	EXPECT_EQ(sky_type::At(12.5f), 2.0f);
	EXPECT_EQ(sky_type::At(12.0f), 0.0f);
}

TEST_F(SkyType, SampleFrameWraps)
{
	sky_type::SetThresholds(0.786f, 1.206f, 1.626f, 2.046f);
	sky_type::SampleFrame(-1.0f);
	EXPECT_EQ(sky_type::FrameHour(), 23.0f);
	EXPECT_EQ(sky_type::Frame(), sky_type::At(1.0f)); // folded at 12
	sky_type::SampleFrame(48.5f);
	EXPECT_EQ(sky_type::FrameHour(), 0.5f);
	sky_type::SampleFrame(24.0f);
	EXPECT_EQ(sky_type::FrameHour(), 0.0f);
	sky_type::SampleFrame(12.0f);
	EXPECT_EQ(sky_type::Frame(), 0.0f);
	// at 24 bits: -1e-7 + 24 rounds to 24, which wraps to 0 (not 24.0f)
	sky_type::SampleFrame(-1e-7f);
	EXPECT_EQ(sky_type::FrameHour(), 0.0f);
	EXPECT_EQ(sky_type::Frame(), 2.0f);
}

TEST_F(SkyType, AtNaNIsNight)
{
	// an unordered compare counts as "<"
	sky_type::SetThresholds(0.786f, 1.206f, 1.626f, 2.046f);
	EXPECT_EQ(sky_type::At(std::numeric_limits<float>::quiet_NaN()), 2.0f);
}

TEST_F(SkyType, IsVisualNightDouble)
{
	EXPECT_TRUE(sky_type::IsVisualNight(1.2f)); // 1.20000005 > the double 1.2
	EXPECT_FALSE(sky_type::IsVisualNight(1.1999999f));
	EXPECT_FALSE(sky_type::IsVisualNight(0.0f));
}

TEST_F(SkyType, EveningRamp)
{
	sky_type::SetThresholds(0.786f, 1.206f, 1.626f, 2.046f);
	EXPECT_EQ(sky_type::EveningRamp(23.0f, 1.0f, 0.0f), 1.0f);
	EXPECT_NEAR(sky_type::EveningRamp(21.5f, 1.0f, 0.0f), 1.0f - (2.5f - 2.046f), 1e-6f);
	EXPECT_EQ(sky_type::EveningRamp(20.0f, 1.0f, 0.0f), 0.0f);
}

TEST_F(SkyType, LightColumnAndHaze)
{
	EXPECT_EQ(sky_type::LightColumn(0.0f), 30.0f);
	EXPECT_EQ(sky_type::LightColumn(1.0f), 15.0f);
	EXPECT_EQ(sky_type::LightColumn(2.0f), 0.0f);
	EXPECT_EQ(sky_type::HazeFactor(1.0f), 1.0f);
	EXPECT_EQ(sky_type::HazeFactor(0.5f), 0.25f);
	EXPECT_EQ(sky_type::HazeFactor(1.5f), 0.25f);
	EXPECT_EQ(sky_type::HazeFactor(2.0f), 0.0f);
}

TEST_F(SkyType, DomeWeight)
{
	auto w = sky_type::DomeWeightOf(1.0f);
	EXPECT_EQ(w.lower, 0);
	EXPECT_EQ(w.upper, 1);
	EXPECT_EQ(w.weight, 255);
	w = sky_type::DomeWeightOf(2.0f);
	EXPECT_EQ(w.lower, 1);
	EXPECT_EQ(w.upper, 2);
	EXPECT_EQ(w.weight, 255);
	w = sky_type::DomeWeightOf(0.5f);
	EXPECT_EQ(w.weight, 127);
	w = sky_type::DomeWeightOf(0.0f);
	EXPECT_EQ(w.weight, 0);
}

TEST_F(SkyType, BlendTexel555)
{
	// the weights add up to 255 / 256: a full channel of 31 comes out 30
	EXPECT_EQ(sky_type::BlendTexel555(0x7FFF, 0x0000, 0), 0x7BDE);
	EXPECT_EQ(sky_type::BlendTexel555(0x0000, 0x7FFF, 255), 0x7BDE);
	EXPECT_EQ(sky_type::BlendTexel555(0x8000, 0x8000, 128), 0x0000); // bit 15 comes out 0
	// red 31 / green 0 / blue 10 against red 0 / green 31 / blue 20 at w 128: (31 127) >> 8 = 15, (31 128) >> 8 = 15,
	// (10 127) >> 8 + (20 128) >> 8 = 4 + 10
	EXPECT_EQ(sky_type::BlendTexel555((31 << 10) | 10, (31 << 5) | 20, 128), (15 << 10) | (15 << 5) | 14);
}

TEST_F(SkyType, DomeBlendHysteresisAndRows)
{
	sky_type::DomeBlend dome;
	EXPECT_EQ(dome.RowsDone(), sky_type::DomeBlend::k_Rows);
	EXPECT_EQ(dome.Advance(0.0f).count, 0);
	// (double)0.03f itself is not more than the threshold
	EXPECT_EQ(dome.Advance(0.03f).count, 0);
	auto blocks = dome.Advance(0.031f);
	ASSERT_EQ(blocks.count, 1);
	EXPECT_EQ(blocks.blocks[0].skyType, 0.031f);
	EXPECT_EQ(blocks.blocks[0].firstRow, 0);
	EXPECT_EQ(blocks.blocks[0].rowCount, 32);
	// the latched sky type goes on to the end whatever the frame says
	for (int row = 32; row < 256; row += 32)
	{
		blocks = dome.Advance(1.5f);
		ASSERT_EQ(blocks.count, 1);
		EXPECT_EQ(blocks.blocks[0].skyType, 0.031f);
		EXPECT_EQ(blocks.blocks[0].firstRow, row);
	}
	// whole again: now 1.5 is latched
	blocks = dome.Advance(1.5f);
	ASSERT_EQ(blocks.count, 1);
	EXPECT_EQ(blocks.blocks[0].skyType, 1.5f);
	EXPECT_EQ(blocks.blocks[0].firstRow, 0);

	// a jump: the whole dome at once, then the rows from 0 again with the same sky type
	dome.Jump(2.0f);
	blocks = dome.Advance(2.0f);
	ASSERT_EQ(blocks.count, 2);
	EXPECT_EQ(blocks.blocks[0].skyType, 2.0f);
	EXPECT_EQ(blocks.blocks[0].firstRow, 0);
	EXPECT_EQ(blocks.blocks[0].rowCount, 256);
	EXPECT_EQ(blocks.blocks[1].skyType, 2.0f);
	EXPECT_EQ(blocks.blocks[1].firstRow, 0);
	EXPECT_EQ(blocks.blocks[1].rowCount, 32);
	EXPECT_EQ(dome.Built(), 2.0f);
}

TEST_F(SkyType, DomeBlendHysteresisFloatDifference)
{
	// the difference is rounded to float (24-bit FPU) before the compare with (double)0.03f.
	// f - b is exactly 0.03f + 0.4 ulp (more than the threshold in double) but rounds to 0.03f, which is not more.
	const float f = std::nextafter(0.03f, 1.0f);
	const float b = (f - 0.03f) * 0.6f;
	ASSERT_GT(static_cast<double>(f) - static_cast<double>(b), static_cast<double>(0.03f));
	sky_type::DomeBlend dome;
	dome.Jump(b);
	for (int i = 0; i < 8; ++i)
	{
		(void)dome.Advance(b);
	}
	ASSERT_EQ(dome.RowsDone(), sky_type::DomeBlend::k_Rows);
	EXPECT_EQ(dome.Advance(f).count, 0);
}

TEST_F(SkyType, ForceScriptTimeJumps)
{
	DayNightClock clock;
	clock.Reset(); // noon
	EXPECT_EQ(sky_type::FrameHour(), 12.0f);
	EXPECT_EQ(sky_type::Frame(), 0.0f);
	clock.SetScriptTime(0.0f);
	EXPECT_EQ(sky_type::Frame(), 2.0f);
	EXPECT_EQ(sky_type::Dome().Built(), 2.0f);
	EXPECT_EQ(sky_type::Dome().RowsDone(), 0);
	// MOVE_GAME_TIME does not jump
	clock.MoveScriptTime(12.0f, 10.0f);
	EXPECT_EQ(sky_type::Frame(), 2.0f);
}
