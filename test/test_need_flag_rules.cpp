/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/NeedFlagRules.h"
#include "ECS/WorkshopRules.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{

/// The row of the wood flag: full height at a need of 1, shows for any need, 3.9 metres high
constexpr need_flag::Row k_Wood {.maxNeed = 1.0f, .showsAfter = 0.0f, .maxHeight = 3.9f};

} // namespace

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(NeedFlag, AWorkshopWantsTheWoodOfAScaffoldThatItDoesNotHold)
{
	EXPECT_NEAR(workshop::WoodWant(0, 200), 1.0f, 1e-6f);
	EXPECT_NEAR(workshop::WoodWant(50, 200), 0.75f, 1e-6f);
	EXPECT_FLOAT_EQ(workshop::WoodWant(200, 200), 0.0f);
	// More wood than a scaffold needs does not make the want less than none
	EXPECT_FLOAT_EQ(workshop::WoodWant(900, 200), 0.0f);
	// A scaffold that needs no wood: the small amount keeps the division safe
	EXPECT_FLOAT_EQ(workshop::WoodWant(0, 0), 0.0f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(NeedFlag, AWorkshopRequiresTheWoodOfTheScaffoldsThatItHasRoomFor)
{
	EXPECT_EQ(workshop::WoodRequired(3, 0, 200), 600u);
	EXPECT_EQ(workshop::WoodRequired(2, 150, 200), 250u);
	EXPECT_EQ(workshop::WoodRequired(1, 200, 200), 0u);
	// No room: no wood required, whatever the workshop holds
	EXPECT_EQ(workshop::WoodRequired(0, 0, 200), 0u);
	EXPECT_EQ(workshop::WoodRequired(-1, 0, 200), 0u);
	// More wood than the room needs goes below zero and wraps, as the game counts it
	EXPECT_EQ(workshop::WoodRequired(1, 201, 200), 0xFFFFFFFFu);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(NeedFlag, AFlagShowsOnlyForANeedMoreThanTheLeastOfItsRow)
{
	EXPECT_FALSE(need_flag::Shows(0.0f, k_Wood));
	EXPECT_TRUE(need_flag::Shows(0.01f, k_Wood));
	const need_flag::Row quarter {.maxNeed = 1.0f, .showsAfter = 0.25f};
	EXPECT_FALSE(need_flag::Shows(0.25f, quarter));
	EXPECT_TRUE(need_flag::Shows(0.26f, quarter));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(NeedFlag, AFlagGoesNoHigherThanItsFullHeight)
{
	EXPECT_FLOAT_EQ(need_flag::Fill(0.5f, k_Wood), 0.5f);
	EXPECT_FLOAT_EQ(need_flag::Fill(2.0f, k_Wood), 1.0f);
	EXPECT_FLOAT_EQ(need_flag::Fill(1.0f, {.maxNeed = 2.0f}), 0.5f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(NeedFlag, ANewFlagGrowsToItsFullSizeOverTheFirstFifthOfItsRoom)
{
	EXPECT_FLOAT_EQ(need_flag::DrawnScale(0.0f, k_Wood), 0.0f);
	EXPECT_FLOAT_EQ(need_flag::DrawnScale(0.1f, k_Wood), 0.5f);
	EXPECT_FLOAT_EQ(need_flag::DrawnScale(0.2f, k_Wood), 1.0f);
	EXPECT_FLOAT_EQ(need_flag::DrawnScale(0.9f, k_Wood), 1.0f);
	const need_flag::Row quarter {.maxNeed = 1.0f, .showsAfter = 0.1f};
	// The room is 0.9 and its fifth ends at 0.18: the flag shows from a share of 0.1
	EXPECT_NEAR(need_flag::DrawnScale(0.14f, quarter), 0.5f, 1e-5f);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(NeedFlag, TheFlagFliesAtTheSpecialPointMovedByTheBuildingOnTheMap)
{
	const auto building = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 7.0f, 200.0f)), glm::radians(90.0f),
	                                  glm::vec3(0.0f, 1.0f, 0.0f));
	const auto place = need_flag::Place({3.0f, 5.0f, 0.0f}, building);
	// A quarter turn about the upright takes the point's x to -z
	EXPECT_NEAR(place.x, 100.0f, 1e-3f);
	EXPECT_NEAR(place.y, 197.0f, 1e-3f);
	// The place is a map position
	EXPECT_EQ(place.x, map_coords::Quantise(place.x));
	EXPECT_EQ(place.y, map_coords::Quantise(place.y));
}
