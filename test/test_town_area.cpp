/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

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
