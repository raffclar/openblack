/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the view under the sea and of the sea bed

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <numbers>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/Seabed.h"
#include "3D/SeabedTiles.h"
#include "Enums.h"
#include "Graphics/Underwater.h"

namespace underwater = openblack::graphics::underwater;
namespace seabed = openblack::seabed;
namespace lnd = openblack::lnd;
using openblack::TerrainMaterialType;

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

TEST(Underwater, TheFullMoonsHighTideIsTheUsualHeight)
{
	constexpr float k_Full = std::numbers::pi_v<float>;
	// High tide at midnight and midday, at a full moon exactly at the usual height
	EXPECT_NEAR(underwater::TideLevel(0.0f, k_Full), 0.0f, 1e-5f);
	EXPECT_NEAR(underwater::TideLevel(12.0f, k_Full), 0.0f, 1e-5f);
	// The lowest low tide, at six, at a full moon
	EXPECT_NEAR(underwater::TideLevel(6.0f, k_Full), -underwater::k_LowestTide, 1e-4f);
	// Never above the usual height, at any hour or phase
	for (float phase = 0.0f; phase < 6.3f; phase += 0.1f)
	{
		for (float hour = 0.0f; hour < 24.0f; hour += 0.25f)
		{
			EXPECT_LE(underwater::TideLevel(hour, phase), 1e-5f);
			EXPECT_GE(underwater::TideLevel(hour, phase), -underwater::k_LowestTide - 1e-4f);
		}
	}
	// Away from a full moon the high tide is lower, and the low tide at a quarter moon is not as low
	constexpr float k_Quarter = std::numbers::pi_v<float> * 0.5f;
	EXPECT_LT(underwater::TideLevel(0.0f, k_Quarter), -0.1f);
	EXPECT_GT(underwater::TideLevel(6.0f, k_Quarter), -underwater::k_LowestTide + 0.5f);
}

TEST(Underwater, AStormMakesTheWavesHigher)
{
	const auto calm = underwater::WavesFor(0.0f, 0.0f);
	const auto storm = underwater::WavesFor(1.0f, 0.0f);
	EXPECT_GT(storm.scale, calm.scale * 3.0f);
	EXPECT_GT(storm.swash, calm.swash);
	float calmHighest = 0.0f;
	float stormHighest = 0.0f;
	for (float x = 0.0f; x < 200.0f; x += 1.0f)
	{
		calmHighest = std::max(calmHighest, std::abs(underwater::WavesAt({x, 0.0f}, 3.0f, calm).height));
		stormHighest = std::max(stormHighest, std::abs(underwater::WavesAt({x, 0.0f}, 3.0f, storm).height));
	}
	EXPECT_GT(stormHighest, calmHighest * 2.0f);
	EXPECT_LT(calmHighest, 1.0f);
	// The slope agrees with the height's change along x
	const auto here = underwater::WavesAt({10.0f, 5.0f}, 2.0f, calm);
	const auto next = underwater::WavesAt({10.01f, 5.0f}, 2.0f, calm);
	EXPECT_NEAR((next.height - here.height) / 0.01f, here.slope.x, 0.02f);
}

TEST(Underwater, StorminessAndEasing)
{
	EXPECT_FLOAT_EQ(underwater::Storminess(0.0f, 0.0f, {0.0f, 0.0f}), 0.0f);
	EXPECT_FLOAT_EQ(underwater::Storminess(100.0f, 100.0f, {100.0f, 0.0f}), 1.0f);
	EXPECT_FLOAT_EQ(underwater::EaseTowards(0.0f, 1.0f, 0.5f, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(underwater::EaseTowards(0.0f, 1.0f, 0.5f, 4.0f), 1.0f);
	EXPECT_FLOAT_EQ(underwater::EaseTowards(1.0f, 0.0f, 0.25f, 1.0f), 0.75f);
}

TEST(Seabed, GentleCoastsGoOnAsLongBeaches)
{
	// Two coasts on one sea: low land on the left (gentle), a high cliff on the right
	constexpr glm::ivec2 k_Size {61, 3};
	std::vector<uint8_t> altitudes(static_cast<size_t>(k_Size.x * k_Size.y), 0);
	for (int z = 0; z < k_Size.y; ++z)
	{
		for (int x = 0; x < 5; ++x)
		{
			altitudes[static_cast<size_t>(z * k_Size.x + x)] = 4;
		}
		for (int x = 56; x < k_Size.x; ++x)
		{
			altitudes[static_cast<size_t>(z * k_Size.x + x)] = 120;
		}
	}
	const auto bed = seabed::Build(altitudes, k_Size, {0.0f, 0.0f}, 10.0f);
	const auto depth = [&bed](int x) { return bed.depth[static_cast<size_t>(bed.size.x + x)]; };
	// Four cells out from the beach the water is still shallow, and it is under the lowest tide by nine cells
	EXPECT_LT(depth(4 + 4), underwater::k_LowestTide);
	EXPECT_GT(depth(4 + 9), underwater::k_LowestTide - 0.5f);
	// Five cells out from the cliff it is much deeper
	EXPECT_GT(depth(56 - 5), depth(4 + 4) * 2.0f);
}

TEST(Underwater, TheCurrentsSwirlsHaveNoSinks)
{
	// With no steady flow, no gust and no waves, what flows into a small square flows out of it
	const underwater::CurrentState current {.flow = {0.0f, 0.0f}, .swirl = 1.0f, .gust = 0.0f};
	auto still = underwater::WavesFor(0.0f, 0.0f);
	still.scale = 0.0f;
	for (float x = 0.0f; x < 400.0f; x += 37.0f)
	{
		const float h = 0.05f;
		const auto flow = [&](float px, float pz) {
			return underwater::CurrentAt({px, -20.0f, pz}, 5.0f, current, still, 0.0f);
		};
		const float divergence =
		    (flow(x + h, 10.0f).x - flow(x - h, 10.0f).x + flow(x, 10.0f + h).y - flow(x, 10.0f - h).y) / (2.0f * h);
		EXPECT_NEAR(divergence, 0.0f, 1e-3f);
	}
	// A storm makes the current stronger, and a rising tide adds a steady flow
	const auto calm = underwater::CurrentFor(0.0f, 0.0f, 0.0f);
	const auto storm = underwater::CurrentFor(0.0f, 0.0f, 1.0f);
	EXPECT_GT(storm.swirl, calm.swirl);
	EXPECT_GT(storm.gust, calm.gust);
	EXPECT_GT(glm::length(underwater::CurrentFor(0.01f, 0.0f, 0.0f).flow - calm.flow), 0.0f);
}

TEST(Seabed, TheReefStaysUnderTheLowestTide)
{
	// A wide sea round a small island: every coral and plant is deep enough and not too deep
	constexpr glm::ivec2 k_Size {161, 161};
	std::vector<uint8_t> altitudes(static_cast<size_t>(k_Size.x * k_Size.y), 0);
	for (int z = 75; z <= 85; ++z)
	{
		for (int x = 75; x <= 85; ++x)
		{
			altitudes[static_cast<size_t>(z * k_Size.x + x)] = 6;
		}
	}
	const auto bed = seabed::Build(altitudes, k_Size, {0.0f, 0.0f}, 10.0f);
	const auto reef = seabed::PlaceReef(bed, underwater::k_LowestTide, 500, 500);
	ASSERT_FALSE(reef.empty());
	for (const auto& plant : reef)
	{
		const float under = -plant.position.y - underwater::k_LowestTide;
		const bool coral = plant.kind == seabed::ReefPlant::Kind::Coral;
		EXPECT_GE(under, coral ? seabed::k_CoralTopUnderTide : seabed::k_PlantTopUnderTide);
		EXPECT_LE(under, coral ? seabed::k_CoralBottomUnderTide : seabed::k_PlantBottomUnderTide);
	}
	EXPECT_LE(seabed::PlaceReef(bed, underwater::k_LowestTide, 3, 4).size(), 7u);
}

TEST(Seabed, TheShoreMapPointsToTheCoastAndFindsTheCliffs)
{
	// Low land on the left, a cliff on the right, sea between
	constexpr glm::ivec2 k_Size {61, 9};
	std::vector<uint8_t> altitudes(static_cast<size_t>(k_Size.x * k_Size.y), 0);
	for (int z = 0; z < k_Size.y; ++z)
	{
		for (int x = 0; x < 5; ++x)
		{
			altitudes[static_cast<size_t>(z * k_Size.x + x)] = 6;
		}
		for (int x = 56; x < k_Size.x; ++x)
		{
			altitudes[static_cast<size_t>(z * k_Size.x + x)] = 120;
		}
	}
	const auto bed = seabed::Build(altitudes, k_Size, {0.0f, 0.0f}, 10.0f);
	ASSERT_EQ(bed.shore.size(), altitudes.size());
	const auto at = [&bed](int x) { return static_cast<size_t>(4 * bed.size.x + x); };
	// Out to sea the distance grows, inland it is below zero, and the direction points to the nearer coast
	EXPECT_LT(bed.shore[at(2)].x, 0.0f);
	EXPECT_GT(bed.shore[at(10)].x, bed.shore[at(6)].x);
	EXPECT_LT(bed.shore[at(10)].y, -0.9f);
	EXPECT_GT(bed.shore[at(50)].y, 0.9f);
	// The ground: land up, the sea bed down
	EXPECT_GT(bed.shore[at(2)].w, 0.0f);
	EXPECT_LT(bed.shore[at(30)].w, 0.0f);
	// The cliff's foot is marked, and the beach is not
	EXPECT_GT(bed.cliff[at(54)], 0.5f);
	EXPECT_FLOAT_EQ(bed.cliff[at(6)], 0.0f);
	EXPECT_FALSE(bed.cliffFoot.empty());
	for (const auto& foot : bed.cliffFoot)
	{
		EXPECT_GT(foot.x, 400.0f);
		// Out to sea from the right-hand cliff is towards -x
		EXPECT_LT(foot.z, 0.0f);
	}
}

TEST(Seabed, TheOriginalCoastStaysAtTheWater)
{
	// Land, then the original game's sea-level band of altitudes 3, 2 and 1, then the open sea
	constexpr glm::ivec2 k_Size {40, 3};
	std::vector<uint8_t> altitudes(static_cast<size_t>(k_Size.x * k_Size.y), 0);
	for (int z = 0; z < k_Size.y; ++z)
	{
		for (int x = 0; x < 10; ++x)
		{
			altitudes[static_cast<size_t>(z * k_Size.x + x)] = 8;
		}
		altitudes[static_cast<size_t>(z * k_Size.x + 10)] = 3;
		altitudes[static_cast<size_t>(z * k_Size.x + 11)] = 2;
		altitudes[static_cast<size_t>(z * k_Size.x + 12)] = 1;
	}
	const auto bed = seabed::Build(altitudes, k_Size, {0.0f, 0.0f}, 10.0f);
	const auto depth = [&bed](int x) { return bed.depth[static_cast<size_t>(bed.size.x + x)]; };
	EXPECT_FLOAT_EQ(depth(9), 0.0f);
	// The water's edge at high tide is on the original coast, between altitudes 3 and 2, and the band goes down a little
	EXPECT_FLOAT_EQ(depth(10), 0.0f);
	EXPECT_GT(depth(11), 0.0f);
	EXPECT_LT(depth(10), depth(11));
	EXPECT_LT(depth(11), depth(12));
	// A creature can walk on the cell from altitude 1 to the open sea: there the water only covers its feet
	EXPECT_LE(depth(13), seabed::k_WadeDepth);
	// Past it the sea bed goes on down, with no ledge at the edge of the shallows
	EXPECT_LT(depth(14) - depth(13), 3.0f);
	EXPECT_GT(depth(30), 1.0f);
}

TEST(Seabed, OnlyWalkableCellsAreShallow)
{
	EXPECT_TRUE(seabed::Walkable(0, 0, 0, 1));
	// All four corners at the bottom are deep sea
	EXPECT_FALSE(seabed::Walkable(0, 0, 0, 0));
	// Too steep for a creature: more than ten units of rise across the cell
	EXPECT_FALSE(seabed::Walkable(0, 0, 0, 30));
	EXPECT_TRUE(seabed::Walkable(0, 0, 0, 14));
}

TEST(Seabed, ABeachGoesOnDownGentlyUnderTheWater)
{
	// The same low coast twice, one of them a beach of sand
	constexpr glm::ivec2 k_Size {120, 3};
	std::vector<uint8_t> altitudes(static_cast<size_t>(k_Size.x * k_Size.y), 0);
	for (int z = 0; z < k_Size.y; ++z)
	{
		for (int x = 0; x < 5; ++x)
		{
			altitudes[static_cast<size_t>(z * k_Size.x + x)] = 12;
		}
	}
	std::vector<float> sand(altitudes.size(), 1.0f);
	const auto rock = seabed::Build(altitudes, k_Size, {0.0f, 0.0f}, 10.0f);
	const auto beach = seabed::Build(altitudes, k_Size, {0.0f, 0.0f}, 10.0f, sand);
	const auto depthOf = [](const seabed::Seabed& bed, int x) { return bed.depth[static_cast<size_t>(bed.size.x + x)]; };
	// Fifteen cells out the beach is still shallower than the other coast, at about its own grade
	EXPECT_LT(depthOf(beach, 4 + 15), depthOf(rock, 4 + 15));
	EXPECT_LT(depthOf(beach, 4 + 15), 150.0f * seabed::k_BeachSlope + 1.0f);
	// The grade does not step: each cell out is a little deeper
	for (int x = 6; x < 30; ++x)
	{
		EXPECT_GE(depthOf(beach, x + 1), depthOf(beach, x) - 0.05f) << x;
	}
}

TEST(Seabed, TheBeachIsTheShareOfSand)
{
	lnd::LNDCountry country {};
	country.materials.at(5) = {.indices = {0, 1}, .coefficient = 64};
	country.materials.at(9) = {.indices = {1, 1}, .coefficient = 128};
	const std::vector<uint16_t> types {static_cast<uint16_t>(TerrainMaterialType::Sand),
	                                   static_cast<uint16_t>(TerrainMaterialType::Grass)};
	// The first material weighs the coefficient out of 256
	EXPECT_FLOAT_EQ(seabed::BeachShare(country, 5, types), 0.25f);
	EXPECT_FLOAT_EQ(seabed::BeachShare(country, 9, types), 0.0f);
	// A material that the land does not have is not sand
	country.materials.at(2) = {.indices = {7, 7}, .coefficient = 256};
	EXPECT_FLOAT_EQ(seabed::BeachShare(country, 2, types), 0.0f);
}

TEST(Underwater, TheSprayNoiseTilesAndFillsItsRange)
{
	const auto texels = underwater::SprayNoiseVolume();
	constexpr int k_Side = underwater::k_SprayNoiseSide;
	ASSERT_EQ(texels.size(), static_cast<size_t>(k_Side) * k_Side * k_Side * 4);
	auto at = [&](int x, int y, int z, int channel) {
		return static_cast<int>(texels[(static_cast<size_t>((((z * k_Side) + y) * k_Side) + x) * 4) + channel]);
	};
	for (int channel = 0; channel < 4; ++channel)
	{
		int lowest = 255;
		int highest = 0;
		// The step from one texel to the next inside the cube, and across its edge from the last to the first
		double inside = 0.0;
		double across = 0.0;
		for (int z = 0; z < k_Side; ++z)
		{
			for (int y = 0; y < k_Side; ++y)
			{
				for (int x = 0; x < k_Side; ++x)
				{
					lowest = std::min(lowest, at(x, y, z, channel));
					highest = std::max(highest, at(x, y, z, channel));
				}
				inside += std::abs(at(k_Side / 2, y, z, channel) - at((k_Side / 2) - 1, y, z, channel));
				across += std::abs(at(0, y, z, channel) - at(k_Side - 1, y, z, channel));
			}
		}
		EXPECT_LT(lowest, 64) << channel;
		EXPECT_GT(highest, 160) << channel;
		// The edge is no harder a step than the middle: the volume repeats with no seam
		EXPECT_LT(across, inside * 2.0 + 1.0) << channel;
	}
}

TEST(Underwater, AFarTileNextToABlockIsFine)
{
	const auto blockAtOrigin = [](int x, int z) { return x == 0 && z == 0; };
	EXPECT_TRUE(seabed::FarTileIsFine(1, 0, blockAtOrigin));
	EXPECT_TRUE(seabed::FarTileIsFine(1, 1, blockAtOrigin));
	EXPECT_FALSE(seabed::FarTileIsFine(2, 0, blockAtOrigin));
}

TEST(Underwater, AFineTileSideOnACoarseTileLiesOnTheCoarseSide)
{
	const std::array<bool, 4> coarseAtXEnd {false, true, false, false};
	// Between the coarse tile's corners at z 4 and 8
	const auto [from, to] = seabed::FarCornerSpan({16, 5}, coarseAtXEnd);
	EXPECT_EQ(from, glm::ivec2(16, 4));
	EXPECT_EQ(to, glm::ivec2(16, 8));
	// On a coarse corner, or on a side next to a block or a fine tile, the corner is its own
	const auto [onFrom, onTo] = seabed::FarCornerSpan({16, 8}, coarseAtXEnd);
	EXPECT_EQ(onFrom, onTo);
	const auto [freeFrom, freeTo] = seabed::FarCornerSpan({0, 5}, coarseAtXEnd);
	EXPECT_EQ(freeFrom, glm::ivec2(0, 5));
	EXPECT_EQ(freeTo, glm::ivec2(0, 5));
}

TEST(Underwater, NoiseMatchesTheShaderAtItsCorners)
{
	// At a whole point the noise is the hash there; between points it stays inside 0 to 1
	EXPECT_FLOAT_EQ(underwater::Noise({3.0f, 7.0f}), underwater::Hash({3.0f, 7.0f}));
	for (float x = 0.0f; x < 20.0f; x += 0.37f)
	{
		const float value = underwater::Noise({x, x * 0.7f + 2.0f});
		EXPECT_GE(value, 0.0f);
		EXPECT_LE(value, 1.0f);
	}
}

TEST(Underwater, CliffCrashesComeOnceAWave)
{
	const auto waves = underwater::WavesFor(0.0f, 0.0f);
	const glm::vec2 foot {3020.0f, 2110.0f};
	// A calm sea: omega 1.1, so a crash about every 5.7 seconds
	int starts = 0;
	auto before = underwater::CliffCrashAt(foot, 0.0f, waves, 0.0f, 1.0f);
	for (float t = 0.01f; t <= 57.2f; t += 0.01f)
	{
		const auto now = underwater::CliffCrashAt(foot, t, waves, 0.0f, 1.0f);
		EXPECT_GE(now.since, 0.0f);
		starts += underwater::CrashStarts(before, now) ? 1 : 0;
		before = now;
	}
	EXPECT_EQ(starts, 10);
}

TEST(Underwater, CliffCrashesAreStrongerAtHighTideAndAtCliffs)
{
	const auto waves = underwater::WavesFor(0.0f, 0.0f);
	const glm::vec2 foot {3020.0f, 2110.0f};
	const auto high = underwater::CliffCrashAt(foot, 10.0f, waves, 0.0f, 1.0f);
	const auto low = underwater::CliffCrashAt(foot, 10.0f, waves, -underwater::k_LowestTide, 1.0f);
	const auto flat = underwater::CliffCrashAt(foot, 10.0f, waves, 0.0f, 0.0f);
	EXPECT_GT(high.power, low.power);
	EXPECT_NEAR(low.power / high.power, 0.7f, 1e-4f);
	EXPECT_FLOAT_EQ(flat.power, 0.0f);
	EXPECT_GT(high.power, 0.0f);
}

TEST(Underwater, OnlyNearAndSpreadCliffFeetAreHeard)
{
	const std::vector<glm::vec4> feet {
	    {0.0f, 0.0f, 1.0f, 0.0f},   {10.0f, 0.0f, 1.0f, 0.0f},  {100.0f, 0.0f, 1.0f, 0.0f},
	    {200.0f, 0.0f, 1.0f, 0.0f}, {300.0f, 0.0f, 1.0f, 0.0f}, {900.0f, 0.0f, 1.0f, 0.0f},
	};
	const auto heard = underwater::HeardCliffFeet(feet, {0.0f, 0.0f}, 3, 60.0f, 400.0f);
	// The foot 10 away is too near the first; the foot 900 away is too far
	EXPECT_EQ(heard, (std::vector<size_t> {0, 2, 3}));
	EXPECT_TRUE(underwater::HeardCliffFeet(feet, {5000.0f, 0.0f}, 3, 60.0f, 400.0f).empty());
}

TEST(Underwater, ShoreIsBlendedBetweenCorners)
{
	const std::vector<glm::vec4> shore {
	    {0.0f, 1.0f, 0.0f, 0.0f}, {10.0f, 1.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 0.0f, 0.0f}};
	const std::vector<float> cliff {0.0f, 1.0f, 0.0f, 0.0f};
	const auto middle = underwater::ShoreAt(shore, cliff, {2, 2}, 10.0f, {5.0f, 5.0f});
	EXPECT_FLOAT_EQ(middle.x, 15.0f);
	// The second corner is at the foot of a cliff: its direction is half as long
	EXPECT_FLOAT_EQ(middle.y, 0.375f);
	// No direction to the coast is the foot of a cliff, a full one a beach
	EXPECT_FLOAT_EQ(underwater::ShoreCliff({0.0f, 0.0f, 0.0f, 0.0f}), 1.0f);
	EXPECT_FLOAT_EQ(underwater::ShoreCliff({0.0f, 1.0f, 0.0f, 0.0f}), 0.0f);
}

TEST(Seabed, TheShelfFromTheCoastHasNoSteps)
{
	for (const float slope : {0.02f, 0.1f, 0.3f, 0.6f, 2.0f, 6.0f})
	{
		float last = seabed::ShelfFromCoastLine(0.0f, slope);
		EXPECT_FLOAT_EQ(last, 0.0f);
		for (float from = 0.5f; from < 200.0f; from += 0.5f)
		{
			const float depth = seabed::ShelfFromCoastLine(from, slope);
			EXPECT_GE(depth, last) << slope << " " << from;
			EXPECT_LT(depth - last, 0.5f * std::max(slope, 0.05f) + 0.01f) << slope << " " << from;
			last = depth;
		}
	}
	// A gentle coast only covers a creature's feet at the end of the shallows
	EXPECT_NEAR(seabed::ShelfFromCoastLine(seabed::k_ShallowsWidth, 0.1f), seabed::k_WadeDepth, 1e-4f);
	// A cliff is deep at once
	EXPECT_GT(seabed::ShelfFromCoastLine(5.0f, 6.0f), 20.0f);
}

// The far sea: the cube of the far scene, the long waves and the waves that each pixel shows

TEST(UnderwaterFarSea, EnvironmentFacesSkipTheFaceThatLooksDown)
{
	std::array<int, underwater::k_EnvironmentFaces> drawn {};
	uint8_t face = 0;
	for (int i = 0; i < 50; ++i)
	{
		++drawn.at(face);
		face = underwater::NextEnvironmentFace(face);
	}
	EXPECT_EQ(drawn.at(underwater::k_EnvironmentFaceDown), 0);
	for (uint8_t i = 0; i < underwater::k_EnvironmentFaces; ++i)
	{
		if (i != underwater::k_EnvironmentFaceDown)
		{
			EXPECT_EQ(drawn.at(i), 10);
		}
	}
}

TEST(UnderwaterFarSea, EnvironmentFacesAreSquareToEachOther)
{
	for (uint8_t i = 0; i < underwater::k_EnvironmentFaces; ++i)
	{
		const auto axes = underwater::EnvironmentFaceAxesOf(i);
		EXPECT_NEAR(glm::dot(axes.forward, axes.up), 0.0f, 1e-6f);
		// Each face looks along its own axis, the positive one first
		const int axis = i / 2;
		EXPECT_NEAR(axes.forward[axis], i % 2 == 0 ? 1.0f : -1.0f, 1e-6f);
	}
	const auto centre = underwater::EnvironmentCentre({100.0f, 300.0f, 200.0f}, -1.8f);
	EXPECT_FLOAT_EQ(centre.x, 100.0f);
	EXPECT_FLOAT_EQ(centre.z, 200.0f);
	EXPECT_GT(centre.y, -1.8f);
	EXPECT_LT(centre.y, 2.0f);
}

TEST(UnderwaterFarSea, LongWavesAreGentleAndGrowInAStorm)
{
	const auto calm = underwater::FarWaveTable(underwater::WavesFor(0.0f, 0.5f));
	const auto storm = underwater::FarWaveTable(underwater::WavesFor(1.0f, 0.5f));
	const auto longestWind = underwater::WaveTable(underwater::WavesFor(0.0f, 0.5f)).at(2).y;
	for (size_t i = 0; i < calm.size(); ++i)
	{
		EXPECT_GT(calm.at(i).y, longestWind);
		// Gentle: the steepness of a calm long wave is under 0.02
		EXPECT_LT(calm.at(i).z * 2.0f * std::numbers::pi_v<float> / calm.at(i).y, 0.02f);
		EXPECT_GT(storm.at(i).z, calm.at(i).z * 3.0f);
		EXPECT_GE(storm.at(i).y, calm.at(i).y);
	}
}

TEST(UnderwaterFarSea, TheLongWavesAreInTheHeightOfTheSea)
{
	// The things that float ride the same surface as the shaders draw: the long waves add to the height
	const auto waves = underwater::WavesFor(0.5f, 0.3f);
	float lowest = 1e9f;
	float highest = -1e9f;
	for (int x = 0; x < 200; ++x)
	{
		const float height = underwater::WavesAt({static_cast<float>(x) * 7.0f, 40.0f}, 0.0f, waves).height;
		lowest = std::min(lowest, height);
		highest = std::max(highest, height);
	}
	float farOnly = 0.0f;
	for (const auto& wave : underwater::FarWaveTable(waves))
	{
		farOnly += wave.z;
	}
	EXPECT_GT(highest - lowest, farOnly * 0.5f);
}

TEST(UnderwaterFarSea, APixelKeepsTheWavesItCanShow)
{
	// A wave of many pixels keeps all of its height and slope, and adds no roughness
	const auto big = underwater::FilterWave(100.0f, 0.3f, 1.0f);
	EXPECT_FLOAT_EQ(big.height, 1.0f);
	EXPECT_FLOAT_EQ(big.slope, 1.0f);
	EXPECT_FLOAT_EQ(big.variance, 0.0f);
	// A wave under a pixel shows nothing, and all of its slope goes into the roughness
	const auto small = underwater::FilterWave(1.0f, 0.02f, 2.0f);
	EXPECT_FLOAT_EQ(small.height, 0.0f);
	EXPECT_FLOAT_EQ(small.slope, 0.0f);
	const float steepness = 0.02f * 2.0f * std::numbers::pi_v<float>;
	EXPECT_NEAR(small.variance, 0.5f * steepness * steepness, 1e-6f);
	// Between the two, the normal keeps the slope longer than the surface keeps the height
	const auto middle = underwater::FilterWave(5.0f, 0.1f, 2.0f);
	EXPECT_LT(middle.height, middle.slope);
	EXPECT_GT(middle.variance, 0.0f);
}

TEST(UnderwaterFarSea, TheRoughnessGrowsWithTheWavesAPixelCannotShow)
{
	const float calm = underwater::FilteredRoughness(0.0f);
	EXPECT_GT(calm, 0.0f);
	EXPECT_LT(calm, 0.1f);
	EXPECT_GT(underwater::FilteredRoughness(0.01f), calm);
	EXPECT_GT(underwater::FilteredRoughness(0.1f), underwater::FilteredRoughness(0.01f));
	EXPECT_LE(underwater::FilteredRoughness(10.0f), 1.0f);
}
