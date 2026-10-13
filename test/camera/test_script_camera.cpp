/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <chrono>
#include <numbers>
#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/CameraTrack.h"
#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/Implementations/ScriptControlSystem.h"
#include "Locator.h"

using namespace openblack;
using openblack::ecs::systems::ScriptControlSystem;

namespace
{
float FlatLand(float /*x*/, float /*z*/)
{
	return 0.0f;
}

/// A straight way along x from `from`, 100 long, covered in two seconds at an even 50 metres a second, its handles a
/// third and two thirds along
edt::EDTWay StraightWay(std::array<float, 3> from)
{
	edt::EDTWay way {};
	const std::array<float, 3> to {from[0] + 100.0f, from[1], from[2]};
	way.duration = 2000;
	way.points = {from, to};
	way.handles = {{{{from[0] + 100.0f / 3.0f, from[1], from[2]}, {from[0] + 200.0f / 3.0f, from[1], from[2]}}}};
	way.times = {0.0f, 2000.0f};
	way.speeds = {50.0f, 50.0f};
	return way;
}

std::shared_ptr<const edt::EDTTrack> StraightTrack()
{
	auto track = std::make_shared<edt::EDTTrack>();
	track->position = StraightWay({2400.0f, 60.0f, 2500.0f});
	track->focus = StraightWay({2400.0f, 10.0f, 2550.0f});
	return track;
}

/// Counts the requests to draw all the high-detail villagers as usual again
class CountingHighDetail final: public ecs::systems::HighDetailSystemInterface
{
public:
	explicit CountingHighDetail(int& releases)
	    : _releases(releases)
	{
	}
	void Make(entt::entity) override {}
	void Release(entt::entity) override {}
	bool ReleaseAll() override
	{
		++_releases;
		return true;
	}
	bool Order(entt::entity, ecs::high_detail_rules::ThingSpecial, bool) override { return false; }
	void Update() override {}
	void PlaceEyes(uint32_t, const glm::mat4&) override {}

private:
	int& _releases;
};

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

TEST(ScriptControl, TheCameraGivenBackDrawsTheHighDetailVillagersAsUsual)
{
	int releases = 0;
	Locator::highDetailSystem::emplace<CountingHighDetail>(releases);
	Camera camera;
	ScriptControlSystem control;
	ASSERT_TRUE(control.StartCameraControl(camera, Outside(5), FlatLand));
	EXPECT_EQ(releases, 0);
	EXPECT_TRUE(control.EndCameraControl(camera, 5));
	EXPECT_EQ(releases, 1);
	// The same occurs when the task with the camera stops
	ASSERT_TRUE(control.StartCameraControl(camera, Outside(6), FlatLand));
	control.TaskStopped(camera, 6);
	EXPECT_EQ(releases, 2);
	// Another task that stops does not release them
	control.TaskStopped(camera, 7);
	EXPECT_EQ(releases, 2);
	Locator::highDetailSystem::reset();
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
	model.FollowAndLookAt({.id = 7, .lookup = thing}, 3.0f);
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

TEST(ScriptCamera, ItIsAboutTheThingItLooksAtElseTheOneItFollows)
{
	ScriptCameraModel model({990.0f, 20.0f, 990.0f}, {1000.0f, 5.0f, 1000.0f}, FlatLand);
	bool followedThere = true;
	const auto followed = [&followedThere]() {
		return followedThere ? std::optional(script_camera::FollowedThing {.point = {1000.0f, 5.0f, 1000.0f}}) : std::nullopt;
	};
	const auto lookedAt = []() { return std::optional(script_camera::FollowedThing {.point = {1010.0f, 5.0f, 1000.0f}}); };
	EXPECT_FALSE(model.GetFocusedThing().has_value());
	model.Follow({.id = 3, .lookup = followed});
	EXPECT_EQ(model.GetFocusedThing(), 3u);
	model.LookAt({.id = 5, .lookup = lookedAt});
	EXPECT_EQ(model.GetFocusedThing(), 5u);
	// Sent to look at a point, it is about what it follows again
	model.MoveFocus({1000.0f, 0.0f, 990.0f}, 1.0f);
	EXPECT_EQ(model.GetFocusedThing(), 3u);
	// Once the followed thing is gone, about nothing
	followedThere = false;
	Camera camera;
	static_cast<void>(model.Update(std::chrono::milliseconds(16), camera));
	EXPECT_FALSE(model.GetFocusedThing().has_value());
}

TEST(ScriptCamera, ATrackPutsTheCameraWhereItsWaysHaveGotToAtTheFramesTime)
{
	Camera camera;
	ScriptCameraModel model({2500.0f, 50.0f, 2500.0f}, {2600.0f, 0.0f, 2500.0f}, FlatLand);
	const auto track = StraightTrack();
	model.RunTrack(track);
	EXPECT_TRUE(model.OnTrack());
	EXPECT_FALSE(model.Arrived());

	// The track runs on the game's time, which isn't held to a tenth of a second a frame; with none it holds
	camera_track::WayRunner runner(track->position);
	model.PassGameTime(std::chrono::milliseconds(1250));
	const auto first = model.Update(std::chrono::milliseconds(16), camera);
	ASSERT_TRUE(first.has_value());
	const auto expected = runner.Get(track->position, 1250);
	EXPECT_FLOAT_EQ(first->origin.x, expected.x);
	EXPECT_FLOAT_EQ(first->origin.y, expected.y);
	EXPECT_NEAR(first->origin.x, 2462.5f, 2.0f);
	// It looks where the look-at way is at the same point of its curve
	const auto look = camera_track::Bezier(track->focus, runner.GetSegment(), runner.GetParameter());
	EXPECT_FLOAT_EQ(first->focus.x, look.x);
	EXPECT_FLOAT_EQ(first->focus.z, 2550.0f);
	EXPECT_FALSE(model.Arrived());

	const auto paused = model.Update(std::chrono::milliseconds(16), camera);
	ASSERT_TRUE(paused.has_value());
	EXPECT_FLOAT_EQ(paused->origin.x, first->origin.x);
	model.PassGameTime(std::chrono::milliseconds(749));
	model.Update(std::chrono::milliseconds(16), camera);
	EXPECT_FALSE(model.Arrived());
	model.PassGameTime(std::chrono::milliseconds(1));
	model.Update(std::chrono::milliseconds(16), camera);
	EXPECT_TRUE(model.Arrived());
	// At the end it holds the last point
	model.PassGameTime(std::chrono::milliseconds(500));
	const auto end = model.Update(std::chrono::milliseconds(16), camera);
	ASSERT_TRUE(end.has_value());
	EXPECT_FLOAT_EQ(end->origin.x, 2500.0f);
	EXPECT_TRUE(model.Arrived());

	// Placing or sending the camera takes it off the track
	model.MoveFocus({2505.0f, 6.0f, 2507.0f}, 1.0f);
	EXPECT_FALSE(model.OnTrack());
	EXPECT_FALSE(model.Arrived());
}

TEST(ScriptCamera, PutFromOutsideItStopsThereAtOnce)
{
	Camera camera;
	ScriptCameraModel model({2500.0f, 50.0f, 2500.0f}, {2600.0f, 0.0f, 2500.0f}, FlatLand);
	model.MoveOrigin({2500.0f, 50.0f, 2900.0f}, 5.0f);
	model.MoveFocus({2600.0f, 0.0f, 2900.0f}, 5.0f);
	model.Update(std::chrono::milliseconds(100), camera);
	model.PlaceAt({2450.0f, 40.0f, 2460.0f}, {2470.0f, 5.0f, 2480.0f});
	EXPECT_TRUE(model.Arrived());
	const auto placed = model.Update(std::chrono::milliseconds(100), camera);
	ASSERT_TRUE(placed.has_value());
	EXPECT_FLOAT_EQ(placed->origin.z, 2460.0f);
	EXPECT_FLOAT_EQ(placed->focus.x, 2470.0f);
	EXPECT_TRUE(model.Arrived());
}

TEST(ScriptCamera, PutFromOutsideItsTrackContinues)
{
	Camera camera;
	ScriptCameraModel model({2500.0f, 50.0f, 2500.0f}, {2600.0f, 0.0f, 2500.0f}, FlatLand);
	const auto track = StraightTrack();
	model.RunTrack(track);
	model.PassGameTime(std::chrono::milliseconds(500));
	model.Update(std::chrono::milliseconds(16), camera);
	model.PlaceAt({2000.0f, 40.0f, 2000.0f}, {2010.0f, 5.0f, 2010.0f});
	// The track continues and puts the camera back on it on the next frame
	EXPECT_TRUE(model.OnTrack());
	const auto next = model.Update(std::chrono::milliseconds(16), camera);
	ASSERT_TRUE(next.has_value());
	camera_track::WayRunner runner(track->position);
	const auto expected = runner.Get(track->position, 500);
	EXPECT_FLOAT_EQ(next->origin.x, expected.x);
}

TEST(ScriptControl, TheKeptCameraDetailsStartAtZeroAndStayForANewLand)
{
	ScriptControlSystem control;
	EXPECT_EQ(control.GetCameraDetails().origin, glm::vec3(0.0f));
	EXPECT_EQ(control.GetCameraDetails().focus, glm::vec3(0.0f));
	control.StoreCameraDetails({.origin = {1.0f, 2.0f, 3.0f}, .focus = {4.0f, 5.0f, 6.0f}});
	control.StoreCameraDetails({.origin = {7.0f, 8.0f, 9.0f}, .focus = {10.0f, 11.0f, 12.0f}});
	control.Reset();
	EXPECT_EQ(control.GetCameraDetails().origin, glm::vec3(7.0f, 8.0f, 9.0f));
	EXPECT_EQ(control.GetCameraDetails().focus, glm::vec3(10.0f, 11.0f, 12.0f));
}

TEST(ScriptCamera, NoTrackRunsNothing)
{
	ScriptCameraModel model({2500.0f, 50.0f, 2500.0f}, {2600.0f, 0.0f, 2500.0f}, FlatLand);
	model.RunTrack(nullptr);
	EXPECT_FALSE(model.OnTrack());
	EXPECT_TRUE(model.Arrived());
}

TEST(ScriptCamera, ACameraFacingAThingLooksAtItsMiddleFromTheWayItFaces)
{
	const script_camera::FacedThing thing {.position = {100.0f, 10.0f, 200.0f}, .height = 8.0f, .facing = 0.0f};
	const auto view = script_camera::FaceView(thing, 30.0f);
	EXPECT_FLOAT_EQ(view.focus.y, 14.0f);
	// Heading 0 is along +z, a tenth of a radian up
	EXPECT_NEAR(view.origin.x, 100.0f, 1e-4f);
	EXPECT_NEAR(view.origin.y, 14.0f + (30.0f * std::sin(0.1f)), 1e-4f);
	EXPECT_NEAR(view.origin.z, 200.0f + (30.0f * std::cos(0.1f)), 1e-4f);
	// A facing beyond a turn is brought within one
	const auto turned = script_camera::FaceView({.position = thing.position, .height = 8.0f, .facing = 1.0f}, 30.0f);
	const auto wound = script_camera::FaceView(
	    {.position = thing.position, .height = 8.0f, .facing = 1.0f + script_camera::k_FullTurn}, 30.0f);
	EXPECT_NEAR(glm::distance(turned.origin, wound.origin), 0.0f, 1e-3f);
	EXPECT_NEAR(turned.origin.x - 100.0f, 30.0f * std::cos(0.1f) * std::sin(1.0f), 1e-3f);
}

TEST(ScriptCamera, ACreaturesFacingIsItsBodysTurnPlusATurnLessTwoAndAHalf)
{
	EXPECT_NEAR(script_camera::CreatureFacing(1.0f), 1.0f + script_camera::k_FullTurn - 2.5f, 1e-6f);
	EXPECT_FALSE(script_camera::IsWrongFacing(3.0f * script_camera::k_FullTurn));
	EXPECT_TRUE(script_camera::IsWrongFacing(4.0f * script_camera::k_FullTurn));
}

TEST(ScriptCamera, TheTwoThingCameraStandsOnTheGamesSideOfTheLine)
{
	// The game turns its eighth of a turn from the line's direction measured as atan2(x, -z): along +z that is half a
	// turn and along -z none, so the camera stands back on the -x, -z side of a line along +z and on the +x, +z side of
	// one along -z
	const script_camera::DualThing first {.position = {0.0f, 0.0f, 0.0f}, .height = 0.0f, .radius = 0.0f};
	const script_camera::DualThing north {.position = {0.0f, 0.0f, 100.0f}, .height = 0.0f, .radius = 0.0f};
	const script_camera::DualThing south {.position = {0.0f, 0.0f, -100.0f}, .height = 0.0f, .radius = 0.0f};
	script_camera::DualSettings settings;
	const auto alongZ = script_camera::DualView(first, north, settings);
	EXPECT_LT(alongZ.origin.x - alongZ.focus.x, 0.0f);
	EXPECT_LT(alongZ.origin.z - alongZ.focus.z, 0.0f);
	const auto backAlongZ = script_camera::DualView(first, south, settings);
	EXPECT_GT(backAlongZ.origin.x - backAlongZ.focus.x, 0.0f);
	EXPECT_GT(backAlongZ.origin.z - backAlongZ.focus.z, 0.0f);
}

TEST(ScriptCamera, TheTwoThingCamerasGlidesShortenOverItsFirstSecondAndAHalf)
{
	EXPECT_FLOAT_EQ(script_camera::DualSeconds(0.0f), 2.0f);
	EXPECT_FLOAT_EQ(script_camera::DualSeconds(0.75f), 1.5f);
	EXPECT_FLOAT_EQ(script_camera::DualSeconds(1.5f), 1.0f);
	EXPECT_FLOAT_EQ(script_camera::DualSeconds(3.0f), 1.0f);
}

TEST(ScriptCamera, TheTwoThingCameraFramesBothFromTheSideOfTheLineBetweenThem)
{
	const script_camera::DualThing first {.position = {0.0f, 0.0f, 0.0f}, .height = 10.0f, .radius = 5.0f};
	const script_camera::DualThing second {.position = {100.0f, 0.0f, 0.0f}, .height = 20.0f, .radius = 6.0f};
	script_camera::DualSettings settings;
	const auto view = script_camera::DualView(first, second, settings);
	// Between them, raised by a quarter of their heights
	EXPECT_FLOAT_EQ(view.focus.x, 50.0f);
	EXPECT_FLOAT_EQ(view.focus.y, 7.5f);
	EXPECT_FLOAT_EQ(view.focus.z, 0.0f);
	// As far as they are apart plus their radii, then 1.4 times the taller's height
	EXPECT_NEAR(glm::distance(view.origin, view.focus), 111.0f + 28.0f, 1e-3f);
	// An eighth of a turn round from the line between them, which runs along +x (a quarter turn), and an eighth of a half
	// turn up
	const auto expected = script_camera::PointFromDistanceHeadingAndPitch(view.focus, 139.0f, -std::numbers::pi_v<float> / 4.0f,
	                                                                      std::numbers::pi_v<float> / 8.0f);
	EXPECT_NEAR(glm::distance(view.origin, expected), 0.0f, 1e-3f);

	// It looks down at least a thirteenth of a half turn
	settings.pitch = 0.0f;
	static_cast<void>(script_camera::DualView(first, second, settings));
	EXPECT_FLOAT_EQ(settings.pitch, script_camera::k_DualLowestPitch);

	// Two things in the same spot keep the heading as it is
	script_camera::DualSettings same;
	const auto together = script_camera::DualView(first, first, same);
	const auto unturned =
	    script_camera::PointFromDistanceHeadingAndPitch(together.focus, 10.0f + 14.0f, same.heading, same.pitch);
	EXPECT_NEAR(glm::distance(together.origin, unturned), 0.0f, 1e-3f);
}

TEST(ScriptCamera, ATwoThingCameraTakesOverUntilLetGoOrAThingIsGone)
{
	Camera camera;
	ScriptCameraModel model({2500.0f, 50.0f, 2500.0f}, {2600.0f, 0.0f, 2500.0f}, FlatLand);
	bool secondThere = true;
	const auto subject = [](uint32_t id, glm::vec3 at, const bool* there) {
		return ScriptCameraModel::DualSubject {.id = id, .lookup = [at, there]() -> std::optional<script_camera::DualThing> {
			                                       if (there != nullptr && !*there)
			                                       {
				                                       return std::nullopt;
			                                       }
			                                       return script_camera::DualThing {
			                                           .position = at, .height = 10.0f, .radius = 5.0f};
		                                       }};
	};
	model.StartDualView(subject(1, {2400.0f, 0.0f, 2400.0f}, nullptr), subject(2, {2440.0f, 0.0f, 2400.0f}, &secondThere));
	EXPECT_TRUE(model.InDualView());
	static_cast<void>(model.Update(std::chrono::milliseconds(16), camera));
	// It is sent to frame both, and hasn't arrived
	EXPECT_NEAR(model.GetTargetFocus().x, 2420.0f, 1e-3f);
	EXPECT_FALSE(model.Arrived());
	for (int i = 0; i < 40; ++i)
	{
		static_cast<void>(model.Update(std::chrono::milliseconds(100), camera));
	}
	EXPECT_TRUE(model.Arrived());

	// The same two again change nothing; others take over on top, and letting go goes back to the one below
	model.StartDualView(subject(1, {2400.0f, 0.0f, 2400.0f}, nullptr), subject(2, {2440.0f, 0.0f, 2400.0f}, &secondThere));
	model.StartDualView(subject(1, {2400.0f, 0.0f, 2400.0f}, nullptr), subject(3, {2400.0f, 0.0f, 2480.0f}, nullptr));
	static_cast<void>(model.Update(std::chrono::milliseconds(16), camera));
	EXPECT_NEAR(model.GetTargetFocus().z, 2440.0f, 1e-3f);
	EXPECT_TRUE(model.ReleaseDualView());
	static_cast<void>(model.Update(std::chrono::milliseconds(16), camera));
	EXPECT_NEAR(model.GetTargetFocus().x, 2420.0f, 1e-3f);
	EXPECT_TRUE(model.InDualView());

	// A thing gone is framed where it was last seen until the turn lets go of it; then the script's own camera holds
	// where it was sent
	secondThere = false;
	const auto held = model.GetTargetFocus();
	static_cast<void>(model.Update(std::chrono::milliseconds(16), camera));
	EXPECT_TRUE(model.InDualView());
	EXPECT_NEAR(glm::distance(model.GetTargetFocus(), held), 0.0f, 1e-4f);
	model.LetGoOfGoneThings();
	EXPECT_FALSE(model.InDualView());
	static_cast<void>(model.Update(std::chrono::milliseconds(16), camera));
	EXPECT_NEAR(glm::distance(model.GetTargetFocus(), held), 0.0f, 1e-4f);
	EXPECT_FALSE(model.ReleaseDualView());
}

TEST(ScriptCamera, ATrackHoldsUnderATwoThingCamera)
{
	Camera camera;
	ScriptCameraModel model({2400.0f, 60.0f, 2500.0f}, {2400.0f, 10.0f, 2550.0f}, FlatLand);
	model.RunTrack(StraightTrack());
	const auto thing = [](glm::vec3 at) {
		return [at]() { return std::optional(script_camera::DualThing {.position = at, .height = 1.0f, .radius = 1.0f}); };
	};
	model.StartDualView({.id = 1, .lookup = thing({2400.0f, 0.0f, 2400.0f})},
	                    {.id = 2, .lookup = thing({2410.0f, 0.0f, 2400.0f})});
	model.PassGameTime(std::chrono::milliseconds(5000));
	EXPECT_TRUE(model.ReleaseDualView());
	// Its time didn't run while it was held, so it hasn't arrived at its end
	EXPECT_FALSE(model.Arrived());
}
