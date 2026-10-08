/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// RouteFollower (the route planner's follower) on hand-made obstacles (no
// world, as test_rplan): a straight route followed to its end, a route round one circle, a new obstacle on the route
// that makes it re-plan, and the obstacle hook's lifetime (installed by Update only).

#include <cmath>

#include <algorithm>
#include <memory>

#include <gtest/gtest.h>

#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RouteFollower.h"

using namespace openblack::route_planner;

namespace
{
class RouteFollowerTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		ObstacleGrid::InstallCallbacks(nullptr, nullptr);
		_follow = std::make_unique<RouteFollower>(); // a large object: on the heap
		_follow->Init(
		    7, [this](int32_t, int32_t code) { _doneCodes.push_back(code); },
		    [this](int32_t, float, float length) { _stepLengths.push_back(length); }, nullptr, 64);
		_follow->SetIdle(); // as the creature sets it up
	}
	void TearDown() override { ObstacleGrid::SetObstacleHook(nullptr, nullptr); }

	/// SetDest to `dest` from `start` and the search run until the route is taken
	void PlanTo(const Point2D& start, const Point2D& dest)
	{
		_follow->SetPosition(start);
		_follow->SetPlanStart(start);
		const float maxLength = (start.DistanceTo(dest) + 1.0f) * 5.0f;
		// SetDest(dest, a, b, r, c): the radius 0.5, the max length c
		_follow->SetDest(dest, 0.0f, 1.0f, 0.5f, maxLength);
		for (int i = 0; i < 16 && _follow->GetState() == RouteFollower::State::Planning; ++i)
		{
			_follow->Update(nullptr, 0);
		}
	}

	std::unique_ptr<RouteFollower> _follow;
	std::vector<int32_t> _doneCodes;
	std::vector<float> _stepLengths;
};

TEST_F(RouteFollowerTest, FollowsAStraightRouteToItsEnd)
{
	PlanTo({100.0f, 100.0f}, {300.0f, 100.0f});
	ASSERT_EQ(_follow->GetState(), RouteFollower::State::Following);
	// the first step's length to the callback, the follower on the route's start
	ASSERT_EQ(_stepLengths.size(), 1u);
	EXPECT_GT(_stepLengths.front(), 0.01f);
	EXPECT_TRUE(_doneCodes.empty());
	_follow->SetSpeed(10.0f); // 0.1 x 10 = 1 m a step
	float lastX = _follow->GetPosition().x;
	for (int i = 0; i < 400 && _follow->GetState() == RouteFollower::State::Following; ++i)
	{
		_follow->MoveAlongRoute();
		if (_follow->GetState() == RouteFollower::State::Following)
		{
			EXPECT_GE(_follow->GetPosition().x, lastX - 0.001f);
			lastX = _follow->GetPosition().x;
		}
	}
	// the route's end, the follower at its end, the state 1 (idle) again
	EXPECT_EQ(_follow->GetState(), RouteFollower::State::Idle);
	EXPECT_FALSE(_follow->HasRoute());
	EXPECT_NEAR(_follow->GetPosition().x, 300.0f, 1.5f);
	EXPECT_NEAR(_follow->GetPosition().z, 100.0f, 1.5f);
}

TEST_F(RouteFollowerTest, GoesRoundACircleWithoutEnteringIt)
{
	const Point2D c {200.0f, 100.0f};
	_follow->AddObject(5, c, 10.0f, 0);
	PlanTo({100.0f, 100.0f}, {300.0f, 100.0f});
	ASSERT_EQ(_follow->GetState(), RouteFollower::State::Following);
	_follow->SetSpeed(5.0f);
	float nearest = 1e9f;
	for (int i = 0; i < 1000 && _follow->GetState() == RouteFollower::State::Following; ++i)
	{
		_follow->MoveAlongRoute();
		nearest = std::min(nearest, _follow->GetPosition().DistanceTo(c));
	}
	EXPECT_EQ(_follow->GetState(), RouteFollower::State::Idle);
	// the arcs run on the circle (c + r (cos, sin)), the straight legs outside it
	EXPECT_GE(nearest, 10.0f - 0.05f);
	EXPECT_LE(nearest, 10.0f + 1.0f);
}

TEST_F(RouteFollowerTest, ANewObstacleOnTheRouteMakesItReplan)
{
	PlanTo({100.0f, 100.0f}, {300.0f, 100.0f});
	ASSERT_EQ(_follow->GetState(), RouteFollower::State::Following);
	// ObstacleGrid::AddObject with notify 1: the route meets it (cut there) -> SetDest case 3
	_follow->AddObject(9, {220.0f, 100.0f}, 10.0f, 1);
	EXPECT_EQ(_follow->GetState(), RouteFollower::State::Replanning);
	EXPECT_GT(_follow->GetPlanCount(), 0);
}

TEST_F(RouteFollowerTest, AnObstacleAwayFromTheRouteChangesNothing)
{
	PlanTo({100.0f, 100.0f}, {300.0f, 100.0f});
	ASSERT_EQ(_follow->GetState(), RouteFollower::State::Following);
	_follow->AddObject(9, {200.0f, 400.0f}, 10.0f, 1);
	EXPECT_EQ(_follow->GetState(), RouteFollower::State::Following);
	EXPECT_EQ(_follow->GetPlanCount(), 0);
}

bool g_HookSeenInstalled = false;
void NoteObstacle(void*, int32_t)
{
	g_HookSeenInstalled = ObstacleGrid::HasObstacleHook();
}

TEST_F(RouteFollowerTest, TheHookLivesForOneUpdateOnly)
{
	_follow->SetPosition({100.0f, 100.0f});
	_follow->SetPlanStart({100.0f, 100.0f});
	_follow->SetDest({300.0f, 100.0f}, 0.0f, 1.0f, 0.5f, 1005.0f);
	ASSERT_EQ(_follow->GetState(), RouteFollower::State::Planning);
	_follow->Update(&NoteObstacle, 1);
	// the obstacle hook cleared at the update's end
	EXPECT_FALSE(ObstacleGrid::HasObstacleHook());
}

TEST_F(RouteFollowerTest, TheHookIsInstalledDuringTheUpdate)
{
	// an obstacle with an id on the way: the search tells the hook about it (the turn's update -> NotifyObstacle)
	g_HookSeenInstalled = false;
	_follow->AddObject(5, {200.0f, 100.0f}, 10.0f, 0);
	_follow->SetPosition({100.0f, 100.0f});
	_follow->SetPlanStart({100.0f, 100.0f});
	_follow->SetDest({300.0f, 100.0f}, 0.0f, 1.0f, 0.5f, 1005.0f);
	_follow->Update(&NoteObstacle, 64);
	EXPECT_TRUE(g_HookSeenInstalled);
	EXPECT_FALSE(ObstacleGrid::HasObstacleHook());
}

// (pending) disabled: its premise was wrong. SetDest finds a clear way at once, with no length test, and with an
// obstacle the search still finds a way round it; a failing plan needs a fixture whose search runs out of routes
TEST_F(RouteFollowerTest, DISABLED_AFailedPlanEndsWithCodeThree)
{
	// a failed plan with no pending dest: done(3), state 1
	_follow->SetPosition({100.0f, 100.0f});
	_follow->SetPlanStart({100.0f, 100.0f});
	_follow->SetDest({300.0f, 100.0f}, 0.0f, 1.0f, 0.5f, 10.0f);
	for (int i = 0; i < 16 && _follow->GetState() == RouteFollower::State::Planning; ++i)
	{
		_follow->Update(nullptr, 0);
	}
	EXPECT_EQ(_follow->GetState(), RouteFollower::State::Idle);
	ASSERT_EQ(_doneCodes.size(), 1u);
	EXPECT_EQ(_doneCodes.front(), 3);
	EXPECT_EQ(_follow->GetPlanCount(), 0);
}

// (pending) disabled for the same reason as the one above
TEST_F(RouteFollowerTest, DISABLED_AFailedPlanWithTheHookGivesUp)
{
	// failed, with a hook, the dest farther than the worst rejected cost -> state 6
	_follow->SetPosition({100.0f, 100.0f});
	_follow->SetPlanStart({100.0f, 100.0f});
	_follow->SetDest({300.0f, 100.0f}, 0.0f, 1.0f, 0.5f, 10.0f);
	for (int i = 0; i < 16 && _follow->GetState() == RouteFollower::State::Planning; ++i)
	{
		_follow->Update(&NoteObstacle, 64);
	}
	EXPECT_EQ(_follow->GetState(), RouteFollower::State::GivenUp);
	EXPECT_TRUE(_doneCodes.empty());
	EXPECT_EQ(_follow->GetPlanCount(), 0);
}

TEST_F(RouteFollowerTest, AReplanIsSplicedInAndFollowedRoundTheNewObstacle)
{
	PlanTo({100.0f, 100.0f}, {300.0f, 100.0f});
	ASSERT_EQ(_follow->GetState(), RouteFollower::State::Following);
	const Point2D c {220.0f, 100.0f};
	_follow->AddObject(9, c, 10.0f, 1);
	ASSERT_EQ(_follow->GetState(), RouteFollower::State::Replanning);
	// the re-plan (state 4) spliced in, the state 3 again
	for (int i = 0; i < 64 && _follow->GetState() == RouteFollower::State::Replanning; ++i)
	{
		_follow->Update(nullptr, 0);
	}
	ASSERT_EQ(_follow->GetState(), RouteFollower::State::Following);
	EXPECT_EQ(_follow->GetPlanCount(), 0);
	_follow->SetSpeed(5.0f);
	float nearest = 1e9f;
	for (int i = 0; i < 1000 && _follow->GetState() == RouteFollower::State::Following; ++i)
	{
		_follow->MoveAlongRoute();
		nearest = std::min(nearest, _follow->GetPosition().DistanceTo(c));
	}
	EXPECT_EQ(_follow->GetState(), RouteFollower::State::Idle);
	EXPECT_GE(nearest, 10.0f - 0.05f);
}

TEST(Point2DHeading, IsAtan2OfXAndMinusZ)
{
	// GetHeading: atan2(x, -z); 0 for a (near) null vector
	EXPECT_FLOAT_EQ((Point2D {0.0f, -1.0f}.GetHeading()), 0.0f);
	EXPECT_NEAR((Point2D {1.0f, 0.0f}.GetHeading()), 1.5707963f, 1e-6f);
	EXPECT_NEAR((Point2D {0.0f, 1.0f}.GetHeading()), 3.1415927f, 1e-6f);
	EXPECT_NEAR((Point2D {-1.0f, 0.0f}.GetHeading()), -1.5707963f, 1e-6f);
	EXPECT_FLOAT_EQ((Point2D {0.0005f, 0.0005f}.GetHeading()), 0.0f);
	EXPECT_NEAR((Point2D {1.0f, -1.0f}.GetHeading()), 0.7853982f, 1e-6f);
}
} // namespace
