/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Hand/HandFeel.h"

namespace hand_feel = openblack::hand_feel;

TEST(HandFeel, VillagersAndAnimalsAreHoveredBeforeAndModelsFelt)
{
	using hand_feel::Feel;
	EXPECT_EQ(hand_feel::FeelOf({.living = true}), Feel::AtPosition);
	EXPECT_EQ(hand_feel::FeelOf({.living = true, .creature = true, .posedModel = true}), Feel::OnModel);
	EXPECT_EQ(hand_feel::FeelOf({}), Feel::OnModel);
	EXPECT_EQ(hand_feel::FeelOf({.posedModel = true}), Feel::AtPosition);
	EXPECT_EQ(hand_feel::FeelOf({.posedModel = true, .animatedStatic = true}), Feel::OnModel);
	EXPECT_EQ(hand_feel::FeelOf({.living = true, .ignored = true}), Feel::Nothing);
}

TEST(HandFeel, HoldingSomethingTheHandFeelsTowardsTheRaisedGround)
{
	const glm::vec3 camera(0.0f, 10.0f, 0.0f);
	const glm::vec3 cursor = glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f));
	EXPECT_EQ(hand_feel::FeelDirection(camera, cursor, std::nullopt), cursor);
	const auto towards = hand_feel::FeelDirection(camera, cursor, glm::vec3(10.0f, 5.0f, 0.0f));
	EXPECT_NEAR(towards.x, glm::normalize(glm::vec3(10.0f, -5.0f, 0.0f)).x, 1e-6f);
	EXPECT_NEAR(glm::length(towards), 1.0f, 1e-6f);
}

TEST(HandFeel, TheHandStandsOffTheFaceLeaningToTheCamera)
{
	const glm::vec3 camera(0.0f, 0.0f, 0.0f);
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	// A wall facing the camera, met 10 ahead, its normal turned along the line
	const auto rest =
	    hand_feel::RestOnModel(camera, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -2.0f}, true, std::nullopt);
	EXPECT_EQ(rest.point, glm::vec3(0.0f, 0.0f, -10.0f));
	ASSERT_TRUE(rest.up.has_value());
	// A quarter towards the camera, off the face, and half up
	EXPECT_NEAR(rest.up->x, 0.0f, 1e-6f);
	EXPECT_NEAR(rest.up->y, 0.5f, 1e-6f);
	EXPECT_NEAR(rest.up->z, 1.25f, 1e-6f);
	// A model not felt keeps the point but turns the hand to the land
	EXPECT_FALSE(
	    hand_feel::RestOnModel(camera, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -1.0f}, false, std::nullopt)
	        .up.has_value());
}

TEST(HandFeel, AFaceNearerThanAUnitPutsTheHandAUnitAlongTheLine)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	const auto rest =
	    hand_feel::RestOnModel({}, direction, direction, {0.0f, 0.0f, -0.5f}, {0.0f, 0.0f, -1.0f}, true, std::nullopt);
	EXPECT_EQ(rest.point, glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_TRUE(rest.up.has_value());
	EXPECT_NEAR(rest.up->z, 0.25f + 1.0f, 1e-6f);
}

TEST(HandFeel, HoldingSomethingTheHandIsKeptOutFromTheFace)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	// Kept out by half the held thing's radius, and by how deep in a creature's reach the point is
	const auto rest = hand_feel::RestOnModel({}, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -1.0f}, true,
	                                         hand_feel::Holding {.radius = 2.0f, .creature = std::nullopt});
	EXPECT_NEAR(rest.point.z, -9.0f, 1e-5f);
	const auto atCreature = hand_feel::RestOnModel(
	    {}, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -1.0f}, true,
	    hand_feel::Holding {.radius = 2.0f, .creature = hand_feel::CreatureReach {.centre = {0.0f, -11.0f}, .radius = 4.0f}});
	EXPECT_NEAR(atCreature.point.z, -6.0f, 1e-5f);
}

TEST(HandFeel, AVillagerIsHoveredBeforeAtItsDistanceLessItsRadius)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	const auto point = hand_feel::RestAtPosition({}, direction, direction, {0.0f, 0.0f, -20.0f}, 2.0f);
	EXPECT_NEAR(point.z, -18.0f, 1e-5f);
}

TEST(HandFeel, TheHandTurnsToTheLandOffAFaceOrLowOverAThing)
{
	EXPECT_TRUE(hand_feel::TurnsToLand(false, false, 10.0f, 3.2f));
	EXPECT_FALSE(hand_feel::TurnsToLand(true, true, 10.0f, 3.2f));
	EXPECT_TRUE(hand_feel::TurnsToLand(true, true, 1.0f, 3.2f));
	EXPECT_FALSE(hand_feel::TurnsToLand(true, false, 1.0f, 3.2f));
}

TEST(HandFeel, OverTheLandTheHandEasesToTheLandLessItsHeight)
{
	const auto hover = hand_feel::HoverDistancesOf(
	    {.camera = {}, .land = glm::vec3(0.0f, 0.0f, -50.0f), .handHeight = 3.2f, .minDistance = 2.0f, .reach = 1000.0f});
	EXPECT_NEAR(hover.limit, 46.8f, 1e-4f);
	EXPECT_NEAR(hover.target, 46.8f, 1e-4f);
}

TEST(HandFeel, OnTheSeaTheHandRestsOnTheWater)
{
	const auto hover = hand_feel::HoverDistancesOf({.camera = {},
	                                                .land = glm::vec3(0.0f, 0.0f, -50.0f),
	                                                .landIsSea = true,
	                                                .handHeight = 3.2f,
	                                                .minDistance = 2.0f,
	                                                .reach = 1000.0f});
	EXPECT_NEAR(hover.limit, 50.0f, 1e-4f);
}

TEST(HandFeel, OverAThingTheHandEasesToWhereItRestsOnIt)
{
	// A creature's back 30 along the line, the land behind it 50: the hand goes to the back, not pulled back by its
	// height
	const auto hover = hand_feel::HoverDistancesOf({.camera = {},
	                                                .land = glm::vec3(0.0f, 0.0f, -50.0f),
	                                                .restOnThing = glm::vec3(0.0f, 0.0f, -30.0f),
	                                                .handHeight = 3.2f,
	                                                .minDistance = 2.0f,
	                                                .reach = 1000.0f});
	EXPECT_NEAR(hover.target, 30.0f, 1e-4f);
	EXPECT_NEAR(hover.limit, 46.8f, 1e-4f);
	// Grazing a thing nearly as far as the land, the land less the hand's height still limits it
	const auto grazing = hand_feel::HoverDistancesOf({.camera = {},
	                                                  .land = glm::vec3(0.0f, 0.0f, -50.0f),
	                                                  .restOnThing = glm::vec3(0.0f, 0.0f, -49.0f),
	                                                  .handHeight = 3.2f,
	                                                  .minDistance = 2.0f,
	                                                  .reach = 1000.0f});
	EXPECT_LT(grazing.limit, grazing.target);
}

TEST(HandFeel, TheHandKeepsItsDistanceUnderTheSkyAndStaysWithinItsReach)
{
	const auto sky =
	    hand_feel::HoverDistancesOf({.camera = {}, .currentDistance = 25.0f, .minDistance = 2.0f, .reach = 1000.0f});
	EXPECT_EQ(sky.limit, 25.0f);
	EXPECT_EQ(sky.target, 25.0f);
	const auto beyondReach = hand_feel::HoverDistancesOf(
	    {.camera = {}, .land = glm::vec3(0.0f, 0.0f, -500.0f), .handHeight = 3.2f, .minDistance = 2.0f, .reach = 300.0f});
	EXPECT_EQ(beyondReach.limit, 300.0f);
	const auto closeLand = hand_feel::HoverDistancesOf(
	    {.camera = {}, .land = glm::vec3(0.0f, 0.0f, -3.0f), .handHeight = 3.2f, .minDistance = 2.0f, .reach = 300.0f});
	EXPECT_EQ(closeLand.limit, 2.0f);
	// A thing right at the camera is eased to no nearer than a unit
	const auto close = hand_feel::HoverDistancesOf({.camera = {},
	                                                .land = glm::vec3(0.0f, 0.0f, -50.0f),
	                                                .restOnThing = glm::vec3(0.0f, 0.0f, -0.25f),
	                                                .handHeight = 3.2f,
	                                                .minDistance = 2.0f,
	                                                .reach = 300.0f});
	EXPECT_EQ(close.target, 1.0f);
}
