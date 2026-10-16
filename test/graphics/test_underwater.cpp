/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the view under the sea and of the sea bed

#include <cstdint>

#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/Seabed.h"
#include "Graphics/Underwater.h"

namespace underwater = openblack::graphics::underwater;
namespace seabed = openblack::seabed;

TEST(Underwater, TheMoonIsBrightestWhenFull)
{
	EXPECT_NEAR(underwater::PhaseBrightness(0.0f), 0.0f, 1e-6f);
	EXPECT_NEAR(underwater::PhaseBrightness(std::numbers::pi_v<float>), 1.0f, 1e-6f);
	EXPECT_NEAR(underwater::PhaseBrightness(std::numbers::pi_v<float> * 0.5f), 0.5f, 1e-5f);
}

TEST(Underwater, LightBendsTowardsStraightDownInTheWater)
{
	// A low sun, 10 degrees over the horizon, comes in under the water at no more than the critical angle
	const glm::vec3 low {0.985f, 0.174f, 0.0f};
	const auto bent = underwater::RefractIntoWater(low);
	EXPECT_NEAR(glm::length(bent), 1.0f, 1e-5f);
	EXPECT_GT(bent.y, 0.66f);
	EXPECT_GT(bent.x, 0.0f);
	// A light straight up stays straight up
	const auto up = underwater::RefractIntoWater({0.0f, 1.0f, 0.0f});
	EXPECT_NEAR(up.y, 1.0f, 1e-6f);
}

TEST(Underwater, SunAndMoonBlendByStrength)
{
	const underwater::SkyLight sun {.towards = {1.0f, 1.0f, 0.0f}, .colour = {1.0f, 0.9f, 0.8f}, .strength = 1.0f};
	const underwater::SkyLight moon {.towards = {-1.0f, 1.0f, 0.0f}, .colour = {0.6f, 0.7f, 1.0f}, .strength = 1.0f};
	// The day: the sun alone
	const auto day = underwater::BlendLights(sun, std::nullopt);
	EXPECT_FLOAT_EQ(day.moonShare, 0.0f);
	EXPECT_GT(day.towards.x, 0.0f);
	// The night: the moon alone, dimmer
	const auto night = underwater::BlendLights(std::nullopt, moon);
	EXPECT_FLOAT_EQ(night.moonShare, 1.0f);
	EXPECT_LT(night.towards.x, 0.0f);
	EXPECT_LT(night.colour.b, 1.0f);
	// Dusk: both, the direction between them
	auto fading = sun;
	fading.strength = underwater::k_MoonLight;
	const auto dusk = underwater::BlendLights(fading, moon);
	EXPECT_NEAR(dusk.moonShare, 0.5f, 1e-5f);
	EXPECT_NEAR(dusk.towards.x, 0.0f, 1e-5f);
	EXPECT_NEAR(dusk.aboveTowards.x, 0.0f, 1e-5f);
	// No light: straight up and black
	const auto dark = underwater::BlendLights(std::nullopt, std::nullopt);
	EXPECT_FLOAT_EQ(glm::length(dark.colour), 0.0f);
}

TEST(Underwater, RedIsLostFirstThenGreen)
{
	const auto left = underwater::Transmittance(20.0f);
	EXPECT_LT(left.r, left.g);
	EXPECT_LT(left.g, left.b);
	EXPECT_FLOAT_EQ(underwater::Transmittance(0.0f).r, 1.0f);
	EXPECT_FLOAT_EQ(underwater::Transmittance(-5.0f).b, 1.0f);
}

TEST(Underwater, AmbientIsDarkerAndBluerDeeper)
{
	const glm::vec3 white {1.0f};
	const auto shallow = underwater::AmbientAtDepth(white, 2.0f);
	const auto deep = underwater::AmbientAtDepth(white, 30.0f);
	EXPECT_LT(deep.r, shallow.r);
	EXPECT_LT(deep.b, shallow.b);
	EXPECT_LT(deep.r / deep.b, shallow.r / shallow.b);
}

TEST(Underwater, EyeDepthAndActiveBand)
{
	EXPECT_FLOAT_EQ(underwater::EyeDepth(5.0f), 0.0f);
	EXPECT_FLOAT_EQ(underwater::EyeDepth(-3.0f), 3.0f);
	EXPECT_TRUE(underwater::IsActive(-10.0f));
	EXPECT_TRUE(underwater::IsActive(1.0f));
	EXPECT_FALSE(underwater::IsActive(20.0f));
}

TEST(Seabed, GoesDownFromTheCoast)
{
	EXPECT_FLOAT_EQ(seabed::DepthForDistance(0.0f), 0.0f);
	EXPECT_LT(seabed::DepthForDistance(1.0f), seabed::DepthForDistance(5.0f));
	EXPECT_LT(seabed::DepthForDistance(1000.0f), seabed::k_MaxDepth);
	// A shelf first: the depth grows quickly near the coast, then slowly, then steeply again past the shelf's edge
	EXPECT_GT(seabed::DepthForDistance(5.0f) - seabed::DepthForDistance(1.0f),
	          seabed::DepthForDistance(25.0f) - seabed::DepthForDistance(21.0f));
	EXPECT_GT(seabed::DepthForDistance(75.0f) - seabed::DepthForDistance(71.0f),
	          seabed::DepthForDistance(25.0f) - seabed::DepthForDistance(21.0f));
}

TEST(Seabed, LandStaysAndTheSeaGoesDown)
{
	// A strip of land along x = 0, and sea beyond it
	constexpr glm::ivec2 k_Size {12, 4};
	std::vector<uint8_t> altitudes(static_cast<size_t>(k_Size.x * k_Size.y), 0);
	for (int z = 0; z < k_Size.y; ++z)
	{
		altitudes[static_cast<size_t>(z * k_Size.x)] = 20;
	}
	const auto bed = seabed::Build(altitudes, k_Size, {0.0f, 0.0f}, 10.0f);
	ASSERT_EQ(bed.depth.size(), altitudes.size());
	EXPECT_FLOAT_EQ(bed.depth[0], 0.0f);
	EXPECT_GT(bed.depth[1], 0.0f);
	EXPECT_GT(bed.depth[11], bed.depth[2]);
	// Between corners the depth is blended; outside the grid it is the nearest edge's
	EXPECT_NEAR(bed.DepthAt({5.0f, 0.0f}), (bed.depth[0] + bed.depth[1]) * 0.5f, 1e-4f);
	EXPECT_FLOAT_EQ(bed.DepthAt({-100.0f, 0.0f}), bed.depth[0]);
	// The floor is the land where the land is above the sea, else the sea bed
	EXPECT_FLOAT_EQ(seabed::FloorHeight(bed, {0.0f, 0.0f}, 13.4f), 13.4f);
	EXPECT_FLOAT_EQ(seabed::FloorHeight(bed, {100.0f, 0.0f}, 0.0f), -bed.DepthAt({100.0f, 0.0f}));
}

TEST(Seabed, EmptyWithoutAltitudes)
{
	const auto bed = seabed::Build({}, {4, 4}, {0.0f, 0.0f}, 10.0f);
	EXPECT_EQ(bed.size, glm::ivec2(0));
	EXPECT_FLOAT_EQ(bed.DepthAt({1.0f, 1.0f}), 0.0f);
}

TEST(Underwater, TheTourGoesUnderAndComesBack)
{
	const auto start = underwater::TourAt(0.0f);
	const auto end = underwater::TourAt(underwater::k_TourSeconds);
	EXPECT_GT(start.eye.y, 0.0f);
	EXPECT_GT(end.eye.y, 0.0f);
	EXPECT_LT(underwater::TourAt(12.0f).eye.y, 0.0f);
	// Before the start and after the end it stays at its ends, and it moves smoothly between them
	EXPECT_EQ(underwater::TourAt(-1.0f).eye, start.eye);
	EXPECT_EQ(underwater::TourAt(100.0f).eye, end.eye);
	for (float t = 0.0f; t < underwater::k_TourSeconds; t += 0.05f)
	{
		EXPECT_LT(glm::length(underwater::TourAt(t + 0.05f).eye - underwater::TourAt(t).eye), 1.5f);
	}
}

TEST(Seabed, IsDeeperTowardsTheMapsEdge)
{
	// Land in the middle of a wide sea: the sea bed near the edge is deeper than on the shelf near the land
	constexpr glm::ivec2 k_Size {201, 201};
	std::vector<uint8_t> altitudes(static_cast<size_t>(k_Size.x * k_Size.y), 0);
	for (int z = 95; z <= 105; ++z)
	{
		for (int x = 95; x <= 105; ++x)
		{
			altitudes[static_cast<size_t>(z * k_Size.x + x)] = 30;
		}
	}
	const auto bed = seabed::Build(altitudes, k_Size, {0.0f, 0.0f}, 10.0f);
	const auto depth = [&bed](int x, int z) { return bed.depth[static_cast<size_t>(z * bed.size.x + x)]; };
	EXPECT_FLOAT_EQ(depth(100, 100), 0.0f);
	EXPECT_LT(depth(110, 100), depth(150, 100));
	EXPECT_LT(depth(150, 100), depth(200, 100));
	EXPECT_LE(depth(200, 100), seabed::k_MaxDepth);
}
