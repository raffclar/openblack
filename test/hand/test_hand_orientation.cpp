/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <gtest/gtest.h>

#include "3D/HandOrientation.h"
#include "Camera/Camera.h"

using namespace openblack::hand_orientation;

namespace
{
void ExpectNear(glm::vec3 actual, glm::vec3 expected)
{
	EXPECT_NEAR(actual.x, expected.x, 1e-5f);
	EXPECT_NEAR(actual.y, expected.y, 1e-5f);
	EXPECT_NEAR(actual.z, expected.z, 1e-5f);
}

/// The game's frame for the hand before it is scaled: its side, its front and its down (the hand model's x, y and z),
/// made from the heading angle of the line of sight and the hand's up
glm::mat3 GameHandFrame(glm::vec3 lineOfSight, glm::vec3 up)
{
	const auto angle = std::atan2(lineOfSight.x, -lineOfSight.z) + glm::pi<float>();
	const glm::vec3 back {std::sin(angle), 0.0f, -std::cos(angle)};
	const auto side = glm::normalize(glm::cross(back, up));
	return {side, glm::cross(side, up), -up};
}
} // namespace

TEST(HandOrientation, FacesAlongTheLevelledLineOfSight)
{
	ExpectNear(HeadingAlongRay(glm::normalize(glm::vec3(3.0f, -4.0f, 4.0f)), {0.0f, 0.0f, 1.0f}),
	           glm::normalize(glm::vec3(3.0f, 0.0f, 4.0f)));
}

TEST(HandOrientation, KeepsItsHeadingLookingStraightDown)
{
	const glm::vec3 previous {1.0f, 0.0f, 0.0f};
	ExpectNear(HeadingAlongRay(glm::normalize(glm::vec3(0.005f, -1.0f, -0.008f)), previous), previous);
	// Just past the lean it turns
	ExpectNear(HeadingAlongRay(glm::vec3(0.0f, -0.99f, -0.0101f), previous), {0.0f, 0.0f, -1.0f});
}

TEST(HandOrientation, TurnsByTheAngleBetweenTheCamerasHeadingAndItsOwn)
{
	const auto facingCamera = glm::mat3(glm::eulerAngleY(0.3f) * glm::eulerAngleX(glm::half_pi<float>()));
	const glm::vec3 cameraHeading {0.0f, 0.0f, 1.0f};
	// Facing the camera's own way it is as it was
	const auto same = TurnToHeading(facingCamera, cameraHeading, cameraHeading);
	for (int i = 0; i < 3; ++i)
	{
		ExpectNear(same[i], facingCamera[i]);
	}
	// A quarter turn round to the east turns whatever faced north to face east
	const auto turned = TurnToHeading(glm::mat3(1.0f), cameraHeading, {1.0f, 0.0f, 0.0f});
	ExpectNear(turned * glm::vec3(0.0f, 0.0f, 1.0f), {1.0f, 0.0f, 0.0f});
	ExpectNear(turned * glm::vec3(0.0f, 1.0f, 0.0f), {0.0f, 1.0f, 0.0f});
}

TEST(HandOrientation, StandsUprightOnLevelLand)
{
	const auto level = glm::mat3(glm::eulerAngleY(1.1f) * glm::eulerAngleX(glm::half_pi<float>()));
	const auto stood = StandOnSlope(level, glm::normalize(glm::vec3(0.4f, 0.0f, 0.9f)), {0.0f, 1.0f, 0.0f});
	for (int i = 0; i < 3; ++i)
	{
		ExpectNear(stood[i], level[i]);
	}
}

TEST(HandOrientation, OnASlopeItsUpIsTheSlopesAndItsFrontTheHeadingLaidAlongIt)
{
	const glm::vec3 heading {1.0f, 0.0f, 0.0f};
	const auto up = glm::normalize(glm::vec3(0.3f, 1.0f, 0.4f));
	// On level land the hand's up is the world's up and its front the heading
	const auto stood = StandOnSlope(glm::mat3(1.0f), heading, up);
	ExpectNear(stood * glm::vec3(0.0f, 1.0f, 0.0f), up);
	ExpectNear(stood * heading, glm::normalize(heading - glm::dot(heading, up) * up));
	// It stays a rotation
	EXPECT_NEAR(glm::determinant(stood), 1.0f, 1e-5f);
}

TEST(HandOrientation, ScoopingTheHandTipsItsFingersDownAlongItsHeading)
{
	const glm::vec3 heading {0.0f, 0.0f, -1.0f};
	const auto tipped = TipForwards(glm::mat3(1.0f), heading, glm::half_pi<float>());
	// What pointed ahead points down, and the line across the heading stays where it was
	ExpectNear(tipped * heading, {0.0f, -1.0f, 0.0f});
	ExpectNear(tipped * glm::vec3(1.0f, 0.0f, 0.0f), {1.0f, 0.0f, 0.0f});
	EXPECT_NEAR(glm::determinant(tipped), 1.0f, 1e-5f);
}

TEST(HandOrientation, TheHandsFrameIsTheGamesFrame)
{
	// This frame places the wrist bone, where the carried leash sits, so it must be the game's frame
	const glm::mat4 modelRotationCorrection = glm::eulerAngleX(glm::half_pi<float>());
	const std::array<std::pair<glm::vec3, glm::vec3>, 3> cameras {{
	    {{0.0f, 80.0f, 0.0f}, {60.0f, 0.0f, 90.0f}},
	    {{500.0f, 120.0f, 300.0f}, {420.0f, 0.0f, 360.0f}},
	    {{-40.0f, 60.0f, 10.0f}, {-90.0f, 0.0f, -70.0f}},
	}};
	const std::array<glm::vec3, 2> lines {glm::normalize(glm::vec3(0.6f, -0.5f, -0.2f)),
	                                      glm::normalize(glm::vec3(-0.1f, -0.9f, 0.7f))};
	const std::array<glm::vec3, 2> ups {glm::vec3(0.0f, 1.0f, 0.0f), glm::normalize(glm::vec3(0.3f, 1.0f, -0.4f))};
	for (const auto& [eye, focus] : cameras)
	{
		openblack::Camera camera;
		camera.SetOrigin(eye).SetFocus(focus);
		const auto facingCamera = glm::mat3(glm::eulerAngleY(camera.GetRotation().y) * modelRotationCorrection);
		const auto cameraHeading = HeadingAlongRay(camera.GetForward(), {0.0f, 0.0f, 1.0f});
		for (const auto& line : lines)
		{
			const auto heading = HeadingAlongRay(line, {0.0f, 0.0f, 1.0f});
			for (const auto& up : ups)
			{
				const auto frame = StandOnSlope(TurnToHeading(facingCamera, cameraHeading, heading), heading, up);
				const auto game = GameHandFrame(line, up);
				for (int i = 0; i < 3; ++i)
				{
					ExpectNear(frame[i], game[i]);
				}
			}
		}
	}
}
