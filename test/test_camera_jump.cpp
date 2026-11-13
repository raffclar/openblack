/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraJump.h"
#include "ECS/WorldSave.h"

using namespace openblack;

TEST(CameraJump, AGlideOfTheEyeIsNotAJump)
{
	const glm::vec3 eye {1500.0f, 150.0f, 1327.0f};
	EXPECT_FALSE(camera_jump::IsJump(eye, eye));
	EXPECT_FALSE(camera_jump::IsJump(eye, eye + glm::vec3(600.0f, 0.0f, 800.0f)));
}

TEST(CameraJump, AMoveOfMoreThanTheJumpDistanceInOneFrameIsAJump)
{
	const glm::vec3 eye {1500.0f, 150.0f, 1327.0f};
	EXPECT_TRUE(camera_jump::IsJump(eye, eye + glm::vec3(600.0f, 0.0f, 800.1f)));
	// The world camera of a game server that the old load took: the corner of the land
	EXPECT_TRUE(camera_jump::IsJump(eye, glm::vec3(0.0f, 0.0f, 0.0f)));
}

TEST(CameraJump, AnEyeThatIsNotANumberIsAJump)
{
	const glm::vec3 eye {1500.0f, 150.0f, 1327.0f};
	const float notANumber = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(camera_jump::IsJump(eye, glm::vec3(notANumber, 150.0f, 1327.0f)));
	EXPECT_TRUE(camera_jump::IsJump(glm::vec3(notANumber), eye));
}

TEST(CameraJump, AWorldStateOfTheGameServerKeepsTheCameraOfThisComputer)
{
	// After a resynchronisation, a return or a join, the camera stays where the player has it
	EXPECT_FALSE(ecs::world_save::LoadsCamera(ecs::world_save::LoadKind::WorldState));
}

TEST(CameraJump, ASavedGamePutsTheCameraWhereTheSaveKeepsIt)
{
	EXPECT_TRUE(ecs::world_save::LoadsCamera(ecs::world_save::LoadKind::SavedGame));
}
