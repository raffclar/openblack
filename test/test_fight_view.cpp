/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <numbers>
#include <optional>
#include <tuple>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/FightView.h"

using namespace openblack;
using namespace openblack::fight_view;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
const GroundHeight k_Flat = [](glm::vec2) { return 0.0f; };
constexpr glm::vec3 k_Up {0.0f, 1.0f, 0.0f};
} // namespace

TEST(FightView, HeadingAndPitchUndoPointFrom)
{
	const glm::vec3 focus {10.0f, 2.0f, -4.0f};
	for (const float heading : {0.3f, 1.4f, -2.5f, 3.0f})
	{
		const auto point = PointFrom(focus, 30.0f, heading, 0.6f);
		const auto found = HeadingAndPitch(point, focus);
		EXPECT_NEAR(Wrapped(found.heading - heading), 0.0f, 1e-4f);
		EXPECT_NEAR(found.pitch, 0.6f, 1e-4f);
	}
	// Straight above, it has no heading and looks nearly straight down
	const auto above = HeadingAndPitch(focus + glm::vec3(0.0f, 5.0f, 0.0f), focus);
	EXPECT_EQ(above.heading, 0.0f);
	EXPECT_FLOAT_EQ(above.pitch, 1.5393804f);
}

TEST(FightView, AnglesWrapWithinHalfATurn)
{
	EXPECT_FLOAT_EQ(Wrapped(1.0f), 1.0f);
	EXPECT_NEAR(Wrapped(1.0f + (2.0f * k_Pi)), 1.0f, 1e-5f);
	EXPECT_NEAR(Wrapped(-1.0f - (4.0f * k_Pi)), -1.0f, 1e-4f);
	EXPECT_NEAR(Wrapped(3.5f), 3.5f - (2.0f * k_Pi), 1e-5f);
}

TEST(FightView, OnFlatLandTheBestHeadingIsTheOneGiven)
{
	float pitch = 0.4f;
	const auto heading = BestHeading(1.0f, 40.0f, {0.0f, 0.0f, 0.0f}, pitch, k_Flat, k_Up);
	EXPECT_FLOAT_EQ(heading, 1.0f);
	// The pitch eases: a tenth of the flat land's near upright lean, a fifth of what it was, and a little more
	EXPECT_NEAR(pitch, (1.5393804f * 0.1f) + (0.4f * 0.2f) + 0.37699114f, 1e-5f);
}

TEST(FightView, TheBestHeadingLooksOverLowLand)
{
	// Land falling away towards +z: headings looking from +z, over low land, see the fight best
	const GroundHeight slope = [](glm::vec2 point) { return -point.y; };
	float pitch = 0.4f;
	const auto heading = BestHeading(k_Pi, 40.0f, {0.0f, 0.0f, 0.0f}, pitch, slope, k_Up);
	EXPECT_NEAR(Wrapped(heading), 0.0f, (2.0f * k_Pi / 32.0f) * 3.0f + 1e-4f);
	EXPECT_GE(pitch, k_Pi / 8.0f);
	EXPECT_LE(pitch, k_Pi / 3.0f);
}

TEST(FightView, SuggestedViewsAreEasedBetweenTwentyFiveAndFifty)
{
	const glm::vec3 focus {0.0f, 0.0f, 0.0f};
	const auto near = Suggest({10.0f, 0.0f, 0.0f}, focus, k_Flat, k_Up);
	EXPECT_NEAR(glm::distance(near.origin, focus), 10.0f + (15.0f * 0.8f), 1e-3f);
	const auto far = Suggest({150.0f, 0.0f, 0.0f}, focus, k_Flat, k_Up);
	EXPECT_NEAR(glm::distance(far.origin, focus), 150.0f - (100.0f * 0.1f), 1e-2f);
	const auto start = Start({100.0f, 50.0f}, 30.0f, 5.0f, k_Flat, k_Up);
	EXPECT_EQ(start.focus, glm::vec3(100.0f, 5.0f, 50.0f));
	// From the east, the arena's radius out and half up, a little over 33 away
	EXPECT_NEAR(glm::distance(start.origin, start.focus), std::sqrt((30.0f * 30.0f) + (15.0f * 15.0f)), 1e-3f);
}

TEST(FightView, NearnessToTheArena)
{
	const glm::vec2 centre {0.0f, 0.0f};
	EXPECT_TRUE(WithinArena({5.0f, 0.0f}, {0.0f, 5.0f}, centre, 10.0f));
	EXPECT_FALSE(WithinArena({11.0f, 0.0f}, {0.0f, 5.0f}, centre, 10.0f));
	EXPECT_FALSE(TooFar({30.0f, 0.0f}, {40.0f, 0.0f}, centre, 10.0f, 1.0f));
	EXPECT_TRUE(TooFar({33.0f, 0.0f}, {43.0f, 0.0f}, centre, 10.0f, 1.0f));
	EXPECT_FALSE(TooFar({33.0f, 0.0f}, {41.0f, 0.0f}, centre, 10.0f, 1.0f));
	EXPECT_TRUE(TooFar({0.0f, 0.0f}, {61.0f, 0.0f}, centre, 10.0f, 1.0f));
	EXPECT_FALSE(TooFar({33.0f, 0.0f}, {43.0f, 0.0f}, centre, 10.0f, 2.0f));
}

TEST(FightView, ZoomSpeedsUpWithHeight)
{
	EXPECT_FLOAT_EQ(ZoomDistance(60.0f, 10.0f), 60.0f * 0.0015f * 60.0f);
	EXPECT_FLOAT_EQ(ZoomDistance(60.0f, 100.0f), 60.0f * 0.0015f * 300.0f);
	EXPECT_FLOAT_EQ(ZoomDistance(60.0f, 1000.0f), 60.0f * 0.0015f * 2000.0f);
	EXPECT_FLOAT_EQ(ZoomDistance(60.0f, -1000.0f), 60.0f * 0.0015f * 240.0f);
}

TEST(FightView, TheTrackerKeepsTheFightersSideOn)
{
	Tracker tracker;
	const Fighter first {.position = {0.0f, 0.0f, 0.0f}, .radius = 4.0f, .height = 20.0f};
	const Fighter second {.position = {20.0f, 0.0f, 0.0f}, .radius = 6.0f, .height = 30.0f};
	const auto step = tracker.Follow(first, second, {}, 0.02f);
	EXPECT_FALSE(step.zoomedOut);
	// It looks at their middle, raised by a quarter of their heights
	EXPECT_NEAR(step.view.focus.x, 10.0f, 1e-4f);
	EXPECT_NEAR(step.view.focus.y, 12.5f, 1e-4f);
	EXPECT_NEAR(step.view.focus.z, 0.0f, 1e-4f);
	// As far back as 5.5 of the first's radii, the gap and the second's radius
	EXPECT_NEAR(glm::distance(step.view.origin, step.view.focus), (4.0f * 5.5f) + 20.0f + 6.0f, 1e-3f);
	// Across the line between them: level, the camera is off to the side, not along the line
	const auto away = step.view.origin - step.view.focus;
	EXPECT_NEAR(away.x, 0.0f, 1e-3f);
	EXPECT_GT(std::abs(away.z), 1.0f);
	EXPECT_NEAR(std::asin(away.y / glm::length(away)), k_StartPitch, 1e-4f);
}

TEST(FightView, ZoomingFarOutLeavesTheFight)
{
	Tracker tracker;
	const Fighter first {.position = {0.0f, 0.0f, 0.0f}, .radius = 4.0f, .height = 20.0f};
	const Fighter second {.position = {20.0f, 0.0f, 0.0f}, .radius = 6.0f, .height = 30.0f};
	auto step = tracker.Follow(first, second, {.zoom = 4.0f * 0.3f}, 0.02f);
	EXPECT_FALSE(step.zoomedOut);
	EXPECT_NEAR(tracker.GetScale(), 5.5f + (4.0f * 0.3f * 0.3f / 4.0f), 1e-4f);
	step = tracker.Follow(first, second, {.zoom = 1000.0f}, 0.02f);
	EXPECT_TRUE(step.zoomedOut);
	EXPECT_FLOAT_EQ(tracker.GetScale(), k_MaxScale);
	// Tilting is kept within its bounds
	tracker.Follow(first, second, {.tilt = -10000.0f}, 0.02f);
	EXPECT_FLOAT_EQ(tracker.GetPitch(), k_MaxPitch);
	tracker.Follow(first, second, {.turn = 640.0f, .screenWidth = 1280.0f}, 0.02f);
	EXPECT_NEAR(tracker.GetHeadingOffset(), k_StartHeadingOffset + (k_Pi / 2.0f), 1e-5f);
}

TEST(FightView, TheCameraBlendFallsFromFiveSecondsToTwo)
{
	EXPECT_FLOAT_EQ(BlendSeconds(0.0f), 5.0f);
	EXPECT_FLOAT_EQ(BlendSeconds(1.0f), 3.5f);
	EXPECT_FLOAT_EQ(BlendSeconds(2.0f), 2.0f);
	// After the first 2 seconds it stays at 2 seconds
	EXPECT_FLOAT_EQ(BlendSeconds(2.001f), 2.0f);
	EXPECT_FLOAT_EQ(BlendSeconds(600.0f), 2.0f);
	// It falls evenly
	EXPECT_NEAR(BlendSeconds(0.5f) - BlendSeconds(1.5f), 1.5f, 1e-5f);
}

TEST(FightView, TheWayPointIsHalfWayRaisedAndAboveTheLand)
{
	const glm::vec3 from {0.0f, 100.0f, 0.0f};
	const glm::vec3 to {300.0f, 20.0f, 400.0f};
	// Raised by a share of the distance across the land
	const auto raised = WayPoint(from, to, k_ReturnWayRise, k_Flat);
	EXPECT_FLOAT_EQ(raised.x, 150.0f);
	EXPECT_FLOAT_EQ(raised.z, 200.0f);
	EXPECT_FLOAT_EQ(raised.y, 60.0f + 50.0f);
	EXPECT_FLOAT_EQ(WayPoint(from, to, k_StartWayRise, k_Flat).y, 60.0f);
	// Kept 10 above high land
	const GroundHeight high = [](glm::vec2) { return 80.0f; };
	EXPECT_FLOAT_EQ(WayPoint(from, to, k_StartWayRise, high).y, 90.0f);
}

TEST(FightView, TheChaseFliesThroughTheWayPointThenFollowsOnTheBlend)
{
	constexpr float k_Step = 0.01f;
	// A step of an eighth of a second adds up with no rounding
	constexpr float k_FlightStep = 0.125f;
	const View from {.origin = {0.0f, 50.0f, 0.0f}, .focus = {0.0f, 0.0f, 100.0f}};
	const View still {.origin = glm::vec3(0.0f), .focus = glm::vec3(0.0f)};
	const View to {.origin = {200.0f, 50.0f, 0.0f}, .focus = {200.0f, 0.0f, 100.0f}};
	const glm::vec3 way {100.0f, 80.0f, 0.0f};
	Chase chase;
	chase.Fly(from, still, to, way);
	EXPECT_TRUE(chase.Flying());
	// A wanted view does not move it during the flight: it goes to the way point on a curve of 1.35 seconds
	const View elsewhere {.origin = {-500.0f, 0.0f, 0.0f}, .focus = {-500.0f, 0.0f, 0.0f}};
	glm::vec3 highest = from.origin;
	int steps = 0;
	while (chase.Flying())
	{
		const auto shown = chase.Step(elsewhere, 10.0f, k_FlightStep);
		highest.y = std::max(highest.y, shown.origin.y);
		++steps;
	}
	// It turns to the view itself on the first step after more than 0.75 seconds on the way: 0.875 seconds
	EXPECT_EQ(steps, 8);
	EXPECT_GT(highest.y, 60.0f);
	// Then it follows the wanted view on curves of 2 seconds, sent each step
	auto shown = chase.GetValue();
	for (int i = 0; i < 1000; ++i)
	{
		shown = chase.Step(to, 10.0f, k_Step);
	}
	EXPECT_NEAR(shown.origin.x, to.origin.x, 1e-2f);
	EXPECT_NEAR(shown.focus.z, to.focus.z, 1e-2f);
}

TEST(FightView, AFreshChaseEasesSlowerThanALaterOne)
{
	constexpr float k_Step = 0.01f;
	const View still {.origin = glm::vec3(0.0f), .focus = glm::vec3(0.0f)};
	const View start {.origin = {0.0f, 0.0f, 0.0f}, .focus = {0.0f, 0.0f, 1.0f}};
	const View wanted {.origin = {100.0f, 0.0f, 0.0f}, .focus = {100.0f, 0.0f, 1.0f}};
	// Each chase first ends its flight at the start, then follows the wanted view
	const auto chaseFrom = [&](float modelSeconds) {
		Chase chase;
		chase.Fly(start, still, start, start.origin);
		while (chase.Flying())
		{
			chase.Step(std::nullopt, modelSeconds, k_Step);
		}
		for (int i = 0; i < 50; ++i)
		{
			chase.Step(wanted, modelSeconds, k_Step);
		}
		return chase.GetValue().origin.x;
	};
	// On a 5 second curve it has moved less after half a second than on a 2 second one
	EXPECT_LT(chaseFrom(0.0f), chaseFrom(10.0f));
	EXPECT_GT(chaseFrom(0.0f), 0.0f);
}

TEST(FightView, AZoomerCarriesTheSpeedItTakesOver)
{
	Zoomer zoomer;
	zoomer.Carry(10.0f, 4.0f);
	EXPECT_FLOAT_EQ(zoomer.GetValue(), 10.0f);
	EXPECT_FLOAT_EQ(zoomer.GetSpeed(), 4.0f);
	// Sent to where it is, it first carries on in the way it moved
	zoomer.SetDestination(10.0f, 2.0f);
	zoomer.Update(0.1f);
	EXPECT_GT(zoomer.GetValue(), 10.0f);
	EXPECT_FLOAT_EQ(zoomer.GetElapsed(), 0.1f);
}

TEST(FightView, AFlightEndingOutsideTheFenceStopsAtItAndWooshesWhenFar)
{
	const auto flat = [](glm::vec2) { return 0.0f; };
	// The fence keeps the camera to x <= 100, along its line of sight
	const auto keepInside = [](const glm::vec3& origin, const glm::vec3& focus) {
		if (origin.x <= 100.0f)
		{
			return origin;
		}
		const auto along = (100.0f - origin.x) / (focus.x - origin.x);
		return origin + (focus - origin) * along;
	};
	const glm::vec3 from {0.0f, 50.0f, 0.0f};
	const View to {.origin = {200.0f, 50.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}};
	const auto kept = PlanFlight(from, to, k_StartWayRise, flat, keepInside);
	EXPECT_NEAR(kept.to.origin.x, 100.0f, 1e-4f);
	EXPECT_NEAR(kept.to.origin.y, 25.0f, 1e-4f);
	EXPECT_EQ(kept.to.focus, to.focus);
	// The way point is half way to where the flight now ends
	EXPECT_NEAR(kept.way.x, 50.0f, 1e-4f);
	// It starts less than 150 from there: no woosh
	EXPECT_FALSE(kept.woosh);
	const View far {.origin = {-60.0f, 50.0f, 120.0f}, .focus = {0.0f, 0.0f, 0.0f}};
	EXPECT_TRUE(PlanFlight({100.0f, 50.0f, 0.0f}, far, k_ReturnWayRise, flat, keepInside).woosh);
	// Exactly 150 away is not far enough
	const View edge {.origin = {0.0f, 50.0f, 150.0f}, .focus = {0.0f, 0.0f, 0.0f}};
	EXPECT_FALSE(PlanFlight(from, edge, k_StartWayRise, flat, keepInside).woosh);
}

TEST(FightView, AControlStopsTheFlightAndTheViewFollowsAtOnce)
{
	const View from {.origin = {0.0f, 50.0f, 0.0f}, .focus = {0.0f, 0.0f, 100.0f}};
	const View still {.origin = glm::vec3(0.0f), .focus = glm::vec3(0.0f)};
	const View to {.origin = {200.0f, 50.0f, 0.0f}, .focus = {200.0f, 0.0f, 100.0f}};
	Chase chase;
	chase.Fly(from, still, to, {100.0f, 80.0f, 0.0f});
	std::ignore = chase.Step(to, 10.0f, 0.1f);
	chase.StopFlight();
	EXPECT_FALSE(chase.Flying());
	// The wanted view now moves it, never through the way point's height
	float highest = 0.0f;
	for (int i = 0; i < 100; ++i)
	{
		highest = std::max(highest, chase.Step(to, 10.0f, 0.1f).origin.y);
	}
	EXPECT_LT(highest, 60.0f);
	EXPECT_NEAR(chase.GetValue().origin.x, to.origin.x, 1e-2f);
}

TEST(FightView, AResumedChaseGoesOnFromWhereTheCameraIs)
{
	Chase chase;
	const View at {.origin = {10.0f, 40.0f, 0.0f}, .focus = {10.0f, 0.0f, 50.0f}};
	const View speed {.origin = {5.0f, 0.0f, 0.0f}, .focus = {5.0f, 0.0f, 0.0f}};
	chase.Resume(at, speed);
	EXPECT_FALSE(chase.Flying());
	EXPECT_EQ(chase.GetValue().origin, at.origin);
	EXPECT_EQ(chase.GetHeld().origin, at.origin);
	EXPECT_NEAR(chase.GetSpeed().origin.x, 5.0f, 1e-4f);
}

TEST(FightView, ADragTurnsTheViewAndTiltsItTheOtherWay)
{
	Tracker tracker;
	tracker.Dragged(0.5f, 0.1f);
	EXPECT_FLOAT_EQ(tracker.GetHeadingOffset(), k_StartHeadingOffset + 0.5f);
	EXPECT_FLOAT_EQ(tracker.GetPitch(), k_StartPitch + 0.1f);
}

TEST(FightView, DraggingFarFromTheArenaLeavesTheFightAfterTheFlight)
{
	const glm::vec2 centre {0.0f, 0.0f};
	constexpr float k_Radius = 10.0f;
	// The eye more than 3.2 x 0.75 radii away and where it looks more than 4.2 x 0.75 radii away
	const glm::vec2 eye {40.0f, 0.0f};
	const glm::vec2 looking {35.0f, 0.0f};
	EXPECT_TRUE(DraggedAway(4.6f, true, eye, looking, centre, k_Radius));
	EXPECT_FALSE(DraggedAway(4.5f, true, eye, looking, centre, k_Radius));
	EXPECT_FALSE(DraggedAway(10.0f, false, eye, looking, centre, k_Radius));
	EXPECT_FALSE(DraggedAway(10.0f, true, {20.0f, 0.0f}, {10.0f, 0.0f}, centre, k_Radius));
	// Where it looks alone further than 6 x 0.75 radii is enough
	EXPECT_TRUE(DraggedAway(10.0f, true, {0.0f, 0.0f}, {46.0f, 0.0f}, centre, k_Radius));
}
