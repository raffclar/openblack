/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules that move the camera over the world's ball

#include <cmath>

#include <algorithm>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/PlanetNavigation.h"

using namespace openblack;
using namespace openblack::planet_nav;

namespace
{
constexpr float k_Radius = 6046079.0f;
constexpr glm::vec2 k_Middle {2560.0f, 2560.0f};

Ball EarthBall()
{
	return WorldBall(0.0f, k_Middle, k_Radius);
}

// A flat sea at the height 0
std::optional<float> FlatSea(glm::vec3 eye, glm::vec3 direction)
{
	if (eye.y <= 0.0f || direction.y >= 0.0f)
	{
		return std::nullopt;
	}
	return eye.y / -direction.y;
}

// The direction of a pose's line of sight
glm::vec3 Forward(const Pose& pose)
{
	return glm::normalize(pose.focus - pose.eye);
}
} // namespace

TEST(PlanetNavigation, HeightOverTheBallKeepsSmallHeights)
{
	const auto ball = EarthBall();
	EXPECT_NEAR(HeightOver(ball, {2560.0f, 300.0f, 2560.0f}), 300.0f, 0.01f);
	EXPECT_NEAR(HeightOver(ball, {2560.0f, 1.0e6f, 2560.0f}), 1.0e6f, 1.0f);
	// Across the ground the sea goes down, so a point at the sea's level is over the ball
	EXPECT_GT(HeightOver(ball, {102560.0f, 0.0f, 2560.0f}), 800.0f);
}

TEST(PlanetNavigation, OrbitShareBlendsOverTheBand)
{
	EXPECT_EQ(OrbitShare(0.0f), 0.0f);
	EXPECT_EQ(OrbitShare(k_OrbitFrom), 0.0f);
	EXPECT_NEAR(OrbitShare(0.5f * (k_OrbitFrom + k_OrbitFull)), 0.5f, 1e-5f);
	EXPECT_EQ(OrbitShare(k_OrbitFull), 1.0f);
	EXPECT_EQ(OrbitShare(1.0e7f), 1.0f);
}

TEST(PlanetNavigation, RayMeetsTheBall)
{
	const auto ball = EarthBall();
	const glm::vec3 eye {2560.0f, 1000.0f, 2560.0f};
	const auto down = BallDistance(ball, eye, {0.0f, -1.0f, 0.0f});
	ASSERT_TRUE(down.has_value());
	EXPECT_NEAR(*down, 1000.0f, 0.01f);
	EXPECT_FALSE(BallDistance(ball, eye, {0.0f, 1.0f, 0.0f}).has_value());
	EXPECT_FALSE(BallDistance(ball, {2560.0f, -10.0f, 2560.0f}, {0.0f, -1.0f, 0.0f}).has_value());
}

TEST(PlanetNavigation, GripMovesOnSmoothlyPastTheEdge)
{
	const auto ball = EarthBall();
	const glm::vec3 eye {2560.0f, 2.0f * k_Radius, 2560.0f};
	// The edge of the ball from this eye is 30 degrees from straight down
	const float edge = std::asin(1.0f / 3.0f);
	const auto at = [&](float angle) { return GripOnBall(ball, eye, {std::sin(angle), -std::cos(angle), 0.0f}); };
	const auto inside = at(edge - 1e-6f);
	const auto outside = at(edge + 1e-6f);
	EXPECT_GT(glm::dot(inside, outside), 0.999f);
	EXPECT_NEAR(glm::length(at(0.0f) - glm::vec3(0.0f, 1.0f, 0.0f)), 0.0f, 1e-5f);
}

TEST(PlanetNavigation, GlobeDragPutsTheGripUnderTheCursor)
{
	const auto ball = EarthBall();
	const Pose atGrip {.eye = {2560.0f, 3.0e6f, 2560.0f}, .focus = ball.centre, .up = {0.0f, 0.0f, -1.0f}};
	const glm::vec3 rayAtGrip = glm::normalize(glm::vec3(0.1f, -1.0f, 0.05f));
	const auto gripped = GripOnBall(ball, atGrip.eye, rayAtGrip);
	// The cursor moves; the ray through it from the camera at the grip
	const glm::vec3 rayNow = glm::normalize(glm::vec3(-0.15f, -1.0f, -0.1f));
	const auto turn = GlobeDrag(ball, atGrip.eye, gripped, rayNow);
	const auto moved = TurnAbout(ball, turn, atGrip);
	const auto underCursor = GripOnBall(ball, moved.eye, turn * rayNow);
	EXPECT_GT(glm::dot(underCursor, gripped), 0.999999f);
	// The camera keeps its height over the ball
	EXPECT_NEAR(HeightOver(ball, moved.eye), HeightOver(ball, atGrip.eye), 2.0f);
}

TEST(PlanetNavigation, SwivelOverThePoleDoesNotFlip)
{
	const auto ball = EarthBall();
	Pose pose {.eye = {2560.0f, 1.0e7f, 2560.0f}, .focus = ball.centre, .up = {0.0f, 0.0f, -1.0f}};
	glm::vec3 lastUp = pose.up;
	float lowest = pose.eye.y;
	// Drag the globe again and again up the screen: the camera goes over the top and down the far side
	for (int step = 0; step < 60; ++step)
	{
		// The rays through the cursor at the grip and now, in the camera's own frame, turned into the world
		const glm::vec3 side = glm::cross(Forward(pose), pose.up);
		const glm::quat facing = glm::quat_cast(glm::mat3(side, pose.up, -Forward(pose)));
		const glm::vec3 rayAtGrip = facing * glm::normalize(glm::vec3(0.0f, -0.02f, -1.0f));
		const glm::vec3 rayNow = facing * glm::normalize(glm::vec3(0.0f, 0.02f, -1.0f));
		const auto turn = GlobeDrag(ball, pose.eye, GripOnBall(ball, pose.eye, rayAtGrip), rayNow);
		pose = TurnAbout(ball, turn, pose);
		EXPECT_GT(glm::dot(pose.up, lastUp), 0.99f);
		EXPECT_NEAR(glm::dot(pose.up, Forward(pose)), 0.0f, 1e-3f);
		EXPECT_NEAR(HeightOver(ball, pose.eye), 1.0e7f, 50.0f);
		lastUp = pose.up;
		lowest = std::min(lowest, pose.eye.y);
	}
	// It went past the pole of the turn, to the far side of the ball
	EXPECT_LT(lowest, ball.centre.y);
}

TEST(PlanetNavigation, SpinTurnsTheViewAboutTheEye)
{
	const auto ball = EarthBall();
	const Pose pose {.eye = {2560.0f, 1.0e6f, 2560.0f}, .focus = ball.centre, .up = {0.0f, 0.0f, -1.0f}};
	const auto spun = TurnAbout(ball, SpinAbout(ball, pose.eye, 0.5f), pose);
	EXPECT_NEAR(glm::length(spun.eye - pose.eye), 0.0f, 1.0f);
	EXPECT_NEAR(glm::dot(spun.up, pose.up), std::cos(0.5f), 1e-4f);
}

TEST(PlanetNavigation, OrbitPoseLooksAtTheMiddleWithoutRoll)
{
	const auto ball = EarthBall();
	const Pose ground {.eye = {2560.0f, 3.0e5f, 2560.0f}, .focus = {2560.0f, 0.0f, 62560.0f}, .up = {0.0f, 1.0f, 0.0f}};
	const auto orbit = OrbitPose(ball, ground);
	EXPECT_EQ(orbit.focus, ball.centre);
	EXPECT_NEAR(glm::dot(orbit.up, Forward(orbit)), 0.0f, 1e-5f);
	// The ground camera looks along z, so the top of the screen in the orbit view points along z too
	EXPECT_GT(orbit.up.z, 0.99f);
}

TEST(PlanetNavigation, BlendKeepsTheEyeAndTurnsTheView)
{
	const auto ball = EarthBall();
	const Pose ground {.eye = {2560.0f, 3.0e5f, 2560.0f}, .focus = {2560.0f, 0.0f, 62560.0f}, .up = {0.0f, 1.0f, 0.0f}};
	const auto orbit = OrbitPose(ball, ground);
	const auto none = Blend(ground, orbit, 0.0f);
	EXPECT_EQ(none.focus, ground.focus);
	EXPECT_EQ(none.up, ground.up);
	const auto all = Blend(ground, orbit, 1.0f);
	EXPECT_EQ(all.focus, orbit.focus);
	const auto half = Blend(ground, orbit, 0.5f);
	EXPECT_EQ(half.eye, ground.eye);
	const float toGround = std::acos(glm::clamp(glm::dot(Forward(half), Forward(ground)), -1.0f, 1.0f));
	const float toOrbit = std::acos(glm::clamp(glm::dot(Forward(half), Forward(orbit)), -1.0f, 1.0f));
	EXPECT_NEAR(toGround, toOrbit, 1e-3f);
	EXPECT_NEAR(glm::dot(half.up, Forward(half)), 0.0f, 1e-4f);
}

TEST(PlanetNavigation, SettleNearTheIslandGivesTheWorldCameraToTheLocalCamera)
{
	const auto ball = EarthBall();
	// A turn of 5 degrees towards the east: the point under the eye is about 530 km from the island
	const auto turn = glm::angleAxis(glm::radians(5.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	const Pose local {.eye = {2560.0f, 150000.0f, 2560.0f}, .focus = {2560.0f, 0.0f, 2600.0f}};
	const auto world = TurnAbout(ball, turn, local);
	const auto settled = Settle(ball, turn, local);
	EXPECT_FALSE(settled.spot.has_value());
	EXPECT_NEAR(glm::angle(settled.turn), 0.0f, 1e-5f);
	EXPECT_NEAR(glm::distance(settled.local.eye, world.eye), 0.0f, 1.0f);
	EXPECT_NEAR(glm::distance(settled.local.focus, world.focus), 0.0f, 1.0f);
	EXPECT_EQ(settled.local.up, glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_NEAR(ArcFromMap(ball, settled.local.eye), k_Radius * glm::radians(5.0f), 100.0f);
}

TEST(PlanetNavigation, SettleFarRoundThePlanetKeepsTheWorldCamera)
{
	const auto ball = EarthBall();
	for (const float degrees : {20.0f, 73.0f, 120.0f, 179.9f})
	{
		const auto turn = glm::angleAxis(glm::radians(degrees), glm::normalize(glm::vec3(1.0f, 0.0f, -1.0f)));
		const Pose local {.eye = {12560.0f, 150000.0f, 2560.0f}, .focus = {12560.0f, 0.0f, 2700.0f}};
		const auto world = TurnAbout(ball, turn, local);
		const auto settled = Settle(ball, turn, local);
		ASSERT_TRUE(settled.spot.has_value()) << degrees;
		// The local camera is at the spot, and the turn gives the same world's camera
		EXPECT_NEAR(glm::distance(*settled.spot, k_Middle), k_AnchorSpotDistance, 1.0f) << degrees;
		EXPECT_NEAR(ArcFromMap(ball, settled.local.eye), k_AnchorSpotDistance, 10.0f) << degrees;
		const auto again = TurnAbout(ball, settled.turn, settled.local);
		EXPECT_NEAR(glm::distance(again.eye, world.eye), 0.0f, 8.0f) << degrees;
		EXPECT_NEAR(glm::distance(again.focus, world.focus), 0.0f, 8.0f) << degrees;
		// The height over the ball does not change
		EXPECT_NEAR(HeightOver(ball, settled.local.eye), HeightOver(ball, local.eye), 2.0f) << degrees;
	}
}

TEST(PlanetNavigation, CurvedSeaDragOnAFlatSeaKeepsTheGripUnderTheCursor)
{
	const glm::vec3 eye {2560.0f, 1000.0f, 2560.0f};
	const glm::vec3 gripped {2560.0f, 0.0f, 4560.0f};
	const glm::vec3 ray = glm::normalize(glm::vec3(0.3f, -1.0f, 1.0f));
	const auto move = CurvedSeaDrag(FlatSea, eye, gripped, ray);
	ASSERT_TRUE(move.has_value());
	const glm::vec3 moved = eye + glm::vec3(move->x, 0.0f, move->y);
	const auto along = FlatSea(moved, ray);
	ASSERT_TRUE(along.has_value());
	const glm::vec3 hit = moved + ray * *along;
	EXPECT_NEAR(hit.x, gripped.x, 1e-2f);
	EXPECT_NEAR(hit.z, gripped.z, 1e-2f);
	EXPECT_FALSE(CurvedSeaDrag(FlatSea, eye, gripped, {0.0f, 1.0f, 0.0f}).has_value());
}

TEST(PlanetNavigation, CurvedSeaDragFollowsASeaThatMovesWithTheEye)
{
	// The sea goes down a little with the eye's distance from the middle, as the world's cap moves
	const SeaDistance sea = [](glm::vec3 eye, glm::vec3 direction) -> std::optional<float> {
		const float depth = eye.y + 0.01f * std::abs(eye.x - 2560.0f);
		return direction.y < 0.0f ? std::optional<float>(depth / -direction.y) : std::nullopt;
	};
	const glm::vec3 eye {2560.0f, 30000.0f, 2560.0f};
	const glm::vec3 gripped {302560.0f, 0.0f, 2560.0f};
	const glm::vec3 ray = glm::normalize(glm::vec3(1.0f, -0.05f, 0.0f));
	const auto move = CurvedSeaDrag(sea, eye, gripped, ray);
	ASSERT_TRUE(move.has_value());
	const glm::vec3 moved = eye + glm::vec3(move->x, 0.0f, move->y);
	const float hitX = moved.x + ray.x * *sea(moved, ray);
	EXPECT_NEAR(hitX, gripped.x, 0.02f * std::abs(gripped.x - eye.x));
}

TEST(PlanetNavigation, KeepInsideDiscMovesThePointToTheEdge)
{
	EXPECT_FALSE(KeepInsideDisc({10.0f, 0.0f}, {0.0f, 0.0f}, 20.0f).has_value());
	const auto kept = KeepInsideDisc({0.0f, 50.0f}, {0.0f, 10.0f}, 20.0f);
	ASSERT_TRUE(kept.has_value());
	EXPECT_NEAR(kept->x, 0.0f, 1e-5f);
	EXPECT_NEAR(kept->y, 30.0f, 1e-4f);
}

TEST(PlanetNavigation, PickLineStartsNearTheMap)
{
	const glm::vec3 middle {2560.0f, 0.0f, 2560.0f};
	// A line from near the map stays as it is
	const auto near = LineNearMap({2600.0f, 100.0f, 2500.0f}, {2600.0f, 90.0f, 2510.0f}, middle, k_PickReach);
	ASSERT_TRUE(near.has_value());
	EXPECT_EQ(near->from, glm::vec3(2600.0f, 100.0f, 2500.0f));
	EXPECT_EQ(near->to, glm::vec3(2600.0f, 90.0f, 2510.0f));
	// A line from far out starts on the ball round the map, on the same line
	const glm::vec3 far {2560.0f, 9.0e6f, 2560.0f};
	const auto clipped = LineNearMap(far, far + glm::vec3(0.0f, -1.0f, 0.0f), middle, k_PickReach);
	ASSERT_TRUE(clipped.has_value());
	EXPECT_NEAR(clipped->from.y, k_PickReach, 1.0f);
	EXPECT_LT(clipped->to.y, clipped->from.y);
	// A line that goes past the ball, or away from it, has none
	EXPECT_FALSE(LineNearMap(far, far + glm::vec3(1.0f, 0.0f, 0.0f), middle, k_PickReach).has_value());
	EXPECT_FALSE(LineNearMap(far, far + glm::vec3(0.0f, 1.0f, 0.0f), middle, k_PickReach).has_value());
	const float notANumber = std::nanf("");
	EXPECT_FALSE(LineNearMap({notANumber, 0.0f, 0.0f}, far, middle, k_PickReach).has_value());
}

TEST(PlanetNavigation, OrbitTopHoldsTheEye)
{
	const auto ball = EarthBall();
	EXPECT_FALSE(KeepUnderOrbitTop(ball, {2560.0f, 1.0e7f, 2560.0f}).has_value());
	const auto kept = KeepUnderOrbitTop(ball, {2560.0f, 5.0e7f, 2560.0f});
	ASSERT_TRUE(kept.has_value());
	EXPECT_NEAR(HeightOver(ball, *kept), k_OrbitHighest, 4.0f);
	EXPECT_NEAR(kept->x, 2560.0f, 1.0f);
}

TEST(PlanetNavigation, OrbitZoomChangesTheHeightByAFactor)
{
	EXPECT_NEAR(OrbitZoomHeight(1.0e6f, 0.0f), 1.0e6f, 1.0f);
	EXPECT_NEAR(OrbitZoomHeight(1.0e6f, 60.0f), 1.25e6f, 10.0f);
	EXPECT_NEAR(OrbitZoomHeight(1.0e6f, -60.0f), 0.8e6f, 10.0f);
	EXPECT_GT(OrbitZoomHeight(1.0e6f, -600.0f), 0.0f);
}
