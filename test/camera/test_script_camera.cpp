/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <numbers>
#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "ECS/Systems/Implementations/ScriptControlSystem.h"

using namespace openblack;
using openblack::ecs::systems::ScriptControlSystem;

namespace
{
float FlatLand(float /*x*/, float /*z*/)
{
	return 0.0f;
}

ScriptControlSystem::CameraRequest Outside(uint32_t task)
{
	return {.task = task, .templeScript = false, .insideTemple = false};
}
} // namespace

TEST(ScriptCamera, ACameraSentOutOfTheWorldTurnsBackToItsEdge)
{
	EXPECT_FALSE(script_camera::PullIntoWorld({2560.0f, 100.0f, 2560.0f}).has_value());
	EXPECT_FALSE(script_camera::PullIntoWorld({2560.0f + 3000.0f, 0.0f, 2560.0f}).has_value());

	const auto pulled = script_camera::PullIntoWorld({2560.0f + 7000.0f, 0.0f, 2560.0f});
	ASSERT_TRUE(pulled.has_value());
	// Back along the same line from the middle, onto the edge
	EXPECT_NEAR(pulled->x - 2560.0f, 1.0f / script_camera::k_WorldScale, 0.01f);
	EXPECT_NEAR(pulled->y, 0.0f, 1e-4f);
	EXPECT_NEAR(pulled->z, 2560.0f, 1e-3f);
}

TEST(ScriptCamera, TheCameraIsDrawnAboveTheLand)
{
	// A camera where it looks steps aside and up
	const auto aside = script_camera::Drawn({.origin = {10.0f, 50.0f, 10.0f}, .focus = {10.0f, 50.0f, 10.0f}}, FlatLand);
	EXPECT_FLOAT_EQ(aside.origin.x, 9.0f);
	EXPECT_FLOAT_EQ(aside.origin.y, 51.0f);

	// Under the land, it is lifted a metre above it, and what it looks at with it
	const auto lifted =
	    script_camera::Drawn({.origin = {0.0f, 5.0f, 0.0f}, .focus = {20.0f, 7.0f, 0.0f}}, [](float, float) { return 10.0f; });
	EXPECT_FLOAT_EQ(lifted.origin.y, 11.0f);
	EXPECT_FLOAT_EQ(lifted.focus.y, 13.0f);
}

TEST(ScriptCamera, ItGlidesWhereItIsSentAndArrives)
{
	Camera camera;
	// Well within the world
	ScriptCameraModel model({2500.0f, 50.0f, 2500.0f}, {2600.0f, 0.0f, 2500.0f}, FlatLand);
	EXPECT_TRUE(model.Arrived());

	model.MoveOrigin({2500.0f, 50.0f, 2600.0f}, 1.0f);
	EXPECT_FALSE(model.Arrived());
	EXPECT_FLOAT_EQ(model.GetTargetOrigin().z, 2600.0f);

	// No more than a tenth of a second a frame
	const auto first = model.Update(std::chrono::seconds(5), camera);
	ASSERT_TRUE(first.has_value());
	EXPECT_LT(first->origin.z, 2600.0f);
	EXPECT_FALSE(model.Arrived());
	for (int i = 0; i < 10; ++i)
	{
		model.Update(std::chrono::milliseconds(100), camera);
	}
	EXPECT_TRUE(model.Arrived());

	// Put somewhere, it is there at once
	model.SetFocus({2505.0f, 6.0f, 2507.0f});
	const auto placed = model.Update(std::chrono::milliseconds(10), camera);
	ASSERT_TRUE(placed.has_value());
	EXPECT_FLOAT_EQ(placed->focus.x, 2505.0f);
	EXPECT_TRUE(model.Arrived());
}

TEST(ScriptControl, OneTaskAtATimeHasTheCamera)
{
	Camera camera;
	ScriptControlSystem control;
	EXPECT_EQ(control.GetScriptCamera(camera), nullptr);

	ASSERT_TRUE(control.StartCameraControl(camera, Outside(3), FlatLand));
	EXPECT_EQ(control.GetCameraOwner(), 3u);
	EXPECT_NE(control.GetScriptCamera(camera), nullptr);

	// Another task can't take it until it is given back
	EXPECT_FALSE(control.StartCameraControl(camera, Outside(4), FlatLand));
	EXPECT_EQ(control.GetCameraOwner(), 3u);
	EXPECT_FALSE(control.EndCameraControl(camera, 4));
	EXPECT_NE(control.GetScriptCamera(camera), nullptr);

	// Given back, the player has a camera of their own again
	EXPECT_TRUE(control.EndCameraControl(camera, 3));
	EXPECT_EQ(control.GetCameraOwner(), 0u);
	EXPECT_EQ(control.GetScriptCamera(camera), nullptr);
	EXPECT_NE(dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()), nullptr);
}

TEST(ScriptControl, InsideTheTempleOnlyItsScriptsTakeControl)
{
	Camera camera;
	ScriptControlSystem control;
	EXPECT_FALSE(control.StartCameraControl(camera, {.task = 2, .templeScript = false, .insideTemple = true}, FlatLand));
	EXPECT_EQ(control.GetCameraOwner(), 0u);

	// The temple's own scripts take control, and the temple's camera stays as it is
	EXPECT_TRUE(control.StartCameraControl(camera, {.task = 2, .templeScript = true, .insideTemple = true}, FlatLand));
	EXPECT_EQ(control.GetCameraOwner(), 2u);
	EXPECT_EQ(control.GetScriptCamera(camera), nullptr);
	EXPECT_TRUE(control.EndCameraControl(camera, 2));
	EXPECT_EQ(control.GetCameraOwner(), 0u);
}

TEST(ScriptControl, AStoppedTaskGivesBackWhatItHad)
{
	Camera camera;
	ScriptControlSystem control;
	ASSERT_TRUE(control.StartCameraControl(camera, Outside(5), FlatLand));
	control.StartGameSpeed(6);

	const auto other = control.TaskStopped(camera, 7);
	EXPECT_FALSE(other.camera);
	EXPECT_FALSE(other.gameSpeed);

	const auto withCamera = control.TaskStopped(camera, 5);
	EXPECT_TRUE(withCamera.camera);
	EXPECT_FALSE(withCamera.gameSpeed);
	EXPECT_EQ(control.GetScriptCamera(camera), nullptr);

	const auto withSpeed = control.TaskStopped(camera, 6);
	EXPECT_FALSE(withSpeed.camera);
	EXPECT_TRUE(withSpeed.gameSpeed);
	EXPECT_FALSE(control.MaySetGameSpeed(6));
}

TEST(ScriptControl, OnlyTheTaskWithTheGameSpeedSetsIt)
{
	ScriptControlSystem control;
	control.StartGameSpeed(1);
	control.StartGameSpeed(2);
	EXPECT_TRUE(control.MaySetGameSpeed(1));
	EXPECT_FALSE(control.MaySetGameSpeed(2));

	// Another task can't give it back, the one with it can
	EXPECT_FALSE(control.EndGameSpeed(2));
	EXPECT_TRUE(control.MaySetGameSpeed(1));
	EXPECT_TRUE(control.EndGameSpeed(1));
	EXPECT_FALSE(control.MaySetGameSpeed(1));

	// With nobody holding it, giving it back puts the speed back to normal
	EXPECT_TRUE(control.EndGameSpeed(9));
}

TEST(ScriptCamera, HeadingAndPitchAreTheWayBackFromTheirPoint)
{
	const glm::vec3 focus {100.0f, 10.0f, 200.0f};
	const auto origin = script_camera::PointFromDistanceHeadingAndPitch(focus, 5.0f, 0.4f, 0.3f);
	EXPECT_NEAR(glm::distance(origin, focus), 5.0f, 1e-4f);
	const auto view = script_camera::HeadingAndPitchFromPoints(origin, focus);
	EXPECT_NEAR(view.heading, 0.4f, 1e-5f);
	EXPECT_NEAR(view.pitch, 0.3f, 1e-5f);
	// Straight above, the heading is none and the pitch the game's straight-down one
	const auto above = script_camera::HeadingAndPitchFromPoints({100.0f, 50.0f, 200.0f}, focus);
	EXPECT_FLOAT_EQ(above.heading, 0.0f);
	EXPECT_FLOAT_EQ(above.pitch, script_camera::k_StraightDownPitch);
}

TEST(ScriptCamera, AFollowingCamerasGlidesShortenOverItsFirstTwoSeconds)
{
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(0.0f, 1.0f), 2.0f);
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(1.0f, 1.0f), 1.5f);
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(3.0f, 0.2f), 0.2f);
	// A script asking for no glide follows at once
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(3.0f, 0.0f), 0.0f);
}

TEST(ScriptCamera, AFollowingCameraLooksDownAtLeastALittleAndTurnsWithTheThing)
{
	script_camera::FollowSettings settings {.heading = 0.0f, .pitch = 0.0f, .viewingDistance = 3.0f, .relativeHeading = true};
	// Facing a quarter turn round (512 of 2048), the heading turns back by nothing
	const script_camera::FollowedThing thing {.point = {10.0f, 0.0f, 10.0f}, .gameAngle = 512, .height = 1.0f};
	const auto origin = script_camera::FollowOrigin(thing, settings);
	EXPECT_FLOAT_EQ(settings.pitch, script_camera::k_FollowLowestPitch);
	const auto view = script_camera::HeadingAndPitchFromPoints(origin, thing.point);
	EXPECT_NEAR(view.heading, 0.0f, 1e-5f);
	EXPECT_NEAR(view.pitch, script_camera::k_FollowLowestPitch, 1e-5f);
	// Facing the other way, the camera goes round with it
	const script_camera::FollowedThing turned {.point = thing.point, .gameAngle = 0, .height = 1.0f};
	const auto behind = script_camera::FollowOrigin(turned, settings);
	EXPECT_NEAR(script_camera::HeadingAndPitchFromPoints(behind, thing.point).heading, std::numbers::pi_v<float> / 2.0f, 1e-5f);
}

TEST(ScriptCamera, ItFollowsAThingAtOnceWithNoGlideUntilSentElsewhere)
{
	glm::vec3 at {1000.0f, 5.0f, 1000.0f};
	ScriptCameraModel model({990.0f, 20.0f, 990.0f}, {1000.0f, 5.0f, 1000.0f}, FlatLand);
	const auto thing = [&at]() {
		return std::optional(script_camera::FollowedThing {.point = at, .gameAngle = std::nullopt, .height = 1.0f});
	};
	model.FollowAndLookAt(thing, 3.0f);
	model.SetFollowSettings(3.0f, 0.0f, 0.0f, false);
	at += glm::vec3(4.0f, 0.0f, 0.0f);
	Camera camera;
	const auto update = model.Update(std::chrono::milliseconds(16), camera);
	ASSERT_TRUE(update.has_value());
	EXPECT_NEAR(glm::distance(model.GetTargetFocus(), at), 0.0f, 1e-4f);
	EXPECT_NEAR(glm::distance(model.GetTargetOrigin(), at), 3.0f, 1e-3f);
	// Placed somewhere, it follows no more, nor looks at what it followed
	model.SetOrigin({980.0f, 50.0f, 980.0f});
	const auto lastSeen = at;
	at += glm::vec3(4.0f, 0.0f, 0.0f);
	static_cast<void>(model.Update(std::chrono::milliseconds(16), camera));
	EXPECT_NEAR(glm::distance(model.GetTargetOrigin(), glm::vec3(980.0f, 50.0f, 980.0f)), 0.0f, 1e-4f);
	EXPECT_NEAR(glm::distance(model.GetTargetFocus(), lastSeen), 0.0f, 1e-4f);
}
