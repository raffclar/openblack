/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "ECS/AbodeSurroundings.h"
#include "ECS/TownCentreParts.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The points of the Celtic crèche's model: the first of the sign's kind, the second of another kind
constexpr std::array<graphics::L3DMesh::EffectPoint, 2> k_CrechePoints {
    graphics::L3DMesh::EffectPoint {.position = {-3.679f, 0.0f, -6.613f}, .kind = 1},
    graphics::L3DMesh::EffectPoint {.position = {6.291f, 0.0f, -7.384f}, .kind = 2},
};
} // namespace

TEST(AbodeSurroundings, TheSignStandsAtTheFirstPointOfItsKind)
{
	const auto point = abode_surroundings::FindPoint(k_CrechePoints, abode_surroundings::k_TipSignPoint);
	ASSERT_TRUE(point.has_value());
	EXPECT_FLOAT_EQ(point->x, -3.679f);
	EXPECT_FLOAT_EQ(point->z, -6.613f);
}

TEST(AbodeSurroundings, AModelWithNoSignPointHasNoSign)
{
	EXPECT_FALSE(
	    abode_surroundings::FindPoint(std::span(k_CrechePoints).subspan(1), abode_surroundings::k_TipSignPoint).has_value());
	EXPECT_FALSE(abode_surroundings::FindPoint({}, abode_surroundings::k_TipSignPoint).has_value());
	// No model has a point of kind 0, where a lantern would stand
	EXPECT_FALSE(abode_surroundings::FindPoint(k_CrechePoints, 0).has_value());
}

TEST(AbodeSurroundings, OnlyABuildingWithATipHasASign)
{
	EXPECT_TRUE(abode_surroundings::HasTipSign(4139));
	EXPECT_FALSE(abode_surroundings::HasTipSign(0));
}

TEST(AbodeSurroundings, TheSignPointTurnsAndScalesWithTheBuilding)
{
	const auto placement = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 5.0f, 200.0f)) *
	                       glm::mat4(glm::mat3(glm::eulerAngleY(glm::radians(90.0f)))) *
	                       glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
	const auto placed = abode_surroundings::PlacePoint(placement, {1.0f, 0.0f, 0.0f});
	EXPECT_NEAR(placed.x, 100.0f, 1e-4f);
	EXPECT_NEAR(placed.y, 5.0f, 1e-4f);
	EXPECT_NEAR(placed.z, 198.0f, 1e-4f);
}

TEST(AbodeSurroundings, SignsGoByTheMapCellOfThePoint)
{
	EXPECT_TRUE(abode_surroundings::InSameCell({101.0f, 0.0f, 205.0f}, {109.9f, 30.0f, 200.0f}));
	EXPECT_FALSE(abode_surroundings::InSameCell({101.0f, 0.0f, 205.0f}, {110.0f, 0.0f, 205.0f}));
	EXPECT_FALSE(abode_surroundings::InSameCell({101.0f, 0.0f, 205.0f}, {101.0f, 0.0f, 199.9f}));
}

TEST(TownCentreParts, ATownCentreMadeUnbuiltHasNoPartsUntilFinished)
{
	// Its town puts it up again from nothing, or it is put up part built: no totem, hands or symbols
	EXPECT_FALSE(town_centre_parts::PartsWhenMade(0.0f));
	EXPECT_FALSE(town_centre_parts::PartsWhenMade(0.99f));
	EXPECT_FALSE(town_centre_parts::ShowsBeliefSymbols(0.0f));
	EXPECT_FALSE(town_centre_parts::ShowsBeliefSymbols(0.5f));
	// Its town finishes it: it gets them
	EXPECT_TRUE(town_centre_parts::PartsWhenFinished(true, false));
	EXPECT_TRUE(town_centre_parts::ShowsBeliefSymbols(std::nullopt));
}

TEST(TownCentreParts, ATownCentreMadeBuiltHasItsPartsAtOnce)
{
	EXPECT_TRUE(town_centre_parts::PartsWhenMade(1.0f));
	EXPECT_TRUE(town_centre_parts::ShowsBeliefSymbols(1.0f));
}

TEST(TownCentreParts, FinishingGivesPartsOnceAndOnlyToATownCentre)
{
	// A centre that has its totem already gets no second set; another building gets none
	EXPECT_FALSE(town_centre_parts::PartsWhenFinished(true, true));
	EXPECT_FALSE(town_centre_parts::PartsWhenFinished(false, false));
}
