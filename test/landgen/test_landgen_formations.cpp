/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>
#include <landgen/Formations.h>

using namespace openblack::landgen;
using biomes::Biome;
using features::Feature;
using features::Kind;

namespace
{
/// A fake land of 2 km a side, with 3.8 square kilometres of flat grassland at 40 m and the sea on one side
struct FlatLand
{
	static constexpr int k_Side = 200;
	static constexpr int k_SeaEdge = 10;
	Grid<Biome> biomes {k_Side, Biome::Grassland};
	/// 60 altitude units are 40.2 m
	Grid<float> heights {k_Side, 60.0f};
	Grid<float> sea {k_Side, 0.0f};
	Grid<uint8_t> keepClear {k_Side, 0};

	FlatLand()
	{
		for (int x = 0; x < k_Side; ++x)
		{
			for (int z = 0; z < k_Side; ++z)
			{
				if (x < k_SeaEdge)
				{
					biomes.At(x, z) = Biome::Sea;
					heights.At(x, z) = 0.0f;
				}
				else
				{
					sea.At(x, z) = static_cast<float>(x - k_SeaEdge + 1);
				}
			}
		}
	}

	[[nodiscard]] features::Ground Ground() const
	{
		return {.biomes = biomes,
		        .heights = heights,
		        .seaDistance = sea,
		        .keepClear = keepClear,
		        .clearings = {},
		        .townX = 20.0f,
		        .townZ = 20.0f};
	}
};

float Distance(float ax, float az, float bx, float bz)
{
	return std::hypot(ax - bx, az - bz);
}

/// Many rings of a land of the story and no pillars, or many groups of its pillars and no rings
constexpr formations::Settings ManyRings(uint8_t land)
{
	return {.land = land, .ringsPerKm2 = 1.0f, .pillarGroupsPerKm2 = 0.0f};
}
constexpr formations::Settings ManyPillars(uint8_t land)
{
	return {.land = land, .ringsPerKm2 = 0.0f, .pillarGroupsPerKm2 = 1.5f};
}
/// The first land of the story: both a ring and groups of pillars
constexpr formations::Settings k_Both {.land = 0, .ringsPerKm2 = 1.0f, .pillarGroupsPerKm2 = 1.5f};
constexpr formations::Settings k_ManyRings = ManyRings(0);
constexpr formations::Settings k_ManyPillars = ManyPillars(0);

std::vector<const Feature*> Near(const std::vector<Feature>& placed, float x, float z, float radius)
{
	std::vector<const Feature*> near;
	for (const auto& feature : placed)
	{
		if (Distance(feature.x, feature.z, x, z) <= radius)
		{
			near.push_back(&feature);
		}
	}
	return near;
}
} // namespace

TEST(LandgenFormations, TheCountIsTheMeanWithItsFractionAsAChance)
{
	EXPECT_EQ(formations::CountFor(0.5f, 3.0f, 0.0f), 1u);
	EXPECT_EQ(formations::CountFor(0.5f, 3.0f, 0.49f), 1u);
	EXPECT_EQ(formations::CountFor(0.5f, 3.0f, 0.5f), 2u);
	EXPECT_EQ(formations::CountFor(0.5f, 3.0f, 0.999f), 2u);
	EXPECT_EQ(formations::CountFor(0.0f, 3.0f, 0.999f), 0u);
}

TEST(LandgenFormations, EachDensityIsTheDensityOfALandOfTheStory)
{
	// The first land: one ring and six groups of pillars on 2.06 km2
	EXPECT_NEAR(formations::DensityOf(0.0f).rings, 0.485f, 0.001f);
	EXPECT_NEAR(formations::DensityOf(0.1f).pillarGroups, 2.910f, 0.001f);
	// The second land: two groups on 2.65 km2, and no ring
	EXPECT_EQ(formations::DensityOf(0.2f).rings, 0.0f);
	EXPECT_NEAR(formations::DensityOf(0.2f).pillarGroups, 0.754f, 0.001f);
	// The third land: one ring and one group on 1.71 km2
	EXPECT_EQ(formations::DensityOf(0.3f).land, 2u);
	EXPECT_NEAR(formations::DensityOf(0.3f).rings, 0.585f, 0.001f);
	EXPECT_NEAR(formations::DensityOf(0.3f).pillarGroups, 0.585f, 0.001f);
	// The other five lands have none
	for (const float unit : {0.4f, 0.5f, 0.7f, 0.8f, 0.99f})
	{
		EXPECT_EQ(formations::DensityOf(unit).rings, 0.0f);
		EXPECT_EQ(formations::DensityOf(unit).pillarGroups, 0.0f);
	}
}

TEST(LandgenFormations, TheAreaCountsTheCornersOutOfTheSea)
{
	const FlatLand land;
	EXPECT_NEAR(formations::LandKm2(land.Ground()), 3.8f, 1e-4f);
}

TEST(LandgenFormations, EachRingIsACopyOfTheRingOfItsLandOfTheStory)
{
	const FlatLand land;
	for (uint64_t seed = 1; seed <= 6; ++seed)
	{
		std::vector<Feature> placed;
		// The odd seeds take the first land, and the even seeds the third land
		const uint8_t story = seed % 2 == 1 ? 0 : 2;
		const auto result = formations::Place(land.Ground(), placed, {}, seed, ManyRings(story));
		ASSERT_FALSE(result.rings.empty()) << seed;
		EXPECT_TRUE(result.pillarGroups.empty());
		for (const auto& ring : result.rings)
		{
			const auto things = Near(placed, ring.x, ring.z, 10.0f);
			const auto count = [&things](Kind kind, uint8_t type) {
				return std::ranges::count_if(things,
				                             [kind, type](const Feature* f) { return f->kind == kind && f->type == type; });
			};
			EXPECT_EQ(ring.stamp, static_cast<uint8_t>(story == 0 ? formations::Ring::First : formations::Ring::Third));
			if (ring.stamp == static_cast<uint8_t>(formations::Ring::First))
			{
				ASSERT_EQ(things.size(), 17u);
				EXPECT_EQ(count(Kind::Mobile, formations::k_MagicMushroomObject), 10);
				EXPECT_EQ(count(Kind::Mobile, formations::k_ChampiObject), 2);
				EXPECT_EQ(count(Kind::Static, features::k_PlainRockStatic), 5);
			}
			else
			{
				ASSERT_EQ(things.size(), 6u);
				EXPECT_EQ(count(Kind::Mobile, formations::k_MagicMushroomObject), 6);
			}
			for (const auto* thing : things)
			{
				const float r = Distance(thing->x, thing->z, ring.x, ring.z);
				if (thing->kind == Kind::Static)
				{
					// The small rocks lie in the middle and lean into the ground, as the rocks of the story do
					EXPECT_LT(r, 3.0f);
					EXPECT_GT(thing->altitude, 0.0f);
				}
				else if (ring.stamp == static_cast<uint8_t>(formations::Ring::First))
				{
					EXPECT_GT(r, 5.0f);
					EXPECT_LT(r, 9.0f);
				}
				else
				{
					EXPECT_GT(r, 1.5f);
					EXPECT_LT(r, 4.5f);
					EXPECT_GE(thing->size, 1.0f);
				}
			}
		}
	}
}

TEST(LandgenFormations, RingsAndGroupsOfPillarsKeepTheirGaps)
{
	const FlatLand land;
	for (uint64_t seed = 1; seed <= 6; ++seed)
	{
		std::vector<Feature> placed;
		const auto rings = formations::Place(land.Ground(), placed, {}, seed, k_ManyRings).rings;
		for (size_t a = 0; a < rings.size(); ++a)
		{
			for (size_t b = a + 1; b < rings.size(); ++b)
			{
				EXPECT_GE(Distance(rings[a].x, rings[a].z, rings[b].x, rings[b].z), 116.0f);
			}
		}
		placed.clear();
		const auto groups = formations::Place(land.Ground(), placed, {}, seed, k_ManyPillars).pillarGroups;
		EXPECT_GE(groups.size(), 3u);
		for (size_t a = 0; a < groups.size(); ++a)
		{
			for (size_t b = a + 1; b < groups.size(); ++b)
			{
				EXPECT_GE(Distance(groups[a].x, groups[a].z, groups[b].x, groups[b].z), 217.0f);
			}
		}
	}
}

TEST(LandgenFormations, TheLonePillarsAreThePillarsOfLimestoneOfTheStory)
{
	const FlatLand land;
	for (uint64_t seed = 1; seed <= 6; ++seed)
	{
		std::vector<Feature> placed;
		const auto story = static_cast<uint8_t>(seed % 3);
		const auto result = formations::Place(land.Ground(), placed, {}, seed, ManyPillars(story));
		// The first land has groups 0 to 5, the second land groups 6 and 7, and the third land group 8
		for (const auto& group : result.pillarGroups)
		{
			EXPECT_GE(group.stamp, story == 0 ? 0u : (story == 1 ? 6u : 8u));
			EXPECT_LE(group.stamp, story == 0 ? 5u : (story == 1 ? 7u : 8u));
		}
		size_t pillars = 0;
		for (const auto& feature : placed)
		{
			ASSERT_EQ(feature.kind, Kind::Feature);
			EXPECT_TRUE(feature.type == formations::k_FatPillarLime || feature.type == formations::k_Pillar2Lime ||
			            feature.type == formations::k_Pillar3Lime);
			EXPECT_GE(feature.size, 0.059f);
			EXPECT_LE(feature.size, 1.0f);
			// Each pillar is 13 m or less from the first pillar of its group
			EXPECT_TRUE(std::ranges::any_of(result.pillarGroups, [&feature](const formations::Placed& group) {
				return Distance(feature.x, feature.z, group.x, group.z) < 13.0f;
			}));
			++pillars;
		}
		EXPECT_GE(pillars, result.pillarGroups.size());
		EXPECT_LE(pillars, result.pillarGroups.size() * 3);
	}
}

TEST(LandgenFormations, ARingRemovesTheTreesAndRocksUnderIt)
{
	const FlatLand land;
	std::vector<Feature> placed;
	for (int i = 12; i < FlatLand::k_Side; i += 1)
	{
		for (int j = 0; j < FlatLand::k_Side; j += 1)
		{
			placed.push_back({.kind = Kind::Tree,
			                  .type = static_cast<uint8_t>(features::Tree::Oak),
			                  .x = static_cast<float>(i) * 10.0f,
			                  .z = static_cast<float>(j) * 10.0f,
			                  .angle = 0.0f,
			                  .size = 1.0f,
			                  .maxSize = 1.0f});
		}
	}
	const auto result = formations::Place(land.Ground(), placed, {}, 5, k_ManyRings);
	ASSERT_FALSE(result.rings.empty());
	for (const auto& ring : result.rings)
	{
		// The clearing goes 1 m past the farthest thing of the ring
		const float clearing = ring.stamp == static_cast<uint8_t>(formations::Ring::First) ? 9.6f : 4.9f;
		for (const auto* thing : Near(placed, ring.x, ring.z, clearing))
		{
			EXPECT_NE(thing->kind, Kind::Tree);
		}
	}
}

TEST(LandgenFormations, ALandOfTheStoryWithNoPillarsGivesNoPillars)
{
	const FlatLand land;
	std::vector<Feature> placed;
	for (uint8_t story = 3; story < 8; ++story)
	{
		const auto result = formations::Place(land.Ground(), placed, {}, 4, ManyPillars(story));
		EXPECT_TRUE(result.pillarGroups.empty());
	}
	EXPECT_TRUE(placed.empty());
}

TEST(LandgenFormations, NoRingOnSteepGround)
{
	FlatLand land;
	// A slope of 25 degrees in steps 100 m long: too steep for a ring, not too steep for a pillar
	for (int x = FlatLand::k_SeaEdge; x < FlatLand::k_Side; ++x)
	{
		for (int z = 0; z < FlatLand::k_Side; ++z)
		{
			land.heights.At(x, z) = 50.0f + (7.0f * static_cast<float>(z % 10));
		}
	}
	std::vector<Feature> placed;
	const auto result = formations::Place(land.Ground(), placed, {}, 2, k_Both);
	EXPECT_TRUE(result.rings.empty());
	EXPECT_FALSE(result.pillarGroups.empty());
}

TEST(LandgenFormations, NothingOnHighGroundOrInTheCirclesToAvoid)
{
	FlatLand land;
	// 150 m is higher than every ring and lone pillar of the story
	std::ranges::replace(land.heights.values, 60.0f, 224.0f);
	std::vector<Feature> placed;
	auto result = formations::Place(land.Ground(), placed, {}, 3, k_Both);
	EXPECT_TRUE(result.rings.empty());
	EXPECT_TRUE(result.pillarGroups.empty());

	const FlatLand flat;
	const std::vector<formations::Circle> avoid {{.x = 1000.0f, .z = 1000.0f, .radius = 2000.0f}};
	result = formations::Place(flat.Ground(), placed, avoid, 3, k_Both);
	EXPECT_TRUE(result.rings.empty());
	EXPECT_TRUE(result.pillarGroups.empty());
	EXPECT_TRUE(placed.empty());
}

TEST(LandgenFormations, TheSameSeedGivesTheSameFormations)
{
	const FlatLand land;
	std::vector<Feature> first;
	std::vector<Feature> second;
	const auto a = formations::Place(land.Ground(), first, {}, 11, k_Both);
	const auto b = formations::Place(land.Ground(), second, {}, 11, k_Both);
	ASSERT_EQ(first.size(), second.size());
	ASSERT_EQ(a.rings.size(), b.rings.size());
	ASSERT_EQ(a.pillarGroups.size(), b.pillarGroups.size());
	for (size_t k = 0; k < first.size(); ++k)
	{
		EXPECT_EQ(first[k].x, second[k].x);
		EXPECT_EQ(first[k].z, second[k].z);
		EXPECT_EQ(first[k].type, second[k].type);
	}
}
