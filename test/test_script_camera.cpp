/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// script_camera:: (src/Camera/ScriptCamera.h) against the original: the script camera mode and its exit,
// MoveCameraPosition, the mode's arrival, the camera update (0.1 s cap, disc of the world,
// the drawn camera's nudge and ground clearance), the FOV setting and its reset; the follows of
// the follow mode (set, update, validate), the camera
// helpers and SET/MOVE_CAMERA_TO_FACE_OBJECT

#include <cmath>

#include <functional>
#include <map>
#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Camera/ScriptCamera.h"

using namespace openblack;

namespace
{
std::function<float(float, float)> Ground(float height)
{
	return [height](float, float) { return height; };
}

void Frames(int n, float seconds)
{
	for (int i = 0; i < n; ++i)
	{
		script_camera::Frame(seconds, 0, 0.0f);
	}
}

/// The things of a test, read through script_camera::detail::SetThingReaderForTests
struct Things
{
	std::map<entt::entity, script_camera::ThingInfo> things;

	Things()
	{
		script_camera::Reset();
		script_camera::detail::SetThingReaderForTests([this](entt::entity e) -> std::optional<script_camera::ThingInfo> {
			const auto it = things.find(e);
			return it != things.end() ? std::optional(it->second) : std::nullopt;
		});
	}
	~Things()
	{
		script_camera::detail::SetThingReaderForTests(nullptr);
		script_camera::Reset();
	}
	Things(const Things&) = delete;
	Things& operator=(const Things&) = delete;
	Things(Things&&) = delete;
	Things& operator=(Things&&) = delete;
};

constexpr auto k_Thing = static_cast<entt::entity>(7);
constexpr auto k_Other = static_cast<entt::entity>(8);

/// A thing drawn 5 m east of its MapCoords
script_camera::ThingInfo Villager(glm::vec3 mapPoint, float height)
{
	script_camera::ThingInfo info;
	info.mapPoint = mapPoint;
	info.height = height;
	info.drawnPoint = mapPoint + glm::vec3(5.0f, 0.0f, 0.0f);
	return info;
}

void ExpectNear(const glm::vec3& a, const glm::vec3& b, float tolerance = 1e-3f)
{
	EXPECT_NEAR(a.x, b.x, tolerance);
	EXPECT_NEAR(a.y, b.y, tolerance);
	EXPECT_NEAR(a.z, b.z, tolerance);
}
} // namespace

TEST(ScriptCamera, OneScriptModeAtATime)
{
	script_camera::Reset();
	EXPECT_FALSE(script_camera::Active());
	EXPECT_TRUE(script_camera::Begin({0, 10, 0}, {0, 0, 10}));
	EXPECT_TRUE(script_camera::Active());
	EXPECT_FALSE(script_camera::Begin({0, 10, 0}, {0, 0, 10})); // the script mode cannot exit while it lives
	EXPECT_TRUE(script_camera::End());
	EXPECT_FALSE(script_camera::Active());
	EXPECT_FALSE(script_camera::End()); // no script mode: only the FOV goes back
	EXPECT_TRUE(script_camera::Begin({0, 10, 0}, {0, 0, 10}));
	script_camera::Reset();
}

TEST(ScriptCamera, BeginStartsFromTheDrawnCamera)
{
	script_camera::Reset();
	script_camera::Begin({100, 20, 50}, {110, 5, 60});
	EXPECT_TRUE(script_camera::ScriptArrived()); // nothing to move to
	const auto drawn = script_camera::DrawnCamera(Ground(0.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(100, 20, 50));
	EXPECT_EQ(drawn.focus, glm::vec3(110, 5, 60));
	script_camera::Reset();
}

TEST(ScriptCamera, MoveArrivesAfterItsTime)
{
	script_camera::Reset();
	script_camera::Begin({1000, 50, 1000}, {1100, 0, 1100});
	script_camera::MovePosition({1200, 80, 900}, 2.0f);
	script_camera::MoveFocus({1300, 10, 1000}, 1.0f);
	EXPECT_FALSE(script_camera::ScriptArrived());
	Frames(19, 0.1f); // 1.9 s
	EXPECT_FALSE(script_camera::ScriptArrived());
	Frames(2, 0.1f); // Zoomer::Update: at t >= T the value is the destination
	EXPECT_TRUE(script_camera::ScriptArrived());
	const auto drawn = script_camera::DrawnCamera(Ground(0.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(1200, 80, 900));
	EXPECT_EQ(drawn.focus, glm::vec3(1300, 10, 1000));
	script_camera::Reset();
}

TEST(ScriptCamera, CameraSecondsCappedAtATenth)
{
	script_camera::Reset();
	script_camera::Begin({1000, 50, 1000}, {1100, 0, 1100});
	script_camera::MovePosition({1010, 50, 1000}, 1.0f);
	Frames(5, 1.0f); // 5 frames of 1 s are 0.5 s
	EXPECT_FALSE(script_camera::ScriptArrived());
	Frames(6, 1.0f);
	EXPECT_TRUE(script_camera::ScriptArrived());
	script_camera::Reset();
}

TEST(ScriptCamera, ShortTimeSetsAtOnce)
{
	script_camera::Reset();
	script_camera::Begin({1000, 50, 1000}, {1100, 0, 1100});
	script_camera::MovePosition({1500, 60, 1500}, 0.0005f); // T < 0.001
	EXPECT_TRUE(script_camera::ScriptArrived());
	EXPECT_EQ(script_camera::DrawnCamera(Ground(0.0f)).origin, glm::vec3(1500, 60, 1500));
	script_camera::Reset();
}

TEST(ScriptCamera, DiscOfTheWorld)
{
	script_camera::Reset();
	script_camera::Begin({2560, 50, 2560}, {2600, 0, 2600});
	script_camera::MovePosition({2560 + 7000, 50, 2560}, 1.0f);
	Frames(1, 0.01f);
	// the destination pulled back to d / (|d| 0.000285796) + centre in 3 s
	const auto destination = script_camera::Get().position.GetDestination();
	const float d = std::sqrt(7000.0f * 7000.0f + 50.0f * 50.0f);
	EXPECT_NEAR(destination.x, 2560.0f + 7000.0f / (d * script_camera::k_DiscScale), 0.05f);
	EXPECT_NEAR(destination.y, 50.0f / (d * script_camera::k_DiscScale), 0.01f);
	EXPECT_NEAR(script_camera::Get().position.axis[0].duration, 3.0f, 1e-6f);
	script_camera::Reset();
}

TEST(ScriptCamera, DrawnCamera)
{
	script_camera::Reset();
	script_camera::Begin({100, 5, 100}, {100.01f, 5, 100});
	// the same point -> position.x - 1, y + 1
	auto drawn = script_camera::DrawnCamera(Ground(-100.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(99, 6, 100));
	// 1 m above the ground, the focus lifted as much
	script_camera::SetPositionAndFocus({100, 5, 100}, {150, 2, 100});
	drawn = script_camera::DrawnCamera(Ground(10.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(100, 11, 100));
	EXPECT_EQ(drawn.focus, glm::vec3(150, 8, 100));
	drawn = script_camera::DrawnCamera(Ground(3.0f));
	EXPECT_EQ(drawn.origin, glm::vec3(100, 5, 100));
	script_camera::Reset();
}

TEST(ScriptCamera, FovFollowsGameTime)
{
	script_camera::Reset();
	auto& fov = script_camera::Get().fov;
	EXPECT_FLOAT_EQ(fov.value, script_camera::k_DefaultFov);
	script_camera::SetFov(40.0f * script_camera::k_DegreesToRadians, 1.0f);
	script_camera::Frame(0.1f, 0, 0.0f); // paused: no game time passes
	EXPECT_FLOAT_EQ(fov.value, script_camera::k_DefaultFov);
	for (int i = 0; i < 11; ++i)
	{
		script_camera::Frame(0.0f, 100, 0.1f);
	}
	EXPECT_FLOAT_EQ(fov.value, 40.0f * script_camera::k_DegreesToRadians);
	script_camera::Begin({0, 10, 0}, {0, 0, 10});
	script_camera::End(); // 70 degrees in 0.5 s
	EXPECT_FLOAT_EQ(fov.destination, script_camera::k_DefaultFov);
	EXPECT_FLOAT_EQ(fov.duration, 0.5f);
	script_camera::Reset();
}

TEST(ScriptCamera, SetDropsTheMove)
{
	script_camera::Reset();
	script_camera::Begin({1000, 50, 1000}, {1100, 0, 1100});
	script_camera::MovePosition({1200, 80, 900}, 2.0f);
	script_camera::SetPosition({1050, 40, 1000}); // Zoomer::SetPosition
	EXPECT_TRUE(script_camera::ScriptArrived());
	script_camera::Reset();
}

// ---- The follow mode ---------------------------------------------------------------------------------------------------

TEST(ScriptCameraFollow, TimeRule)
{
	// (t > 2 ? 1 : t / 2 x (1 - 2) + 2) x factor
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(0.0f, 0.2f), 0.4f);
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(1.0f, 0.2f), 0.3f);
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(2.0f, 0.2f), 0.2f);
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(30.0f, 0.2f), 0.2f);
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(0.5f, 0.0f), 0.0f); // CAMERA_PROPERTIES speed 0: placed at once
}

TEST(ScriptCameraFollow, DistanceAndPitchLimits)
{
	EXPECT_FLOAT_EQ(script_camera::ClampFollowDistance(-5.0f), 2.0f);
	EXPECT_FLOAT_EQ(script_camera::ClampFollowDistance(2.0f), 2.0f);
	EXPECT_FLOAT_EQ(script_camera::ClampFollowDistance(80.0f), 80.0f);
	EXPECT_FLOAT_EQ(script_camera::ClampFollowDistance(1500.0f), 1500.0f);
	EXPECT_FLOAT_EQ(script_camera::ClampFollowDistance(9000.0f), 1500.0f);
	EXPECT_FLOAT_EQ(script_camera::FollowPitch(0.1f), script_camera::k_FollowMinPitch);
	EXPECT_FLOAT_EQ(script_camera::FollowPitch(script_camera::k_FollowMinPitch), script_camera::k_FollowMinPitch);
	EXPECT_FLOAT_EQ(script_camera::FollowPitch(0.6f), 0.6f);
	EXPECT_FLOAT_EQ(script_camera::k_FollowMinPitch, 0.241661f);
}

TEST(ScriptCameraFollow, HeadingBehindAWallHug)
{
	EXPECT_FLOAT_EQ(script_camera::FollowHeading(0.5f, false, uint16_t {0}), 0.5f);
	EXPECT_FLOAT_EQ(script_camera::FollowHeading(0.5f, true, std::nullopt), 0.5f);
	// heading - (2 a x pi / 2048 - pi / 2): a quarter turn (512) cancels the pi / 2
	EXPECT_NEAR(script_camera::FollowHeading(0.5f, true, uint16_t {512}), 0.5f, 1e-6f);
	EXPECT_NEAR(script_camera::FollowHeading(0.5f, true, uint16_t {0}), 0.5f + 1.5707964f, 1e-6f);
	EXPECT_NEAR(script_camera::FollowHeading(0.0f, true, uint16_t {1024}), -1.5707964f, 1e-6f);
}

TEST(ScriptCameraFollow, PointFromDistanceHeadingAndPitch)
{
	// p + d (sin h cos q, sin q, cos h cos q)
	ExpectNear(script_camera::PointFromDistanceHeadingAndPitch({10, 0, 10}, 10.0f, 0.0f, 0.0f), {10, 0, 20});
	ExpectNear(script_camera::PointFromDistanceHeadingAndPitch({10, 0, 10}, 10.0f, 1.5707964f, 0.0f), {20, 0, 10});
	ExpectNear(script_camera::PointFromDistanceHeadingAndPitch({10, 0, 10}, 10.0f, 0.0f, 1.5707964f), {10, 10, 10});
	const float h = 2.0f;
	const float q = 0.4f;
	ExpectNear(script_camera::PointFromDistanceHeadingAndPitch({1, 2, 3}, 5.0f, h, q),
	           {1 + 5 * std::sin(h) * std::cos(q), 2 + 5 * std::sin(q), 3 + 5 * std::cos(h) * std::cos(q)});
	EXPECT_FLOAT_EQ(script_camera::ThingViewingDistance(1.5f), 12.0f); // height x 8
}

TEST(ScriptCameraFollow, ArcTan2ByOctants)
{
	// affine::ArcTanOctant(x, y), stored as a float by the camera, is atan2(y, x) in each octant
	for (const float x : {-3.0f, -1.0f, -0.25f, 0.25f, 1.0f, 3.0f})
	{
		for (const float y : {-3.0f, -1.0f, -0.25f, 0.0f, 0.25f, 1.0f, 3.0f})
		{
			EXPECT_NEAR(static_cast<float>(affine::ArcTanOctant(x, y)), std::atan2(y, x), 1e-5f) << x << " " << y;
		}
	}
}

TEST(ScriptCameraFollow, HeadingAndPitchFromPoints)
{
	// HeadingAndPitchFromPoints undoes PointFromDistanceHeadingAndPitch, the heading in [0, 2 pi)
	for (const float h : {0.0f, 0.3f, 2.0f, 4.0f, 6.0f})
	{
		const glm::vec3 focus(100, 5, 200);
		const auto position = script_camera::PointFromDistanceHeadingAndPitch(focus, 30.0f, h, 0.4f);
		float heading = -1.0f;
		float pitch = -1.0f;
		script_camera::HeadingAndPitchFromPoints(position, focus, heading, pitch);
		EXPECT_NEAR(heading, h, 1e-4f) << h;
		EXPECT_NEAR(pitch, 0.4f, 1e-4f) << h;
	}
	// straight above (|dx|, |dz| < 0.01) -> heading 0, pitch 1.5393804
	float heading = -1.0f;
	float pitch = -1.0f;
	script_camera::HeadingAndPitchFromPoints({50.005f, 80, 50}, {50, 0, 50}, heading, pitch);
	EXPECT_EQ(heading, 0.0f);
	EXPECT_FLOAT_EQ(pitch, 1.5393804f);
}

TEST(ScriptCameraFollow, PointOfAThing)
{
	auto villager = Villager({1, 2, 3}, 4.0f);
	// the MapCoords point, or for the update the drawn point; + half the height either way
	ExpectNear(script_camera::FollowPoint(villager, false), {1, 4, 3});
	ExpectNear(script_camera::FollowPoint(villager, true), {6, 4, 3});
	villager.drawnPoint.reset();
	ExpectNear(script_camera::FollowPoint(villager, true), {1, 4, 3});
	// a flock: GetFlockPos and half its leader's height, none without a leader
	script_camera::ThingInfo flock;
	flock.isFlock = true;
	flock.mapPoint = {50, 0, 50};
	flock.flockPoint = {10, 1, 20};
	ExpectNear(script_camera::FollowPoint(flock, true), {10, 1, 20});
	flock.leaderHeight = 3.0f;
	ExpectNear(script_camera::FollowPoint(flock, false), {10, 2.5f, 20});
	// facing: the MapCoords point of the thing itself
	ExpectNear(script_camera::FacePoint(flock), {50, 0, 50});
}

TEST(ScriptCameraFollow, FacePosition)
{
	// the facing less 2 pi while > 2 pi, pitch 0.1
	const glm::vec3 focus(100, 10, 100);
	const float h = 7.0f - script_camera::k_TwoPi;
	ExpectNear(script_camera::FacePosition(focus, 7.0f, 20.0f),
	           script_camera::PointFromDistanceHeadingAndPitch(focus, 20.0f, h, 0.1f));
	ExpectNear(script_camera::FacePosition(focus, -1.0f, 20.0f), // a negative heading is kept
	           script_camera::PointFromDistanceHeadingAndPitch(focus, 20.0f, -1.0f, 0.1f));
}

TEST(ScriptCameraFollow, FaceObject)
{
	Things test;
	EXPECT_FALSE(script_camera::FaceObject(k_Thing, 10.0f).has_value()); // "no object to face"
	auto info = Villager({300, 10, 300}, 2.0f);
	info.facingDirection = 1.0f;
	test.things[k_Thing] = info;
	const auto points = script_camera::FaceObject(k_Thing, 10.0f);
	ASSERT_TRUE(points.has_value());
	ExpectNear(points->focus, {300, 11, 300}); // the MapCoords point, not the drawn one
	ExpectNear(points->position, script_camera::PointFromDistanceHeadingAndPitch({300, 11, 300}, 10.0f, 1.0f, 0.1f));
}

TEST(ScriptCameraFollow, PositionFollowTakesTheCurrentAngles)
{
	Things test;
	test.things[k_Thing] = Villager({300, 10, 300}, 2.0f);
	script_camera::Begin({100, 50, 100}, {200, 0, 100});
	auto& state = script_camera::Get();
	EXPECT_EQ(state.timeFactor, 0.2f); // the mode's default
	EXPECT_TRUE(state.behind);
	script_camera::PositionFollow(k_Thing);
	EXPECT_EQ(state.positionThing, k_Thing);
	EXPECT_FLOAT_EQ(state.distance, 16.0f);           // the thing's viewing distance: 2 x 8
	EXPECT_EQ(state.heading, 0.0f);                   // "behind"
	EXPECT_NEAR(state.pitch, std::atan(0.5f), 1e-5f); // from the zoomers' destinations
	// FocusAndPositionFollow keeps the heading and takes the distance given
	script_camera::FocusAndPositionFollow(k_Thing, 30.0f);
	EXPECT_FLOAT_EQ(state.distance, 30.0f);
	EXPECT_NEAR(state.heading, 3.0f * 1.5707964f, 1e-5f);
}

TEST(ScriptCameraFollow, PlaceNowThenFollow)
{
	Things test;
	test.things[k_Thing] = Villager({300, 10, 300}, 2.0f);
	script_camera::Begin({100, 50, 100}, {200, 0, 100});
	auto& state = script_camera::Get();
	script_camera::PositionFollow(k_Thing);
	const float pitch = state.pitch;
	script_camera::PlaceFollowNow(); // SET_POSITION_FOLLOW
	EXPECT_FLOAT_EQ(state.modeSeconds, 2.0f);
	EXPECT_TRUE(script_camera::ScriptArrived());
	// the focus follows the position thing, on the MapCoords point
	ExpectNear(state.focus.GetCurrentValue(), {300, 11, 300});
	ExpectNear(state.position.GetCurrentValue(),
	           script_camera::PointFromDistanceHeadingAndPitch({300, 11, 300}, 16.0f, 0.0f, pitch));
	// then every frame both head for the drawn point in T = 0.2 s (2.1 s after the mode change)
	script_camera::Frame(0.1f, 0, 0.0f);
	ExpectNear(state.focus.GetDestination(), {305, 11, 300});
	ExpectNear(state.position.GetDestination(),
	           script_camera::PointFromDistanceHeadingAndPitch({305, 11, 300}, 16.0f, 0.0f, pitch));
	EXPECT_FLOAT_EQ(state.focus.axis[0].duration, 0.2f);
	EXPECT_FALSE(script_camera::ScriptArrived());
}

TEST(ScriptCameraFollow, FollowStartsSlower)
{
	Things test;
	test.things[k_Thing] = Villager({300, 10, 300}, 2.0f);
	script_camera::Begin({100, 50, 100}, {200, 0, 100});
	script_camera::FocusFollow(k_Thing);
	script_camera::Frame(0.1f, 0, 0.0f); // 0.1 s after the mode change: (0.05 x -1 + 2) x 0.2
	EXPECT_FLOAT_EQ(script_camera::Get().focus.axis[0].duration, 0.39f);
	EXPECT_EQ(script_camera::Get().position.GetDestination(), glm::vec3(100, 50, 100)); // no position thing
}

TEST(ScriptCameraFollow, CameraPropertiesWithoutSpeedPlaceAtOnce)
{
	Things test;
	auto info = Villager({300, 10, 300}, 2.0f);
	info.wallHugAngle = uint16_t {0};
	test.things[k_Thing] = info;
	script_camera::Begin({100, 50, 100}, {200, 0, 100});
	script_camera::PositionFollow(k_Thing);
	const float pitch = script_camera::Get().pitch;
	script_camera::SetFollowProperties(5000.0f, 0.0f, 0.25f, true); // CAMERA_PROPERTIES(5000, 0, 14.3, true)
	script_camera::Frame(0.05f, 0, 0.0f);
	auto& state = script_camera::Get();
	EXPECT_FLOAT_EQ(state.distance, 1500.0f);
	ExpectNear(state.focus.GetCurrentValue(), {305, 11, 300});
	// behind a wall hug at angle 0: heading 0.25 + pi / 2
	ExpectNear(state.position.GetCurrentValue(),
	           script_camera::PointFromDistanceHeadingAndPitch({305, 11, 300}, 1500.0f, 0.25f + 1.5707964f, pitch), 0.05f);
}

TEST(ScriptCameraFollow, AThingThatGoesIsDropped)
{
	Things test;
	test.things[k_Thing] = Villager({300, 10, 300}, 2.0f);
	test.things[k_Other] = Villager({400, 10, 300}, 2.0f);
	script_camera::Begin({100, 50, 100}, {200, 0, 100});
	script_camera::FocusFollow(k_Other);
	script_camera::PositionFollow(k_Thing);
	test.things.erase(k_Other);
	script_camera::Validate();           // once a turn
	script_camera::Frame(0.1f, 0, 0.0f); // after Validate dropped the focus thing: the focus goes to the position thing
	auto& state = script_camera::Get();
	EXPECT_TRUE(state.focusThing == entt::null);
	ExpectNear(state.focus.GetDestination(), {305, 11, 300});
	test.things.erase(k_Thing);
	script_camera::Validate();
	script_camera::Frame(0.1f, 0, 0.0f); // the position thing dropped, the zoomers keep their destinations
	EXPECT_TRUE(state.positionThing == entt::null);
	ExpectNear(state.focus.GetDestination(), {305, 11, 300});
	// FOCUS_FOLLOW of a thing that is not there follows nothing
	script_camera::FocusFollow(k_Other);
	EXPECT_TRUE(state.focusThing == entt::null);
}

TEST(ScriptCameraFollow, SetAndMoveDropTheFollow)
{
	Things test;
	test.things[k_Thing] = Villager({300, 10, 300}, 2.0f);
	script_camera::Begin({100, 50, 100}, {200, 0, 100});
	auto& state = script_camera::Get();
	script_camera::FocusAndPositionFollow(k_Thing, 30.0f);
	script_camera::FocusFollow(k_Thing);
	EXPECT_NE(state.heading, 0.0f);
	script_camera::MovePosition({120, 50, 100}, 1.0f); // the position follow cleared, and with "behind" the heading is 0
	EXPECT_TRUE(state.positionThing == entt::null);
	EXPECT_EQ(state.heading, 0.0f);
	EXPECT_EQ(state.focusThing, k_Thing);
	script_camera::SetFocus({0, 0, 0}); // the focus follow cleared
	EXPECT_TRUE(state.focusThing == entt::null);
}
