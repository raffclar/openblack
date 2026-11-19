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
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "landgen/Generator.h"
#include "landgen/RoadBed.h"

using namespace openblack::landgen;
using openblack::landgen::roads::BedOf;
using openblack::landgen::roads::CutThinWalls;
using openblack::landgen::roads::Grade;
using openblack::landgen::roads::Ground;
using openblack::landgen::roads::Profile;

namespace
{
constexpr int k_Side = 48;
constexpr float k_Corner = 10.0f;

/// The largest grade between two samples next to each other
float SteepestOf(const std::vector<float>& profile, float step)
{
	float steepest = 0.0f;
	for (size_t i = 1; i < profile.size(); ++i)
	{
		steepest = std::max(steepest, std::fabs(profile[i] - profile[i - 1]) / step);
	}
	return steepest;
}

/// The largest change of grade from one sample to the next
float LargestBendOf(const std::vector<float>& profile, float step)
{
	float largest = 0.0f;
	for (size_t i = 2; i < profile.size(); ++i)
	{
		const float before = (profile[i - 1] - profile[i - 2]) / step;
		const float after = (profile[i] - profile[i - 1]) / step;
		largest = std::max(largest, std::fabs(after - before));
	}
	return largest;
}

/// A land that slopes across the z axis: the height grows with z by the rise over the run
struct SideSlope
{
	Grid<float> metres {k_Side, 0.0f};
	Grid<uint8_t> fixed {k_Side, 0};
	Grid<float> lowest {k_Side, -1000.0f};

	explicit SideSlope(float slope)
	{
		for (int x = 0; x < k_Side; ++x)
		{
			for (int z = 0; z < k_Side; ++z)
			{
				metres.At(x, z) = 100.0f + (slope * k_Corner * static_cast<float>(z - (k_Side / 2)));
			}
		}
	}

	[[nodiscard]] Ground GroundOf() const { return {.metres = metres, .fixed = fixed, .lowest = lowest}; }
};

/// A straight path along the x axis, with its drawn middle on the row z = k_Side / 2
paths::Path StraightPath()
{
	paths::Path path {.link = {.from = 0, .to = 1}, .points = {}};
	for (int x = 2; x < k_Side - 2; ++x)
	{
		path.points.push_back({.x = static_cast<float>(x) - 0.5f, .z = static_cast<float>(k_Side / 2) - 0.5f});
	}
	return path;
}

/// The largest slope between two corners next to each other along an axis
float SteepestCornerOf(const Grid<float>& metres)
{
	float steepest = 0.0f;
	for (int x = 0; x + 1 < metres.side; ++x)
	{
		for (int z = 0; z + 1 < metres.side; ++z)
		{
			steepest = std::max(steepest, std::fabs(metres.At(x + 1, z) - metres.At(x, z)) / k_Corner);
			steepest = std::max(steepest, std::fabs(metres.At(x, z + 1) - metres.At(x, z)) / k_Corner);
		}
	}
	return steepest;
}
} // namespace

TEST(LandgenRoadBed, TheProfileKeepsAnEvenClimb)
{
	const roads::Settings settings;
	std::vector<float> land;
	for (int i = 0; i < 200; ++i)
	{
		land.push_back(0.1f * settings.step * static_cast<float>(i));
	}
	const auto bed = Profile(land, {}, settings);
	ASSERT_EQ(bed.size(), land.size());
	for (size_t i = 0; i < land.size(); ++i)
	{
		EXPECT_NEAR(bed[i], land[i], 0.01f) << i;
	}
}

TEST(LandgenRoadBed, TheProfileSmoothsAStepAndKeepsTheEnds)
{
	const roads::Settings settings;
	// Flat ground, a rise of 12 m over one sample, then flat ground
	std::vector<float> land(120, 0.0f);
	std::fill(land.begin() + 60, land.end(), 12.0f);
	const auto bed = Profile(land, {}, settings);
	EXPECT_EQ(bed.front(), land.front());
	EXPECT_EQ(bed.back(), land.back());
	const float landSteepest = SteepestOf(land, settings.step);
	const float bedSteepest = SteepestOf(bed, settings.step);
	EXPECT_LE(bedSteepest, settings.steepestGrade + 0.001f);
	EXPECT_LT(bedSteepest, landSteepest / 5.0f);
	EXPECT_LT(LargestBendOf(bed, settings.step), LargestBendOf(land, settings.step) / 10.0f);
	// The bed climbs as the land does
	EXPECT_NEAR(bed[20], 0.0f, 0.01f);
	EXPECT_NEAR(bed[100], 12.0f, 0.01f);
}

TEST(LandgenRoadBed, TheProfileRemovesBumpsButFollowsTheLand)
{
	const roads::Settings settings;
	std::vector<float> land;
	for (int i = 0; i < 200; ++i)
	{
		// A slow climb with a bump of 2 m up and down on every fourth sample
		land.push_back((0.05f * settings.step * static_cast<float>(i)) + ((i % 4) == 0 ? 2.0f : 0.0f));
	}
	const auto bed = Profile(land, {}, settings);
	EXPECT_LT(LargestBendOf(bed, settings.step), LargestBendOf(land, settings.step) / 4.0f);
	for (size_t i = 0; i < land.size(); ++i)
	{
		EXPECT_LE(std::fabs(bed[i] - land[i]), 2.5f) << i;
	}
}

TEST(LandgenRoadBed, TheProfileStaysAtTheLandAtAFixedSample)
{
	const roads::Settings settings;
	std::vector<float> land(100, 10.0f);
	std::vector<uint8_t> fixed(100, 0);
	// A river bed 4 m below the land in the middle of the path
	land[50] = 6.0f;
	fixed[50] = 1;
	const auto bed = Profile(land, fixed, settings);
	EXPECT_EQ(bed[50], 6.0f);
	// The bed goes down to the river and comes back up with no step steeper than the fixed grade
	EXPECT_LE(SteepestOf(bed, settings.step), settings.fixedGrade + 0.001f);
	EXPECT_NEAR(bed[10], 10.0f, 0.01f);
}

TEST(LandgenRoadBed, TheProfileOfASteepClimbStaysNearTheLand)
{
	const roads::Settings settings;
	std::vector<float> land;
	for (int i = 0; i < 200; ++i)
	{
		land.push_back(0.6f * settings.step * static_cast<float>(i));
	}
	const auto bed = Profile(land, {}, settings);
	for (size_t i = 0; i < land.size(); ++i)
	{
		EXPECT_LE(std::fabs(bed[i] - land[i]), settings.deepest + 0.001f) << i;
	}
}

TEST(LandgenRoadBed, TheBedIsLevelAcrossThePathAndTheBanksHaveTheFixedSlope)
{
	const roads::Settings settings;
	// The land slopes across the path less steeply than the bank
	const SideSlope land(0.3f);
	const auto path = StraightPath();
	const auto ground = land.GroundOf();
	const auto bed = BedOf(std::span(&path, 1), ground, settings);
	const auto graded = Grade(bed, ground, settings);
	const int middle = k_Side / 2;
	const int x = k_Side / 2;
	// The corners on each side of the middle are level with it
	EXPECT_TRUE(bed.covered.At(x, middle) != 0);
	EXPECT_TRUE(bed.covered.At(x, middle - 1) != 0);
	EXPECT_TRUE(bed.covered.At(x, middle + 1) != 0);
	EXPECT_NEAR(graded.At(x, middle - 1), graded.At(x, middle), 0.01f);
	EXPECT_NEAR(graded.At(x, middle + 1), graded.At(x, middle), 0.01f);
	EXPECT_NEAR(graded.At(x, middle), land.metres.At(x, middle), 0.01f);
	// The bank cuts into the land on the high side and fills on the low side, at the slope of the bank
	EXPECT_LT(graded.At(x, middle + 2), land.metres.At(x, middle + 2));
	EXPECT_GT(graded.At(x, middle - 2), land.metres.At(x, middle - 2));
	EXPECT_NEAR(graded.At(x, middle + 2) - graded.At(x, middle + 1), settings.cutSlope * k_Corner, 0.01f);
	EXPECT_NEAR(graded.At(x, middle - 1) - graded.At(x, middle - 2), settings.cutSlope * k_Corner, 0.01f);
	// The bank meets the land, and the land beyond it does not change
	for (int z = 0; z < k_Side; ++z)
	{
		if (std::abs(z - middle) >= 6)
		{
			EXPECT_EQ(graded.At(x, z), land.metres.At(x, z)) << z;
		}
	}
	EXPECT_LE(SteepestCornerOf(graded), settings.cutSlope + 0.001f);
}

TEST(LandgenRoadBed, ABankOnASteepSideHasNoVerticalStepAndStopsAtTheWidestBank)
{
	const roads::Settings settings;
	// The land slopes across the path more steeply than the bank, as a cliff does
	const SideSlope land(1.2f);
	const auto path = StraightPath();
	const auto ground = land.GroundOf();
	const auto graded = Grade(BedOf(std::span(&path, 1), ground, settings), ground, settings);
	const int middle = k_Side / 2;
	const int x = k_Side / 2;
	EXPECT_NEAR(graded.At(x, middle + 1), graded.At(x, middle), 0.01f);
	// The bank is wider than on gentle land. Its change to the land becomes less by the taper, so that it stops.
	EXPECT_LT(graded.At(x, middle + 4), land.metres.At(x, middle + 4));
	EXPECT_GT(graded.At(x, middle - 4), land.metres.At(x, middle - 4));
	const float edge = std::fabs(graded.At(x, middle + 1) - land.metres.At(x, middle + 1));
	const int farthest = 1 + static_cast<int>(std::ceil(edge / (settings.taper * k_Corner)));
	for (int z = 0; z < k_Side; ++z)
	{
		if (std::abs(z - middle) > farthest)
		{
			EXPECT_EQ(graded.At(x, z), land.metres.At(x, z)) << z;
		}
	}
	// No step between two corners is vertical: the ground is never steeper than the land by more than the taper
	EXPECT_LE(SteepestCornerOf(graded), 1.2f + settings.taper + 0.001f);
}

TEST(LandgenRoadBed, APathAtTheFootOfACliffDoesNotCutTheCliff)
{
	const roads::Settings settings;
	SideSlope land(0.0f);
	const int middle = k_Side / 2;
	// A cliff of 80 m starts on the row next to the middle of the path
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = middle + 1; z < k_Side; ++z)
		{
			land.metres.At(x, z) = 180.0f;
		}
	}
	const auto path = StraightPath();
	const auto ground = land.GroundOf();
	const auto bed = BedOf(std::span(&path, 1), ground, settings);
	const auto graded = Grade(bed, ground, settings);
	EXPECT_EQ(bed.covered.At(middle, middle + 1), 0);
	EXPECT_EQ(graded.values, land.metres.values);
}

TEST(LandgenRoadBed, TheLandOfAThinHumpIsCutAndAWideHillStays)
{
	const roads::Settings settings;
	// Flat land at 100 m with a hump of 3 samples (15 m) at 140 m and a hill of 20 samples (100 m) at 110 m
	std::vector<float> land(60, 100.0f);
	for (size_t i = 10; i < 13; ++i)
	{
		land[i] = 140.0f;
	}
	for (size_t i = 30; i < 50; ++i)
	{
		land[i] = 110.0f;
	}
	const std::vector<uint8_t> fixed(land.size(), 0);
	const auto cut = CutThinWalls(land, fixed, settings);
	// The cut goes no deeper than the deepest cut of a wall
	EXPECT_FLOAT_EQ(cut[11], 140.0f - settings.deepestWallCut);
	EXPECT_FLOAT_EQ(cut[5], 100.0f);
	EXPECT_FLOAT_EQ(cut[40], 110.0f);
	// A fixed sample keeps its land
	auto ring = fixed;
	ring[11] = roads::k_Ring;
	EXPECT_FLOAT_EQ(CutThinWalls(land, ring, settings)[11], 140.0f);
}

TEST(LandgenRoadBed, ThePathCutsThroughAThinWallOfRockAndDoesNotFillTheLandNearIt)
{
	const roads::Settings settings;
	SideSlope land(0.0f);
	const int middle = k_Side / 2;
	// A wall of rock two corners thick and 15 m high across the path
	for (int z = 0; z < k_Side; ++z)
	{
		land.metres.At(20, z) = 115.0f;
		land.metres.At(21, z) = 115.0f;
	}
	const auto path = StraightPath();
	const auto ground = land.GroundOf();
	const auto graded = Grade(BedOf(std::span(&path, 1), ground, settings), ground, settings);
	// The bed goes through the wall at the height of the land on each side
	EXPECT_NEAR(graded.At(20, middle), 100.0f, 0.5f);
	EXPECT_NEAR(graded.At(21, middle), 100.0f, 0.5f);
	// No corner is filled above the land
	for (size_t i = 0; i < graded.values.size(); ++i)
	{
		EXPECT_LE(graded.values[i], land.metres.values[i] + 0.01f);
	}
}

TEST(LandgenRoadBed, FixedCornersAndTheLowestHeightStayAsTheyAre)
{
	const roads::Settings settings;
	SideSlope land(0.3f);
	const int middle = k_Side / 2;
	// A river across the path, and a sea floor that no cut goes below on the low side
	for (int z = 0; z < k_Side; ++z)
	{
		land.fixed.At(20, z) = 1;
		land.fixed.At(21, z) = 1;
	}
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			land.lowest.At(x, z) = land.metres.At(x, middle + 2) + 1.0f;
		}
	}
	const auto path = StraightPath();
	const auto ground = land.GroundOf();
	const auto bed = BedOf(std::span(&path, 1), ground, settings);
	const auto graded = Grade(bed, ground, settings);
	for (int z = 0; z < k_Side; ++z)
	{
		EXPECT_EQ(graded.At(20, z), land.metres.At(20, z));
		EXPECT_EQ(graded.At(21, z), land.metres.At(21, z));
		EXPECT_EQ(bed.covered.At(20, z), 0);
	}
	for (int x = 0; x < k_Side; ++x)
	{
		// The cut on the high side stops at the lowest height
		EXPECT_GE(graded.At(x, middle + 3), std::min(land.metres.At(x, middle + 3), land.lowest.At(x, middle + 3)));
	}
}

TEST(LandgenRoadBed, NoPathChangesNothing)
{
	const roads::Settings settings;
	const SideSlope land(0.5f);
	const auto ground = land.GroundOf();
	const auto graded = Grade(BedOf({}, ground, settings), ground, settings);
	EXPECT_EQ(graded.values, land.metres.values);
}

TEST(LandgenRoadBed, AGeneratedLandCutsAndFillsTheBedsOfItsPathsAndMakesNoNewCliff)
{
	openblack::landgen::Settings settings;
	settings.seed = 7;
	const auto graded = Generate(settings);
	settings.roads.grade = false;
	const auto flat = Generate(settings);
	ASSERT_EQ(graded.paths.size(), flat.paths.size());
	ASSERT_EQ(graded.cells.values.size(), flat.cells.values.size());
	const int side = graded.cells.side;

	// Each change is near a path, and no water changes. On steep ground the change tapers slowly, so it can go 24 corners
	// from the path.
	constexpr int k_Near = 24;
	Grid<uint8_t> nearPath(side, 0);
	for (const auto& path : graded.paths)
	{
		for (const auto& point : path.points)
		{
			const int px = static_cast<int>(point.x);
			const int pz = static_cast<int>(point.z);
			for (int x = px - k_Near; x <= px + k_Near; ++x)
			{
				for (int z = pz - k_Near; z <= pz + k_Near; ++z)
				{
					if (nearPath.Contains(x, z))
					{
						nearPath.At(x, z) = 1;
					}
				}
			}
		}
	}
	size_t changed = 0;
	const auto metres = [&](const GeneratedLand& land, int x, int z) {
		return static_cast<float>(land.heightUnits.At(x, z)) * k_MetresPerUnit;
	};
	const auto slope = [&](const GeneratedLand& land, int x, int z) {
		const float gx = (metres(land, x + 1, z) - metres(land, x - 1, z)) / (2.0f * k_Corner);
		const float gz = (metres(land, x, z + 1) - metres(land, x, z - 1)) / (2.0f * k_Corner);
		return std::sqrt((gx * gx) + (gz * gz));
	};
	size_t newCliffs = 0;
	for (int x = 1; x + 1 < side; ++x)
	{
		for (int z = 1; z + 1 < side; ++z)
		{
			const auto& after = graded.cells.At(x, z);
			const auto& before = flat.cells.At(x, z);
			EXPECT_EQ(after.properties.hasWater, before.properties.hasWater);
			if (graded.heightUnits.At(x, z) != flat.heightUnits.At(x, z))
			{
				++changed;
				EXPECT_NE(nearPath.At(x, z), 0) << x << " " << z;
			}
			if (slope(graded, x, z) >= 0.8391f && slope(flat, x, z) < 0.8391f)
			{
				++newCliffs;
			}
		}
	}
	EXPECT_GT(changed, 100u);
	EXPECT_EQ(newCliffs, 0u);
}

TEST(LandgenRoadBed, ABedBesideAShoreIsLevel)
{
	const roads::Settings settings;
	SideSlope land(0.5f);
	const int middle = k_Side / 2;
	// The water of a lake starts two corners from the middle of the path, on the low side
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z <= middle - 2; ++z)
		{
			land.fixed.At(x, z) = roads::k_Fixed;
		}
	}
	const auto path = StraightPath();
	const auto ground = land.GroundOf();
	const auto bed = BedOf(std::span(&path, 1), ground, settings);
	const auto graded = Grade(bed, ground, settings);
	for (int x = 12; x < k_Side - 12; ++x)
	{
		ASSERT_NE(bed.covered.At(x, middle - 1), 0);
		EXPECT_FLOAT_EQ(graded.At(x, middle - 1), bed.height.At(x, middle - 1));
		EXPECT_NEAR(graded.At(x, middle - 1), graded.At(x, middle), 0.01f);
		EXPECT_EQ(graded.At(x, middle - 2), land.metres.At(x, middle - 2));
	}
}

TEST(LandgenRoadBed, ASecondPathKeepsTheBedOfTheFirstPathWhereTheyMeet)
{
	const roads::Settings settings;
	SideSlope land(0.1f);
	const int middle = k_Side / 2;
	// The second path climbs the slope and meets the first path at its middle
	paths::Path second {.link = {.from = 1, .to = 2}, .points = {}};
	for (int z = 4; z <= middle; ++z)
	{
		second.points.push_back({.x = static_cast<float>(middle) - 0.5f, .z = static_cast<float>(z) - 0.5f});
	}
	const std::array<paths::Path, 1> first {StraightPath()};
	const std::array<paths::Path, 2> both {StraightPath(), second};
	const auto ground = land.GroundOf();
	const auto alone = BedOf(first, ground, settings);
	const auto joined = BedOf(both, ground, settings);
	for (size_t i = 0; i < alone.covered.values.size(); ++i)
	{
		if (alone.covered.values[i] != 0)
		{
			EXPECT_EQ(joined.height.values[i], alone.height.values[i]);
		}
	}
	// The second bed comes to the first bed with no step
	EXPECT_NEAR(joined.height.At(middle, middle - 3), alone.height.At(middle, middle - 1), 0.2f * k_Corner * 2.0f + 0.01f);
}

TEST(LandgenRoadBed, TheBedLeavesARingWithTheGradeOfTheBed)
{
	const roads::Settings settings;
	std::vector<float> land(100, 10.0f);
	std::vector<uint8_t> fixed(100, 0);
	// The ring of a town on flat ground, then a climb of 2 in 5 away from the town to a flat top
	for (size_t i = 0; i < land.size(); ++i)
	{
		fixed[i] = i < 20 ? roads::k_Ring : 0;
		const auto climb = static_cast<float>(std::clamp<size_t>(i, 19, 60) - 19);
		land[i] = 10.0f + (0.4f * settings.step * climb);
	}
	const auto bed = Profile(land, fixed, settings);
	EXPECT_EQ(bed[19], land[19]);
	// For the first 40 m out of the ring, no step of the bed is steeper than the grade of the bed
	const std::vector<float> first(bed.begin() + 19, bed.begin() + 28);
	EXPECT_LE(SteepestOf(first, settings.step), settings.ringGrade + 0.001f);
	// A fixed sample of another kind lets the bed come back more steeply
	auto river = fixed;
	std::ranges::replace(river, roads::k_Ring, roads::k_Fixed);
	const auto other = Profile(land, river, settings);
	const std::vector<float> otherFirst(other.begin() + 19, other.begin() + 28);
	EXPECT_GT(SteepestOf(otherFirst, settings.step), settings.ringGrade + 0.01f);
}

TEST(LandgenRoadBed, TheChangeNextToARingIsAShortBlend)
{
	// A climb of 1 in 2 along the path, from the ring of a town at the low end to a flat top
	Grid<float> metres(k_Side, 0.0f);
	Grid<uint8_t> fixed(k_Side, 0);
	const Grid<float> lowest(k_Side, -1000.0f);
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			metres.At(x, z) = 0.5f * k_Corner * static_cast<float>(std::min(x, 22));
			fixed.At(x, z) = x <= 10 ? roads::k_Ring : 0;
		}
	}
	const Ground ground {.metres = metres, .fixed = fixed, .lowest = lowest};
	const std::array<paths::Path, 1> paths {StraightPath()};
	roads::Settings settings;
	const auto graded = Grade(BedOf(paths, ground, settings), ground, settings);
	const int middle = k_Side / 2;
	// Two corners from the ring, the cut is deeper than the taper lets it be, and no deeper than the ring taper
	const float cut = metres.At(12, middle) - graded.At(12, middle);
	EXPECT_GT(cut, (settings.taper * k_Corner * 2.0f) + 0.5f);
	EXPECT_LE(cut, (settings.ringTaper * k_Corner * 2.0f) + 0.001f);
	EXPECT_EQ(graded.At(10, middle), metres.At(10, middle));
	settings.ringTaper = settings.taper;
	const auto tapered = Grade(BedOf(paths, ground, settings), ground, settings);
	EXPECT_LE(metres.At(12, middle) - tapered.At(12, middle), (settings.taper * k_Corner * 2.0f) + 0.001f);
}

TEST(LandgenRoadBed, TheBedIsLevelOverTheFullWidthOfAPathOnTheCornerLines)
{
	const roads::Settings settings;
	const SideSlope land(0.25f);
	// A generated path has its points on the corners. Its drawn middle is then half a corner from the corners.
	paths::Path path {.link = {.from = 0, .to = 1}, .points = {}};
	const int middle = k_Side / 2;
	for (int x = 2; x < k_Side - 2; ++x)
	{
		path.points.push_back({.x = static_cast<float>(x), .z = static_cast<float>(middle)});
	}
	const auto ground = land.GroundOf();
	const auto bed = BedOf(std::span(&path, 1), ground, settings);
	const auto graded = Grade(bed, ground, settings);
	const int x = k_Side / 2;
	// Four corners across the path are level: the two corners of the drawn path and one more corner on each side
	for (int z = middle - 1; z <= middle + 2; ++z)
	{
		EXPECT_NE(bed.covered.At(x, z), 0) << z;
		EXPECT_NEAR(graded.At(x, z), graded.At(x, middle), 0.01f) << z;
	}
	// The banks have the fixed slope next to the bed
	EXPECT_NEAR(graded.At(x, middle + 3) - graded.At(x, middle + 2), settings.cutSlope * k_Corner, 0.01f);
	EXPECT_NEAR(graded.At(x, middle - 1) - graded.At(x, middle - 2), settings.fillSlope * k_Corner, 0.01f);
}

TEST(LandgenRoadBed, InAValleyTheBedMovesAwayFromTheSteepSide)
{
	roads::Settings settings;
	const int middle = k_Side / 2;
	// A valley: a gentle side at 1 in 10 and a steep side at 7 in 10, which is too steep for the bank
	Grid<float> metres(k_Side, 0.0f);
	const Grid<uint8_t> fixed(k_Side, 0);
	const Grid<float> lowest(k_Side, -1000.0f);
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = 0; z < k_Side; ++z)
		{
			const float run = static_cast<float>(z - middle) * k_Corner;
			metres.At(x, z) = 100.0f + (z < middle ? -0.1f * run : 0.7f * run);
		}
	}
	const Ground ground {.metres = metres, .fixed = fixed, .lowest = lowest};
	const auto path = StraightPath();
	const auto bed = BedOf(std::span(&path, 1), ground, settings);
	const auto graded = Grade(bed, ground, settings);
	const int x = k_Side / 2;
	// The bed covers the gentle side and is level there. The steep side keeps nearly its height.
	EXPECT_EQ(bed.covered.At(x, middle + 1), 0);
	EXPECT_NE(bed.covered.At(x, middle - 2), 0);
	EXPECT_NEAR(graded.At(x, middle - 2), graded.At(x, middle), 0.01f);
	EXPECT_NEAR(graded.At(x, middle - 1), graded.At(x, middle), 0.01f);
	EXPECT_LE(std::fabs(graded.At(x, middle + 1) - metres.At(x, middle + 1)), 1.5f);

	// When the bed does not move, it cuts deeply into the steep side
	settings.narrowSide = 10.0f;
	settings.cliffSlope = 10.0f;
	const auto still = BedOf(std::span(&path, 1), ground, settings);
	EXPECT_NE(still.covered.At(x, middle + 1), 0);
	EXPECT_GT(metres.At(x, middle + 1) - Grade(still, ground, settings).At(x, middle + 1), 4.0f);
}

TEST(LandgenRoadBed, TheBanksOnASideSteeperThanTheBankMakeNoNewCliff)
{
	const roads::Settings settings;
	// The land is steeper than the bank, but less steep than a cliff
	const SideSlope land(0.6f);
	const auto path = StraightPath();
	const auto ground = land.GroundOf();
	const auto graded = Grade(BedOf(std::span(&path, 1), ground, settings), ground, settings);
	const int middle = k_Side / 2;
	const int x = k_Side / 2;
	EXPECT_NEAR(graded.At(x, middle + 1), graded.At(x, middle), 0.01f);
	EXPECT_NEAR(graded.At(x, middle - 1), graded.At(x, middle), 0.01f);
	// The cut and the fill go on until they meet the land
	EXPECT_LT(graded.At(x, middle + 3), land.metres.At(x, middle + 3));
	EXPECT_GT(graded.At(x, middle - 3), land.metres.At(x, middle - 3));
	EXPECT_EQ(graded.At(x, 2), land.metres.At(x, 2));
	EXPECT_EQ(graded.At(x, k_Side - 3), land.metres.At(x, k_Side - 3));
	// No slope between two corners next to each other is as steep as a cliff
	EXPECT_LT(SteepestCornerOf(graded), settings.cliffSlope);
}

TEST(LandgenRoadBed, ALowCliffNextToThePathGetsABankOfTheFixedSlope)
{
	const roads::Settings settings;
	SideSlope land(0.0f);
	const int middle = k_Side / 2;
	// A cliff of 15 m starts two rows from the middle of the path, with flat ground on its top
	for (int x = 0; x < k_Side; ++x)
	{
		for (int z = middle + 2; z < k_Side; ++z)
		{
			land.metres.At(x, z) = 115.0f;
		}
	}
	const auto path = StraightPath();
	const auto ground = land.GroundOf();
	const auto graded = Grade(BedOf(std::span(&path, 1), ground, settings), ground, settings);
	const int x = k_Side / 2;
	// The bank rises from the foot of the cliff with the fixed slope until it meets the top of the cliff
	EXPECT_NEAR(graded.At(x, middle + 1), 100.0f, 0.01f);
	for (int z = middle + 2; z <= middle + 4; ++z)
	{
		EXPECT_NEAR(graded.At(x, z) - graded.At(x, z - 1), settings.cutSlope * k_Corner, 0.01f) << z;
	}
	EXPECT_EQ(graded.At(x, middle + 5), 115.0f);
	EXPECT_LE(SteepestCornerOf(graded), settings.cutSlope + 0.001f);

	// A cliff higher than the deepest bank keeps its face
	SideSlope high(0.0f);
	for (int gx = 0; gx < k_Side; ++gx)
	{
		for (int z = middle + 2; z < k_Side; ++z)
		{
			high.metres.At(gx, z) = 160.0f;
		}
	}
	const auto highGround = high.GroundOf();
	const auto kept = Grade(BedOf(std::span(&path, 1), highGround, settings), highGround, settings);
	EXPECT_EQ(kept.At(x, middle + 3), 160.0f);
}
