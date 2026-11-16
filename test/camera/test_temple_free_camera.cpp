/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/TempleFreeCamera.h"

using namespace openblack::temple_free_camera;

namespace
{
constexpr float k_Near = 1e-4f;
} // namespace

TEST(TempleFreeCamera, HasTheViewOnlyWithTheCheatInTheTempleAndTheRoomCameraInControl)
{
	EXPECT_TRUE(HasControl({.cheatOn = true, .inTemple = true, .roomCameraInControl = true}));
	EXPECT_FALSE(HasControl({.cheatOn = false, .inTemple = true, .roomCameraInControl = true}));
	EXPECT_FALSE(HasControl({.cheatOn = true, .inTemple = false, .roomCameraInControl = true}));
	// The path into a room and a walk through a door stay as with no cheat
	EXPECT_FALSE(HasControl({.cheatOn = true, .inTemple = true, .roomCameraInControl = false}));
	EXPECT_FALSE(HasControl({}));
}

TEST(TempleFreeCamera, StartsWithTheViewOfTheRoomCamera)
{
	const glm::vec3 origin {1.0f, 2.0f, 3.0f};
	const glm::vec3 focus {11.0f, 2.0f, 3.0f};
	const auto view = ViewOf(origin, focus);
	EXPECT_NEAR(glm::distance(view.origin, origin), 0.0f, k_Near);
	EXPECT_NEAR(view.pitch, 0.0f, k_Near);
	const auto way = glm::normalize(Focus(view) - view.origin);
	EXPECT_NEAR(glm::distance(way, glm::vec3(1.0f, 0.0f, 0.0f)), 0.0f, k_Near);
}

TEST(TempleFreeCamera, TheMiddleButtonTurnsTheCameraAboutItsPlace)
{
	const View start {.origin = {5.0f, 6.0f, 7.0f}};
	const auto turned = Step(start, {.steer = true, .mouse = {100.0f, -40.0f}, .seconds = 0.016f});
	EXPECT_EQ(turned.origin, start.origin);
	EXPECT_NEAR(turned.heading, 100.0f * k_TurnPerPixel, k_Near);
	// A move up the screen looks up
	EXPECT_NEAR(turned.pitch, 40.0f * k_TurnPerPixel, k_Near);
}

TEST(TempleFreeCamera, TheMouseDoesNotTurnTheCameraWithoutTheMiddleButton)
{
	const View start {.origin = {5.0f, 6.0f, 7.0f}, .heading = 0.3f};
	const auto next = Step(start, {.mouse = {100.0f, -40.0f}, .seconds = 0.016f});
	EXPECT_EQ(next.heading, start.heading);
	EXPECT_EQ(next.pitch, start.pitch);
}

TEST(TempleFreeCamera, ThePitchStopsShortOfStraightUpAndDown)
{
	const auto up = Step({}, {.steer = true, .mouse = {0.0f, -100000.0f}});
	EXPECT_EQ(up.pitch, k_PitchLimit);
	const auto down = Step({}, {.steer = true, .mouse = {0.0f, 100000.0f}});
	EXPECT_EQ(down.pitch, -k_PitchLimit);
	EXPECT_LT(k_PitchLimit, std::acos(0.0f));
	// The view of a room camera that looks straight down stays in the limit too
	EXPECT_EQ(ViewOf({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 0.0f}).pitch, -k_PitchLimit);
}

TEST(TempleFreeCamera, TheArrowKeysMoveAlongTheViewAndToTheSide)
{
	// A heading of 0 looks along z, and the right of the view is x
	const View start {};
	const auto forwards = Step(start, {.forwards = true, .seconds = 0.5f});
	EXPECT_NEAR(glm::distance(forwards.origin, glm::vec3(0.0f, 0.0f, k_MoveSpeed * 0.5f)), 0.0f, k_Near);
	const auto right = Step(start, {.right = true, .seconds = 0.5f});
	EXPECT_NEAR(glm::distance(right.origin, glm::vec3(k_MoveSpeed * 0.5f, 0.0f, 0.0f)), 0.0f, k_Near);
	const auto left = Step(start, {.left = true, .seconds = 0.5f});
	EXPECT_NEAR(glm::distance(left.origin, glm::vec3(-k_MoveSpeed * 0.5f, 0.0f, 0.0f)), 0.0f, k_Near);
	// Forwards and backwards together do not move the camera
	const auto both = Step(start, {.forwards = true, .backwards = true, .seconds = 0.5f});
	EXPECT_EQ(both.origin, start.origin);
}

TEST(TempleFreeCamera, ForwardsFollowsThePitchUpAndDown)
{
	const View lookingUp {.pitch = 0.5f};
	const auto next = Step(lookingUp, {.forwards = true, .seconds = 1.0f});
	EXPECT_NEAR(next.origin.y, k_MoveSpeed * std::sin(0.5f), 1e-3f);
	// A move to the side and forwards together goes at the same speed
	const auto diagonal = Step({}, {.forwards = true, .right = true, .seconds = 1.0f});
	EXPECT_NEAR(glm::length(diagonal.origin), k_MoveSpeed, 1e-3f);
}
