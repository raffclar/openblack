/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' timers (ECS/ScriptTimer.h: the script timers and the script's countdown), the help profile's event
// counters (Help/HelpProfile.h: Trigger, Process, CameraHelpCallback, GET_TOTAL_EVENTS) and the field of view tests
// (Graphics/RegionOnScreen.h and Camera/FieldOfView.h).

#include <optional>

#include <gtest/gtest.h>

#include "Camera/FieldOfView.h"
#include "ECS/ScriptTimer.h"
#include "GameClock.h"
#include "Graphics/RegionOnScreen.h"
#include "Help/HelpProfile.h"

namespace st = openblack::ecs::script_timer;
namespace cd = openblack::ecs::script_countdown;
namespace hp = openblack::help_profile;
namespace fov = openblack::field_of_view;
namespace ros = openblack::graphics::region_on_screen;
using openblack::ecs::components::ScriptTimer;

namespace
{
class TimersEventsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		openblack::game_clock::Reset();
		cd::Reset();
		hp::Reset();
		hp::Queries queries;
		queries.paused = [this] { return paused; };
		queries.scriptWideScreen = [this] { return wideScreen; };
		hp::SetQueries(queries);
	}
	// Drop the queries that capture this fixture and leave the modules clean for the next test
	void TearDown() override
	{
		hp::SetQueries({});
		hp::Reset();
		cd::Reset();
		openblack::game_clock::Reset();
	}
	bool paused {false};
	bool wideScreen {false};
};
} // namespace

TEST_F(TimersEventsTest, TimerCountsGameTurns)
{
	ScriptTimer timer;
	st::SetSeconds(timer, 30.0f, 100); // TeachRotate's RotateTimer: 1000 / 100 x 30 = 300 turns
	EXPECT_EQ(timer.startTurn, 100u);
	EXPECT_EQ(timer.durationTurns, 300);
	EXPECT_FLOAT_EQ(st::RemainingSeconds(timer, 100, 100), 30.0f);
	EXPECT_FLOAT_EQ(st::RemainingSeconds(timer, 250, 100), 15.0f);
	// run out: exactly 0 (the scripts compare with == 0.0), never negative
	EXPECT_EQ(st::RemainingSeconds(timer, 400, 100), 0.0f);
	EXPECT_EQ(st::RemainingSeconds(timer, 500, 100), 0.0f);
	// the time since it was set keeps growing
	EXPECT_FLOAT_EQ(st::SecondsSinceSet(timer, 500, 100), 40.0f);
	// SET_TIMER_TIME restarts it from the current turn
	st::SetSeconds(timer, 2.0f, 500);
	EXPECT_EQ(timer.durationTurns, 20);
	EXPECT_FLOAT_EQ(st::RemainingSeconds(timer, 510, 100), 1.0f);
	EXPECT_EQ(st::SecondsSinceSet(timer, 500, 100), 0.0f);
	// truncated: 0.25 s is 2 turns
	st::SetSeconds(timer, 0.25f, 0);
	EXPECT_EQ(timer.durationTurns, 2);
	// a CREATE_TIMER(0) is already out
	st::SetSeconds(timer, 0.0f, 7);
	EXPECT_EQ(st::RemainingSeconds(timer, 7, 100), 0.0f);
}

TEST_F(TimersEventsTest, TimerSaveRecord)
{
	ScriptTimer timer {12, 34};
	const auto record = st::ToSave(timer);
	const auto back = st::FromSave(record);
	EXPECT_EQ(back.startTurn, 12u);
	EXPECT_EQ(back.durationTurns, 34);
}

TEST_F(TimersEventsTest, Countdown)
{
	EXPECT_FALSE(cd::Exists());
	EXPECT_TRUE(cd::Start(1.5f)); // 15 turns
	EXPECT_TRUE(cd::Exists());
	EXPECT_TRUE(cd::GetDisplay().visible);
	EXPECT_TRUE(cd::GetDisplay().red);
	EXPECT_EQ(cd::RemainingSeconds(), 1.0f); // whole seconds: 15 / 10
	for (int i = 0; i < 14; ++i)
	{
		cd::ProcessTurn();
	}
	EXPECT_TRUE(cd::Exists());
	cd::ProcessTurn();
	EXPECT_FALSE(cd::Exists());
	EXPECT_FALSE(cd::GetDisplay().visible);
	// HIDE / REVEAL only touch the shown flag
	EXPECT_TRUE(cd::Start(20.0f));
	EXPECT_FALSE(cd::GetDisplay().red);
	cd::SetShown(false);
	EXPECT_FALSE(cd::GetDisplay().visible);
	EXPECT_TRUE(cd::Exists());
	cd::Remove();
	EXPECT_FALSE(cd::Exists());
	// 0 is "Invalid time for timer" but the countdown starts anyway, and the first turn takes it below 0
	EXPECT_FALSE(cd::Start(0.0f));
	cd::ProcessTurn();
	EXPECT_TRUE(cd::Exists());
	EXPECT_EQ(cd::Get().turnsLeft, -1);
}

TEST_F(TimersEventsTest, EventsCountOncePerTurn)
{
	hp::Trigger(hp::Event::Rotate);
	hp::Trigger(hp::Event::Rotate);
	EXPECT_EQ(hp::TotalEvents(25).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(43).value(), 1.0f); // AllInterface (1..42)
	EXPECT_EQ(hp::TotalEvents(48).value(), 1.0f); // AllEvents
	EXPECT_EQ(hp::TotalEvents(24).value(), 0.0f); // GestureTotal is 14..23 only
	hp::Process();
	EXPECT_FLOAT_EQ(hp::Get(hp::Event::Rotate).rate, 0.005f);
	hp::Trigger(hp::Event::Rotate);
	EXPECT_EQ(hp::TotalEvents(25).value(), 2.0f);
	EXPECT_EQ(hp::AccumulatedTime(), 100u);
	// the readers' range is 1..48
	EXPECT_FALSE(hp::TotalEvents(0).has_value());
	EXPECT_FALSE(hp::TotalEvents(49).has_value());
	EXPECT_FALSE(hp::TotalEvents(-1).has_value());
}

TEST_F(TimersEventsTest, EventsGroups)
{
	hp::Trigger(hp::Event::SelectGesture); // 15: + GestureTotal, AllInterface, AllEvents
	hp::Trigger(hp::Event::CastSpell);     // 9: + CastAll, (AllInterface, AllEvents already this turn)
	EXPECT_EQ(hp::TotalEvents(24).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(11).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(43).value(), 1.0f);
	hp::Process();
	hp::Trigger(hp::Event::LookAtLand); // 44: only AllEvents
	EXPECT_EQ(hp::TotalEvents(43).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(48).value(), 2.0f);
}

TEST_F(TimersEventsTest, EventsBlocked)
{
	paused = true;
	hp::Trigger(hp::Event::Pitch);
	hp::Process();
	EXPECT_EQ(hp::TotalEvents(28).value(), 0.0f);
	EXPECT_EQ(hp::AccumulatedTime(), 0u);
	paused = false;
	wideScreen = true; // a script's cinematic: the hand demos' camera moves do not count
	hp::OnPlayerCameraMove(0.0f, 3.0f, 0.0f, 0);
	EXPECT_EQ(hp::TotalEvents(28).value(), 0.0f);
	wideScreen = false;
	hp::OnPlayerCameraMove(0.0f, 3.0f, 0.0f, 0);
	EXPECT_EQ(hp::TotalEvents(28).value(), 1.0f);
}

TEST_F(TimersEventsTest, CameraMoves)
{
	hp::OnPlayerCameraMove(0.005f, 0.0f, 0.0f, 0); // below the 0.01 threshold
	EXPECT_EQ(hp::TotalEvents(25).value(), 0.0f);
	hp::OnPlayerCameraMove(0.02f, 0.0f, 0.0f, 0);
	EXPECT_EQ(hp::TotalEvents(25).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(26).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(27).value(), 0.0f);
	hp::Process();
	hp::OnPlayerCameraMove(-0.02f, 0.0f, -1.0f, 0);
	EXPECT_EQ(hp::TotalEvents(25).value(), 2.0f);
	EXPECT_EQ(hp::TotalEvents(27).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(29).value(), 1.0f);
	// the 0x2nn / 0x1nn reasons only feed the camera help's own tables
	hp::Process();
	hp::CameraHelpCallback(hp::CameraReason::Move0, 0);
	hp::CameraHelpCallback(hp::CameraReason::InsideInclusionZone, 0);
	EXPECT_EQ(hp::TotalEvents(48).value(), 2.0f);
	hp::CameraHelpCallback(hp::CameraReason::DoubleClickPos, 0);
	EXPECT_EQ(hp::TotalEvents(30).value(), 1.0f);
}

TEST_F(TimersEventsTest, TimeSinceAndRate)
{
	EXPECT_EQ(hp::TimeSince(29).value(), 0.0f); // never: 0
	hp::Trigger(hp::Event::Zoom);               // at 0 ms
	hp::Process();
	hp::Trigger(hp::Event::Zoom); // at 100 ms
	hp::Process();
	hp::Process(); // now 300 ms
	EXPECT_FLOAT_EQ(hp::TimeSince(29).value(), 0.2f);
	EXPECT_FLOAT_EQ(hp::EventsPerSecond(29).value(), 1000.0f / 300.0f);
	EXPECT_EQ(hp::EventsPerSecond(28).value(), 0.0f); // fewer than 2 times
	EXPECT_FALSE(hp::EventsPerSecond(0).has_value());
}

TEST_F(TimersEventsTest, SpecialTriggers)
{
	hp::Queries queries;
	queries.paused = [] { return false; };
	queries.scriptWideScreen = [] { return false; };
	hp::CameraView view {true, 10.0f, 0.6f};
	queries.playerCamera = [&view] { return std::optional<hp::CameraView>(view); };
	hp::SetQueries(queries);
	hp::Process();
	EXPECT_EQ(hp::TotalEvents(44).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(45).value(), 1.0f);
	view = {false, 10.0f, -0.5f};
	hp::Process();
	EXPECT_EQ(hp::TotalEvents(46).value(), 1.0f);
	EXPECT_EQ(hp::TotalEvents(44).value(), 1.0f);
	hp::SetQueries({}); // the camera query captures `view`
}

namespace
{
/// W the identity: (X, Y, Z) = (x, y, z), Z the depth; the screen 640 x 480, near 1, tan(fov / 2) = 1
fov::View TestView()
{
	fov::View view;
	view.worldToClipping = {}; // affine::AffineMatrix's default: the identity
	view.nearW = 1.0f;
	view.screen = {640, 480};
	view.eye = glm::vec3(0.0f);
	view.tanHalfFov = 1.0f;
	return view;
}
} // namespace

TEST_F(TimersEventsTest, PointOnScreen)
{
	const auto view = TestView();
	EXPECT_TRUE(ros::PointOnScreen(view, {0.0f, 0.0f, 10.0f}));
	EXPECT_FALSE(ros::PointOnScreen(view, {20.0f, 0.0f, 10.0f}));
	EXPECT_FALSE(ros::PointOnScreen(view, {0.0f, 0.0f, 0.5f})); // before the near plane
	EXPECT_FALSE(ros::PointOnScreen(view, {0.0f, 0.0f, -10.0f}));
	EXPECT_TRUE(ros::PointOnScreen(view, {-10.0f, 0.0f, 10.0f}));   // the left edge, pixel 0
	EXPECT_TRUE(ros::PointOnScreen(view, {-10.01f, 0.0f, 10.0f}));  // -0.32 truncates to 0
	EXPECT_FALSE(ros::PointOnScreen(view, {-10.05f, 0.0f, 10.0f})); // -1.6 -> -1
	EXPECT_FALSE(ros::PointOnScreen(view, {10.0f, 0.0f, 10.0f}));   // the right edge is pixel 640: out
	EXPECT_TRUE(ros::PointOnScreen(view, {0.0f, 10.0f, 10.0f}));    // y up: the top row, sy = 0
	EXPECT_FALSE(ros::PointOnScreen(view, {0.0f, -10.0f, 10.0f}));  // the bottom: sy = 480, out
}

TEST_F(TimersEventsTest, SphereOnScreen)
{
	const auto view = TestView();
	// the centre 960 px right, the radius 1 x 0.1 x 640 x 0.5 = 32 px: out
	EXPECT_FALSE(ros::SphereOnScreen(view, {20.0f, 0.0f, 10.0f}, 1.0f, {20.0f, 0.0f, 10.0f}));
	// a radius of 40 (1280 px) reaches into the screen
	EXPECT_TRUE(ros::SphereOnScreen(view, {20.0f, 0.0f, 10.0f}, 40.0f, {20.0f, 0.0f, 10.0f}));
	// behind the camera
	EXPECT_FALSE(ros::SphereOnScreen(view, {0.0f, 0.0f, -50.0f}, 1.0f, {0.0f, 0.0f, -50.0f}));
	// the eye inside it (by the object's own position)
	EXPECT_TRUE(ros::SphereOnScreen(view, {0.0f, 0.0f, -5.0f}, 10.0f, {0.0f, 0.0f, 0.0f}));
}
