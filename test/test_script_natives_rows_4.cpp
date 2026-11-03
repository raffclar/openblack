/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/ScriptCameraModel.h"
#include "ScriptHeaders/ScriptTimers.h"

using namespace openblack;
using openblack::script::timers::CountdownTimer;

namespace
{

constexpr glm::vec3 k_Start {2500.0f, 80.0f, 2500.0f};
constexpr glm::vec3 k_StartFocus {2550.0f, 80.0f, 2550.0f};
constexpr glm::vec3 k_Thing {2520.0f, 20.0f, 2610.0f};
constexpr glm::vec3 k_Point {2600.0f, 15.0f, 2610.0f};

ScriptCameraModel MakeModel()
{
	return {k_Start, k_StartFocus, [](float, float) { return 0.0f; }};
}

ScriptCameraModel::Subject FollowedAt(glm::vec3 place)
{
	return {.id = 7, .lookup = [place]() -> std::optional<script_camera::FollowedThing> {
		        return script_camera::FollowedThing {.point = place, .height = 10.0f};
	        }};
}

ScriptCameraModel::DualSubject DualAt(uint32_t id, glm::vec3 place)
{
	return {.id = id,
	        .lookup = [place]() -> std::optional<script_camera::DualThing> {
		        return script_camera::DualThing {.position = place, .height = 10.0f, .radius = 5.0f};
	        },
	        .point = std::nullopt};
}

} // namespace

TEST(ScriptCountdownTimer, CountsTheTurnsOfTheSecondsDownToZero)
{
	CountdownTimer timer;
	EXPECT_FALSE(timer.IsRunning());
	EXPECT_FALSE(timer.IsVisible());
	timer.Start(2.5f);
	EXPECT_TRUE(timer.IsRunning());
	EXPECT_TRUE(timer.IsVisible());
	EXPECT_EQ(timer.GetTurnsLeft(), 25u);
	// The seconds left are whole seconds
	EXPECT_EQ(timer.GetSecondsLeft(), 2.0f);
	for (int turn = 0; turn < 24; ++turn)
	{
		timer.ProcessTurn();
	}
	EXPECT_TRUE(timer.IsRunning());
	EXPECT_EQ(timer.GetTurnsLeft(), 1u);
	EXPECT_EQ(timer.GetSecondsLeft(), 0.0f);
	timer.ProcessTurn();
	EXPECT_FALSE(timer.IsRunning());
	EXPECT_EQ(timer.GetTurnsLeft(), 0u);
	// A timer that stopped does not count
	timer.ProcessTurn();
	EXPECT_EQ(timer.GetTurnsLeft(), 0u);
	// It stays visible: only the scripts hide it
	EXPECT_TRUE(timer.IsVisible());
}

TEST(ScriptCountdownTimer, CutsTheTurnsToWholeTurns)
{
	CountdownTimer timer;
	timer.Start(0.15f);
	EXPECT_EQ(timer.GetTurnsLeft(), 1u);
	timer.Start(0.09f);
	EXPECT_EQ(timer.GetTurnsLeft(), 0u);
}

TEST(ScriptCountdownTimer, ATimerOfNoTurnsWrapsAndRunsOn)
{
	for (const float seconds : {0.0f, -3.0f})
	{
		CountdownTimer timer;
		timer.Start(seconds);
		EXPECT_EQ(timer.GetTurnsLeft(), 0u);
		timer.ProcessTurn();
		EXPECT_TRUE(timer.IsRunning());
		EXPECT_EQ(timer.GetTurnsLeft(), 0xFFFFFFFFu);
		EXPECT_EQ(timer.GetSecondsLeft(), static_cast<float>(0xFFFFFFFFu / 10u));
	}
}

TEST(ScriptCountdownTimer, RemoveHideRevealAndReset)
{
	CountdownTimer timer;
	timer.Start(10.0f);
	timer.Hide();
	EXPECT_FALSE(timer.IsVisible());
	EXPECT_TRUE(timer.IsRunning());
	timer.Reveal();
	EXPECT_TRUE(timer.IsVisible());
	timer.Remove();
	EXPECT_FALSE(timer.IsRunning());
	// A removed timer keeps its turns, and the scripts still read its seconds
	EXPECT_EQ(timer.GetSecondsLeft(), 10.0f);
	timer.ProcessTurn();
	EXPECT_EQ(timer.GetTurnsLeft(), 100u);
	timer.Start(5.0f);
	timer.Reset();
	EXPECT_FALSE(timer.IsRunning());
	EXPECT_FALSE(timer.IsVisible());
	EXPECT_EQ(timer.GetTurnsLeft(), 50u);
}

TEST(ScriptDualCameraWithPoint, APointCountsAsALittleHighForTheMiddleAndFlatForTheDistance)
{
	const script_camera::DualThing thing {.position = {0.0f, 0.0f, 0.0f}, .height = 10.0f, .radius = 4.0f};
	const script_camera::DualThing point {
	    .position = {100.0f, 5.0f, 0.0f}, .height = 0.0f, .radius = script_camera::k_DualNoModelRadius, .point = true};
	script_camera::DualSettings settings {.zoom = script_camera::k_DualPointZoom};
	const auto view = script_camera::DualView(thing, point, settings);
	// Half of the way, raised by half of the mean of 10 and 1
	EXPECT_FLOAT_EQ(view.focus.x, 50.0f);
	EXPECT_FLOAT_EQ(view.focus.y, 2.5f + 2.75f);
	EXPECT_FLOAT_EQ(view.focus.z, 0.0f);
	// As far back as the gap and the radii, 1.2 times, then 1.4 times the height of the thing
	const float distance = ((100.0f + 30.0f + 4.0f) * 1.2f) + (10.0f * 1.4f);
	EXPECT_NEAR(glm::distance(view.origin, view.focus), distance, 0.01f);
}

TEST(ScriptDualCameraWithPoint, TheSameThingAndPointAgainChangeNothing)
{
	auto model = MakeModel();
	model.StartDualViewWithPoint(DualAt(3, k_Thing), k_Point);
	model.StartDualViewWithPoint(DualAt(3, k_Thing), k_Point);
	ASSERT_TRUE(model.InDualView());
	EXPECT_TRUE(model.ReleaseDualView());
	EXPECT_FALSE(model.InDualView());
	// Another point puts a second camera on top
	model.StartDualViewWithPoint(DualAt(3, k_Thing), k_Point);
	model.StartDualViewWithPoint(DualAt(3, k_Thing), k_Point + glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_TRUE(model.ReleaseDualView());
	EXPECT_TRUE(model.InDualView());
	// Two things after a thing and a point put a camera on top too
	model.StartDualView(DualAt(3, k_Thing), DualAt(0, k_Point));
	EXPECT_TRUE(model.ReleaseDualView());
	EXPECT_TRUE(model.InDualView());
}

TEST(ScriptDualCameraWithPoint, ASaveKeepsThePoint)
{
	auto model = MakeModel();
	model.StartDualViewWithPoint(DualAt(3, k_Thing), k_Point);
	const auto saved = model.GetSaved();
	ASSERT_EQ(saved.duals.size(), 1u);
	EXPECT_EQ(saved.duals.front().secondPoint, std::optional(k_Point));
	EXPECT_EQ(saved.duals.front().settings.zoom, script_camera::k_DualPointZoom);

	auto loaded = MakeModel();
	ScriptCameraModel::Lookups lookups;
	lookups.dual = [](uint32_t id) { return DualAt(id, k_Thing).lookup; };
	loaded.Restore(saved, lookups);
	ASSERT_TRUE(loaded.InDualView());
	// The point is there after the load: the camera does not let go of it
	loaded.LetGoOfGoneThings();
	EXPECT_EQ(loaded.GetSaved().duals.size(), 1u);
	EXPECT_EQ(loaded.GetSaved().duals.front().secondPoint, std::optional(k_Point));
}

TEST(ScriptFollowCamera, AFollowThatIsNotAtOnceLeavesTheCameraWhereItIs)
{
	auto model = MakeModel();
	model.Follow(FollowedAt(k_Thing), false);
	EXPECT_EQ(model.GetOrigin(), k_Start);
	EXPECT_EQ(model.GetFocus(), k_StartFocus);
	EXPECT_EQ(model.GetFocusedThing(), std::optional<uint32_t>(7));

	model.FollowAndLookAt(FollowedAt(k_Thing), 50.0f, false);
	EXPECT_EQ(model.GetOrigin(), k_Start);
	EXPECT_EQ(model.GetFocus(), k_StartFocus);

	model.FollowAndLookAt(FollowedAt(k_Thing), 50.0f, true);
	EXPECT_EQ(model.GetFocus(), k_Thing);
	EXPECT_NE(model.GetOrigin(), k_Start);
}
