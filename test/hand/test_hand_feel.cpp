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
using openblack::HoldType;
using openblack::Zoomer;

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
	const auto towards =
	    hand_feel::FeelDirection(camera, cursor, hand_feel::RaisedGround {.ground = {10.0f, 0.0f, 0.0f}, .lift = 5.0f});
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

TEST(HandFeel, HoldingSomethingTheHandGoesAllTheWayToTheLand)
{
	const auto hover = hand_feel::HoverDistancesOf({.camera = {},
	                                                .land = glm::vec3(0.0f, 0.0f, -50.0f),
	                                                .holding = true,
	                                                .handHeight = 3.2f,
	                                                .minDistance = 2.0f,
	                                                .reach = 1000.0f});
	EXPECT_NEAR(hover.limit, 50.0f, 1e-4f);
	EXPECT_NEAR(hover.target, 50.0f, 1e-4f);
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

TEST(HandFeel, TheHoverEasesOutSlowlyAndInQuickly)
{
	// Re-aimed every frame, it eases on towards the distance aimed at, faster towards the camera than away from it
	Zoomer out(100.0f);
	Zoomer in(100.0f);
	for (int frame = 0; frame < 6; ++frame)
	{
		hand_feel::EaseHover(out, 150.0f, 0.016f, false);
		hand_feel::EaseHover(in, 50.0f, 0.016f, false);
	}
	EXPECT_GT(out.GetValue(), 100.0f);
	EXPECT_LT(out.GetValue(), 150.0f);
	EXPECT_LT(in.GetValue(), 100.0f);
	EXPECT_GT(in.GetValue(), 50.0f);
	EXPECT_GT(100.0f - in.GetValue(), out.GetValue() - 100.0f);

	// Never nearer than a unit
	Zoomer close(3.0f);
	hand_feel::EaseHover(close, 0.0f, 0.2f, false);
	EXPECT_FLOAT_EQ(close.GetValue(), 1.0f);
}

TEST(HandFeel, PullingAThingOutOfTheGroundLeavesTheHoverWhereItWas)
{
	// Looking down on a tree, the hand hovered on its crown, 110 from the camera, when it took hold of it. While it
	// pulls, the cursor is over the land 150 away, or over the leaning crown nearer the camera.
	Zoomer hover(110.0f);
	for (int frame = 0; frame < 60; ++frame)
	{
		hand_feel::EaseHover(hover, frame % 2 == 0 ? 150.0f : 95.0f, 0.016f, true);
		ASSERT_FLOAT_EQ(hover.GetValue(), 110.0f);
	}
	// Free, it eases on from where it hovered when it took hold
	hand_feel::EaseHover(hover, 150.0f, 0.016f, false);
	EXPECT_GT(hover.GetValue(), 110.0f);
	EXPECT_LT(hover.GetValue(), 112.0f);
}

TEST(HandFeel, AMiraclesIconIsRestedNearerByTheHandsHeight)
{
	EXPECT_EQ(hand_feel::FeelOf({.spellIcon = true}), hand_feel::Feel::AtIcon);
	// What the hand holds is still no thing to it
	EXPECT_EQ(hand_feel::FeelOf({.ignored = true, .spellIcon = true}), hand_feel::Feel::Nothing);
	// An icon 40 along the line: the hand eases to 3.2 nearer, so it does not go into the icon
	const auto hover = hand_feel::HoverDistancesOf({.camera = {},
	                                                .land = glm::vec3(0.0f, 0.0f, -60.0f),
	                                                .restOnThing = glm::vec3(0.0f, 0.0f, -40.0f),
	                                                .restNearer = hand_feel::k_IconGap,
	                                                .handHeight = 3.2f,
	                                                .minDistance = 2.0f,
	                                                .reach = 1000.0f});
	EXPECT_EQ(hover.target, 40.0f - 3.2f);
	// An icon nearer than the gap still keeps the hand a unit out
	const auto close = hand_feel::HoverDistancesOf({.camera = {},
	                                                .land = glm::vec3(0.0f, 0.0f, -60.0f),
	                                                .restOnThing = glm::vec3(0.0f, 0.0f, -2.0f),
	                                                .restNearer = hand_feel::k_IconGap,
	                                                .handHeight = 3.2f,
	                                                .minDistance = 2.0f,
	                                                .reach = 1000.0f});
	EXPECT_EQ(close.target, 1.0f);
}

TEST(HandFeel, OverTheHelpsTextBubbleTheHandEasesToTheBubble)
{
	const auto hover = hand_feel::HoverDistancesOf({.camera = {},
	                                                .land = glm::vec3(0.0f, 0.0f, -60.0f),
	                                                .bubbleDistance = 12.5f,
	                                                .cursorPoint = glm::vec3(0.0f, 0.0f, -56.8f),
	                                                .handHeight = 3.2f,
	                                                .minDistance = 2.0f,
	                                                .reach = 1000.0f});
	EXPECT_EQ(hover.target, 12.5f);
	// The land still limits how far it may be
	EXPECT_NEAR(hover.limit, 56.8f, 1e-4f);
	const auto touching = hand_feel::HoverDistancesOf({.camera = {}, .bubbleDistance = 0.5f, .currentDistance = 20.0f});
	EXPECT_EQ(touching.target, 1.0f);
}

TEST(HandFeel, WatchingAFightOrOfferingTheHoverEasesOutOverTwoSeconds)
{
	Zoomer usual(100.0f);
	Zoomer slow(100.0f);
	hand_feel::EaseHover(usual, 150.0f, 0.016f, false, false);
	hand_feel::EaseHover(slow, 150.0f, 0.016f, false, true);
	EXPECT_FLOAT_EQ(usual.GetDuration(), hand_feel::k_HoverEaseOutSeconds);
	EXPECT_FLOAT_EQ(slow.GetDuration(), hand_feel::k_HoverSlowEaseOutSeconds);
	EXPECT_LT(slow.GetValue() - 100.0f, usual.GetValue() - 100.0f);
	// Towards the camera it eases in quickly all the same
	Zoomer in(100.0f);
	hand_feel::EaseHover(in, 50.0f, 0.016f, false, true);
	EXPECT_FLOAT_EQ(in.GetDuration(), hand_feel::k_HoverEaseInSeconds);
	// Already moving in at a unit a second or faster, it eases out at the usual time
	Zoomer fast(100.0f);
	fast.Carry(100.0f, -1.0f);
	hand_feel::EaseHover(fast, 150.0f, 0.016f, false, true);
	EXPECT_FLOAT_EQ(fast.GetDuration(), hand_feel::k_HoverEaseOutSeconds);
	Zoomer drifting(100.0f);
	drifting.Carry(100.0f, -0.5f);
	hand_feel::EaseHover(drifting, 150.0f, 0.016f, false, true);
	EXPECT_FLOAT_EQ(drifting.GetDuration(), hand_feel::k_HoverSlowEaseOutSeconds);
}

TEST(HandFeel, OnlyAThingHeldOtherwiseThanByMagicAndNotOfferedReachesTheLand)
{
	EXPECT_TRUE(hand_feel::HoldReachesLand(HoldType::Above, false));
	EXPECT_TRUE(hand_feel::HoldReachesLand(HoldType::Side, false));
	EXPECT_FALSE(hand_feel::HoldReachesLand(HoldType::Magic, false));
	EXPECT_FALSE(hand_feel::HoldReachesLand(HoldType::Above, true));
	EXPECT_FALSE(hand_feel::HoldReachesLand(std::nullopt, false));
}
