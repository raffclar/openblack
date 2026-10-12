/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <utility>
#include <vector>

#include <3D/StormClouds.h>
#include <gtest/gtest.h>

namespace
{
namespace storm_clouds = openblack::storm_clouds;
using Milliseconds = std::chrono::duration<float, std::milli>;

/// Hands out the given numbers in turn, recording the ranges asked for
struct FakeRandom
{
	std::vector<float> values;
	std::vector<std::pair<float, float>> asked;
	size_t next {0};

	storm_clouds::Random Bind()
	{
		return [this](float low, float high) {
			asked.emplace_back(low, high);
			return values.at(next++);
		};
	}
};
} // namespace

TEST(StormClouds, AStormGathersAtMostSixteen)
{
	EXPECT_EQ(storm_clouds::Wanted(150), 16);
	EXPECT_EQ(storm_clouds::Wanted(8), 8);
	EXPECT_EQ(storm_clouds::Wanted(0), 0);
	EXPECT_EQ(storm_clouds::Wanted(-3), 0);
}

TEST(StormClouds, ANewCloudDrawsItsShapeThenItsPlace)
{
	FakeRandom random {.values = {3.0f, 0.012f, -0.5f, 4.0f, 0.25f}, .asked = {}};
	const auto cloud = storm_clouds::MakeCloud(random.Bind(), 250.0f);
	const std::vector<std::pair<float, float>> expected {
	    {2.5f, 5.0f}, {0.01f, 0.015f}, {-1.0f, 1.0f}, {-10.0f, 10.0f}, {-1.0f, 1.0f}};
	EXPECT_EQ(random.asked, expected);
	EXPECT_FLOAT_EQ(cloud.edgeShrink, 3.0f);
	EXPECT_FLOAT_EQ(cloud.sizeFactor, 0.024f);
	EXPECT_FLOAT_EQ(cloud.drift.place.x, -0.5f);
	EXPECT_FLOAT_EQ(cloud.drift.place.y, 254.0f);
	EXPECT_FLOAT_EQ(cloud.drift.place.z, 0.25f);
	EXPECT_EQ(cloud.drift.stepsLeft, 0);
}

TEST(StormClouds, ACloudPicksItsNextPlaceThenTakesFourHundredStepsToIt)
{
	FakeRandom random {.values = {1.0f, 10.0f, -1.0f}, .asked = {}};
	storm_clouds::Drift drift {.place = {0.0f, 100.0f, 0.0f}, .target = {}, .step = {}, .stepsLeft = 0};
	storm_clouds::Step(drift, random.Bind(), 100.0f);
	// It stays put as it picks
	EXPECT_EQ(drift.place, glm::vec3(0.0f, 100.0f, 0.0f));
	EXPECT_EQ(drift.target, glm::vec3(1.0f, 110.0f, -1.0f));
	EXPECT_EQ(drift.stepsLeft, storm_clouds::k_StepsPerLeg);
	const std::vector<std::pair<float, float>> expected {{-1.0f, 1.0f}, {0.0f, 20.0f}, {-1.0f, 1.0f}};
	EXPECT_EQ(random.asked, expected);
	for (int i = 0; i < storm_clouds::k_StepsPerLeg; ++i)
	{
		storm_clouds::Step(drift, random.Bind(), 100.0f);
	}
	EXPECT_NEAR(drift.place.x, 1.0f, 1e-4f);
	EXPECT_NEAR(drift.place.y, 110.0f, 1e-3f);
	EXPECT_NEAR(drift.place.z, -1.0f, 1e-4f);
	EXPECT_EQ(drift.stepsLeft, 0);
	// No more numbers were drawn on the way
	EXPECT_EQ(random.asked.size(), 3u);
}

TEST(StormClouds, CloudsSpreadOverHalfTheStormsReachAndGrowWithIt)
{
	const storm_clouds::Drift drift {.place = {1.0f, 260.0f, -0.5f}, .target = {}, .step = {}, .stepsLeft = 3};
	// The guide's storm: inner 150, outer 500, at full strength
	const auto place = storm_clouds::Place(drift, 0.02f, {2000.0f, 0.0f, 3000.0f}, 150.0f, 500.0f);
	EXPECT_FLOAT_EQ(place.xz.x, 2325.0f);
	EXPECT_FLOAT_EQ(place.xz.y, 2837.5f);
	EXPECT_FLOAT_EQ(place.heightAboveGround, 260.0f);
	EXPECT_FLOAT_EQ(place.size, 13.0f);
}

TEST(StormClouds, AStormsCloudsAreThreeQuartersOpaqueAtFullStrength)
{
	EXPECT_EQ(storm_clouds::Alpha(1.0f), 191);
	EXPECT_EQ(storm_clouds::Alpha(0.5f), 95);
	EXPECT_EQ(storm_clouds::Alpha(0.0f), 0);
	// Faint ones are not drawn
	EXPECT_LE(storm_clouds::Alpha(0.03f), storm_clouds::k_LeastShownAlpha);
}

TEST(StormClouds, TheShadeDarkensTheLandsColour)
{
	EXPECT_EQ(storm_clouds::Tint(0xFF80C0FFu, 0.0f), 0x80C0FFu);
	// Half the shade off each channel, cut towards nought
	EXPECT_EQ(storm_clouds::Tint(0x80C0FFu, 0.5f), 0x6090BFu);
	// The guide's storm has a shade of 1.5, which leaves a quarter
	EXPECT_EQ(storm_clouds::Tint(0x80C0FFu, 1.5f), 0x20303Fu);
}

TEST(StormClouds, LightningLightsTheCloudsGrey)
{
	EXPECT_EQ(storm_clouds::FlashGrey(0.0f), 0);
	EXPECT_EQ(storm_clouds::FlashGrey(1.0f), 127);
	EXPECT_EQ(storm_clouds::FlashGrey(3.0f), 255);
}

TEST(StormClouds, ADarkOrStrongStormShadesTheLand)
{
	// The guide's storm, shade 1.5, shades fully at full strength
	EXPECT_EQ(storm_clouds::ShadowAlpha(1.5f, 1.0f), 255);
	// The default shade of 0.5 at full strength makes 1.2, held at full
	EXPECT_EQ(storm_clouds::ShadowAlpha(0.5f, 1.0f), 255);
	EXPECT_EQ(storm_clouds::ShadowAlpha(0.5f, 0.5f), 153);
	EXPECT_FALSE(storm_clouds::ShadowAlpha(0.5f, 0.0f).has_value());
}

TEST(StormClouds, TheShadowIsCentredOnTheStorm)
{
	const auto corner = storm_clouds::ShadowCorner({1000.0f, 2000.0f});
	EXPECT_FLOAT_EQ(corner.x, 805.0f);
	EXPECT_FLOAT_EQ(corner.y, 1805.0f);
}

TEST(StormClouds, TheyStepAtTheScreensRefreshWhateverTheFrameRate)
{
	Milliseconds since {0.0f};
	int steps = 0;
	// A second at 600 frames a second
	for (int i = 0; i < 600; ++i)
	{
		steps += storm_clouds::StepDue(since, Milliseconds(1000.0f / 600.0f)) ? 1 : 0;
	}
	EXPECT_NEAR(steps, 60, 1);
	// Slow frames step once each
	since = Milliseconds(0.0f);
	steps = 0;
	for (int i = 0; i < 20; ++i)
	{
		steps += storm_clouds::StepDue(since, Milliseconds(50.0f)) ? 1 : 0;
	}
	EXPECT_EQ(steps, 20);
}
