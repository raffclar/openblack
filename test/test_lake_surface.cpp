/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>

#include <3D/LandLakes.h>
#include <Graphics/LakeSurface.h>
#include <gtest/gtest.h>

using namespace openblack;
using namespace openblack::graphics;

namespace
{
constexpr float k_Plateau = 40.0f;
constexpr float k_Level = 39.0f;
constexpr float k_Reach = 0.67f;
constexpr float k_Middle = 100.0f;
constexpr float k_Radius = 6.0f;

/// A hill top at the plateau's height with a round bowl in it, 4 deep at its middle
float BowlHeight(int x, int z)
{
	const float dx = static_cast<float>(x) - k_Middle;
	const float dz = static_cast<float>(z) - k_Middle;
	const float part = ((dx * dx) + (dz * dz)) / (k_Radius * k_Radius);
	return part < 1.0f ? k_Plateau - (4.0f * (1.0f - part)) : k_Plateau;
}

/// The lake of the bowl: its water and the shore round it
land_lakes::LakeMap BowlLake()
{
	land_lakes::Lake lake {.level = k_Level, .corners = {}};
	for (int x = 90; x <= 110; ++x)
	{
		for (int z = 90; z <= 110; ++z)
		{
			if (BowlHeight(x, z) < k_Level + k_Reach)
			{
				lake.corners.push_back(static_cast<uint32_t>((x * land_lakes::k_CornersPerSide) + z));
			}
		}
	}
	return land_lakes::LakeMap({lake});
}

lake_surface::Surface BuildBowl(const land_lakes::LakeMap& lakes)
{
	return lake_surface::Build(
	    lakes, [](int x, int z) -> std::optional<float> { return BowlHeight(x, z); },
	    [](int x, int z) { return ((x + z) & 1) != 0; }, k_Reach);
}
} // namespace

TEST(LandLakes, TheWaterLevelIsTheLakesInItsCellsAndTheSeasElsewhere)
{
	const auto lakes = BowlLake();
	EXPECT_FLOAT_EQ(lakes.WaterLevelAt({k_Middle * 10.0f + 5.0f, k_Middle * 10.0f + 5.0f}), k_Level);
	EXPECT_FLOAT_EQ(lakes.WaterLevelAt({500.0f, 500.0f}), land_lakes::k_SeaLevel);
	EXPECT_FALSE(lakes.LakeAt({-10.0f, 20.0f}).has_value());
	EXPECT_FALSE(land_lakes::LakeMap().LakeAt({1000.0f, 1000.0f}).has_value());
}

TEST(LakeSurface, ALakeOnAHillIsNotARectangle)
{
	const auto surface = BuildBowl(BowlLake());
	ASSERT_FALSE(surface.vertices.empty());
	ASSERT_EQ(surface.indices.size() % 3, 0u);
	for (const auto& vertex : surface.vertices)
	{
		// No water goes past the shore: the land under every vertex is at the level or under it
		EXPECT_LE(vertex.landHeight, k_Level + 1e-4f);
		const float dx = (vertex.x / 10.0f) - k_Middle;
		const float dz = (vertex.z / 10.0f) - k_Middle;
		// The land between the corners is flat across each triangle, so the shore can reach a little past the circle
		EXPECT_LT(std::sqrt((dx * dx) + (dz * dz)), k_Radius * 1.05f);
	}
	// The flat part is the round water: its area is near the area of the circle where the bowl is under the level
	float flatArea = 0.0f;
	for (size_t i = 0; i < surface.indices.size(); i += 3)
	{
		const auto& a = surface.vertices[surface.indices[i]];
		const auto& b = surface.vertices[surface.indices[i + 1]];
		const auto& c = surface.vertices[surface.indices[i + 2]];
		if (a.y == k_Level && b.y == k_Level && c.y == k_Level && a.landHeight <= k_Level + 1e-4f &&
		    b.landHeight <= k_Level + 1e-4f && c.landHeight <= k_Level + 1e-4f)
		{
			flatArea += 0.5f * std::abs(((b.x - a.x) * (c.z - a.z)) - ((c.x - a.x) * (b.z - a.z)));
		}
	}
	// The bowl is under the level of 39 where part < 3/4
	const float waterRadius = k_Radius * std::sqrt(0.75f) * 10.0f;
	const float circle = 3.14159265f * waterRadius * waterRadius;
	EXPECT_NEAR(flatArea, circle, circle * 0.08f);
}

TEST(LakeSurface, TheWaterIsFlatAndFadesToNothingAtTheShore)
{
	const auto surface = BuildBowl(BowlLake());
	bool hasEdge = false;
	for (const auto& vertex : surface.vertices)
	{
		EXPECT_FLOAT_EQ(vertex.y, k_Level);
		EXPECT_FLOAT_EQ(vertex.level, k_Level);
		// The water is only over land at the level or under it
		EXPECT_LE(vertex.landHeight, k_Level + 1e-4f);
		EXPECT_LE(vertex.fade, k_Level + 1e-4f);
		if (std::abs(vertex.fade - k_Level) < 1e-4f)
		{
			hasEdge = true;
		}
	}
	EXPECT_TRUE(hasEdge);
}

TEST(LakeSurface, NoLandGivesNoWater)
{
	const auto surface = lake_surface::Build(
	    BowlLake(), [](int, int) -> std::optional<float> { return std::nullopt; }, [](int, int) { return false; }, k_Reach);
	EXPECT_TRUE(surface.vertices.empty());
	EXPECT_TRUE(lake_surface::Build(
	                land_lakes::LakeMap(), [](int, int) -> std::optional<float> { return 0.0f; },
	                [](int, int) { return false; }, k_Reach)
	                .indices.empty());
}

TEST(LakeSurface, LowGroundOutsideTheLakeGetsNoWater)
{
	// The lake has only the corner at the middle, in a cell with three corners as low as the water
	land_lakes::Lake lake {.level = k_Level, .corners = {static_cast<uint32_t>((100 * land_lakes::k_CornersPerSide) + 100)}};
	const land_lakes::LakeMap lakes({lake});
	const auto surface = lake_surface::Build(
	    lakes, [](int, int) -> std::optional<float> { return k_Level - 1.0f; }, [](int, int) { return false; }, k_Reach);
	for (const auto& vertex : surface.vertices)
	{
		// The water stops between the lake's corner and the corners round it, which count as high land
		const float dx = std::abs((vertex.x / 10.0f) - 100.0f);
		const float dz = std::abs((vertex.z / 10.0f) - 100.0f);
		EXPECT_LT(std::max(dx, dz), 1.0f);
	}
}

TEST(LakeSurface, NoWaterGoesPastTheSpill)
{
	// A shore corner just above the level, with the land past it far under the level outside the lake
	land_lakes::Lake lake {.level = k_Level, .corners = {static_cast<uint32_t>((100 * land_lakes::k_CornersPerSide) + 100)}};
	const land_lakes::LakeMap lakes({lake});
	const auto surface = lake_surface::Build(
	    lakes, [](int x, int z) -> std::optional<float> { return x == 100 && z == 100 ? k_Level + 0.3f : k_Level - 10.0f; },
	    [](int, int) { return false; }, k_Reach);
	EXPECT_TRUE(surface.vertices.empty());
}
