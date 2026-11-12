/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GUtilsAngle.h"
#include "ECS/TownAreaRules.h"

using namespace openblack;
using namespace openblack::ecs;

TEST(TownArea, BuildingsWidenTheGroundByTheirRadius)
{
	auto area = town_area::Emptied();
	town_area::Widen(area, map_coords::FromMetres({100.0f, 200.0f}), 5.0f);
	town_area::Widen(area, map_coords::FromMetres({140.0f, 210.0f}), 10.0f);
	EXPECT_NEAR(map_coords::ToMetres(area.min.x), 95.0f, 0.001f);
	EXPECT_NEAR(map_coords::ToMetres(area.min.y), 195.0f, 0.001f);
	EXPECT_NEAR(map_coords::ToMetres(area.max.x), 150.0f, 0.001f);
	EXPECT_NEAR(map_coords::ToMetres(area.max.y), 220.0f, 0.001f);
	// The middle of the box, and half its longer side cut to whole metres
	const auto centre = map_coords::ToMetres(town_area::Centre(area));
	EXPECT_NEAR(centre.x, 122.5f, 0.001f);
	EXPECT_NEAR(centre.y, 207.5f, 0.001f);
	EXPECT_FLOAT_EQ(town_area::Radius(area), 27.5f);
}

TEST(TownArea, ATownWithNothingInItHasAnEmptyBox)
{
	const auto area = town_area::Emptied();
	EXPECT_GT(area.min.x, area.max.x);
	// A new town that never had a building keeps a box at the corner of the map
	const components::TownArea fresh;
	EXPECT_EQ(fresh.min, glm::ivec2(0, 0));
	EXPECT_FLOAT_EQ(town_area::Radius(fresh), 0.0f);
}

TEST(TownArea, AScriptGivesAGatheringPlaceAtItsMapPosition)
{
	// The place is kept in the whole map units of the text
	const auto place = map_coords::ScriptPlace(2208.89, 2978.04);
	const auto gathering = town_area::GatheringPlaceFromScript(place);
	ASSERT_TRUE(gathering.has_value());
	EXPECT_EQ(*gathering, place);
}

TEST(TownArea, AKeptGatheringPlaceAtTheCornerWithNoAltitudeIsFoundAgain)
{
	EXPECT_FALSE(town_area::IsGatheringPlace({.x = 0, .z = 0, .altitude = 0.0f}));
	EXPECT_TRUE(town_area::IsGatheringPlace({.x = 0, .z = 0, .altitude = 1.0f}));
	EXPECT_TRUE(town_area::IsGatheringPlace({.x = 1, .z = 0, .altitude = 0.0f}));
	EXPECT_TRUE(town_area::IsGatheringPlace({.x = 0, .z = 1, .altitude = 0.0f}));
}

TEST(TownArea, AScriptPlaceAtTheCornerOfTheMapIsNoGatheringPlace)
{
	// The town then finds its own gathering place when it first needs one
	EXPECT_FALSE(town_area::GatheringPlaceFromScript(map_coords::ScriptPlace(0.0, 0.0)).has_value());
	EXPECT_TRUE(town_area::GatheringPlaceFromScript(map_coords::ScriptPlace(0.0, 1.0)).has_value());
}

TEST(TownArea, TheGatheringMiddleRoundsTheQuotientOneTime)
{
	// The sum of the places is larger than a float holds exactly. The game divides the whole sum and rounds the quotient
	// one time: 57266109 / 3 = 19088703, rounded to 19088704. A float of the sum first gives 19088702
	const std::array<map_coords::MapCoords, 3> buildings {
	    map_coords::MapCoords {.x = 18054722, .z = 1000},
	    map_coords::MapCoords {.x = 21533673, .z = 2000},
	    map_coords::MapCoords {.x = 17677714, .z = 3000},
	};
	const auto middle = town_area::GatheringMiddle(buildings);
	EXPECT_EQ(middle.x, 19088704);
	EXPECT_EQ(middle.z, 2000);
}

TEST(TownArea, TheGatheringMiddleCountsTheRingAgainPastAHundredBuildings)
{
	// Building 101 takes the place of building 1 in the ring, and the sum reads place 1 again as building 101
	std::vector<map_coords::MapCoords> buildings;
	for (int32_t i = 1; i <= 101; ++i)
	{
		buildings.push_back({.x = 1000 * i, .z = 0});
	}
	// (101000 x 2 + 2000 + 3000 + ... + 100000) / 101 = 5251000 / 101
	EXPECT_EQ(town_area::GatheringMiddle(buildings).x, 51990);
	// With no more buildings than the ring, it is the plain middle
	buildings.pop_back();
	EXPECT_EQ(town_area::GatheringMiddle(buildings).x, 50500);
}

TEST(TownArea, AGatheringPlaceWithNoClearAreaIsAStepFromItsBaseInWholeMapUnits)
{
	// The distance is drawn first, then the angle
	std::vector<float> limits;
	const auto random = [&limits](float limit) {
		limits.push_back(limit);
		return limits.size() == 1 ? 2.5f : 1.0f;
	};
	const map_coords::MapCoords base {.x = 13107203, .z = 6553607, .altitude = 0.0f};
	const auto place = town_area::GatheringPlaceNear(base, random);
	ASSERT_EQ(limits.size(), 2u);
	EXPECT_FLOAT_EQ(limits[0], 10.0f);
	EXPECT_FLOAT_EQ(limits[1], 6.2831855f);
	const auto step = gutils::GetPosFromAngle(1.0f, 12.5f);
	EXPECT_EQ(place.x, base.x + step.x);
	EXPECT_EQ(place.z, base.z + step.z);
}
