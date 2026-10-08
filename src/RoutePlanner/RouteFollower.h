/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/Point2D.h"
#include "RoutePlanner/RoutePlan.h"

// RouteFollower: the route planner's follower, the creature's (Init from its constructors, Update and MoveAlongRoute from
// its update). It is its own ObstacleGrid (its obstacles), plans with up to 5 RoutePlans (the main one and the re-plans of
// parts of the route ahead), follows the found route node by node and re-plans when a new obstacle cuts it
// (ObstacleGrid::AddObject -> OnObjectAdded).
// (openblack) the original moves the plans' RouteNodes into its route by pointer and keeps the adopted plan only as
// their owner; here the follower owns copies of them in its own pool, linked exactly as the original links them.
// The creature's locomotion (ECS/Systems/Implementations/CreatureLocomotionSystem) holds one per creature; it is not
// called from the game yet.

namespace openblack::route_planner
{
class RouteFollower: public ObstacleGrid
{
public:
	enum class State : int32_t
	{
		None = 0,       ///< the constructor's
		Idle = 1,       ///< no plan (Init's state after the constructor is still 0; the end of a route gives 1)
		Planning = 2,   ///< the main plan searching
		Following = 3,  ///< a route, no re-plan
		Replanning = 4, ///< a route and re-plans of parts of it
		GivenUp = 6,    ///< Update: Failed with the hook and the dest farther than the plan's worst rejected cost
	};

	/// The plan's end. code: 2 = re-planning after a found / dest-inside end, 1 = after a failed one, 0 = no pending dest
	/// and found, 3 = no pending dest and failed (OnFailed)
	using DoneFn = std::function<void(int32_t context, int32_t code)>;
	/// The first step of a route, (context, 0, the first node's length >= 0.01)
	using StepFn = std::function<void(int32_t context, float zero, float length)>;
	/// The follower's radius ahead (the creature's nav radius) for the shortcut search
	using RadiusFn = std::function<float(int32_t context)>;

	static constexpr int32_t k_MaxPlans = 5;

	/// ObstacleGrid, the fields below, and the holder's plan = this
	RouteFollower();
	~RouteFollower();
	RouteFollower(const RouteFollower&) = delete;
	RouteFollower& operator=(const RouteFollower&) = delete;

	/// The callbacks' context, the callbacks and the search turns per update
	void Init(int32_t context, DoneFn done, StepFn step, RadiusFn radius, int32_t turnsPerUpdate);
	/// a and b are stored, r is the radius (the creature passes its nav radius) and c the plans' max length. By the
	/// state: a new main plan (1), a re-plan when the new dest moved by more than half the way left (2 / 4), or re-plans
	/// of the parts of the route ahead (3)
	void SetDest(const Point2D& dest, float a, float b, float r, float c);
	/// Up to `turns` (or Init's) search turns of the best plan; the hook installed for them only
	void Update(ObstacleHook hook, int32_t turns);
	/// One step of 0.1 x speed along the route
	void MoveAlongRoute();
	/// A new obstacle (ObstacleGrid::AddObject with notify 1); re-plans when it cuts the route
	void OnObjectAdded(const RouteObstacle& entry);

	[[nodiscard]] State GetState() const { return _state; }
	/// The creature sets the state to Idle before its first SetDest
	void SetIdle() { _state = State::Idle; }
	/// The follower's position (PositionAt writes it while following)
	[[nodiscard]] const Point2D& GetPosition() const { return _position; }
	void SetPosition(const Point2D& p) { _position = p; }
	/// The heading PositionAt gives
	[[nodiscard]] float GetHeading() const { return _heading; }
	/// The next plan's start (SetDest case 1), the route's end while following
	[[nodiscard]] const Point2D& GetPlanStart() const { return _planStart; }
	void SetPlanStart(const Point2D& p) { _planStart = p; }
	/// The speed MoveAlongRoute multiplies by 0.1 ((pending) the creature's writer)
	void SetSpeed(float speed) { _speed = speed; }
	[[nodiscard]] int32_t GetPlanCount() const { return _planCount; }
	[[nodiscard]] bool HasRoute() const { return _routeHead != k_None; }
	/// The route from the current node on (for the tests and the debug draw)
	[[nodiscard]] std::vector<RouteNode> GetRouteAhead() const;
	/// The callbacks' context Init got (the creature's entity)
	[[nodiscard]] int32_t GetContext() const { return _context; }
	/// What is left of the route: the current node's length past where the follower is, then every node after it; 0
	/// without a route
	[[nodiscard]] float RemainingLength() const;

private:
	/// One node of the follower's own route
	NodeIndex NewNode(const RouteNode& copy);
	void FreeNode(NodeIndex node);
	RouteNode& Node(NodeIndex node) { return _nodes.at(static_cast<size_t>(node)); }
	[[nodiscard]] const RouteNode& Node(NodeIndex node) const { return _nodes.at(static_cast<size_t>(node)); }
	/// The plan's best route copied into the pool, linked; its first and last nodes
	std::pair<NodeIndex, NodeIndex> CopyBestRoute(const RoutePlan& plan);

	/// A node's length (arc 1: ArcLength, else DistanceTo)
	[[nodiscard]] float Length(NodeIndex node) const;
	/// The arc on the previous node's circle and side
	[[nodiscard]] float ArcLength(NodeIndex node) const;
	/// cost = the previous node's cost (0 without) + the node's own length (type 0 straight, else arc)
	void Cumulate(NodeIndex node);
	/// Every node after `node` deleted, the route's tail = node
	void CutAfter(NodeIndex node);
	/// The route from `from` meets the new obstacle; it is cut there
	bool RouteMeets(NodeIndex from, const RouteObstacle& entry);
	/// RouteMeets' test on a plan's own route (the plans' routes in OnObjectAdded), without the cut
	bool PlanRouteMeets(const RoutePlan& plan, NodeIndex from, const RouteObstacle& entry);
	/// The follower at distance t along the current node
	void PositionAt(float t);

	/// Every plan deleted
	void FreePlans();
	/// The search ended without a route
	void OnFailed();
	/// The search found its route
	void OnFound();
	/// The finished re-plan spliced in at `at`, then the shortcut search ahead
	void SpliceAndShortcut(NodeIndex at);

	Point2D _position;
	float _speed {0.0f};
	float _travelled {0.0f}; ///< Along the current node
	float _x6403C {5.0f};    ///< (pending) its reader
	float _heading {0.0f};
	Point2D _planStart;
	float _destRadius {1.0f}; ///< SetDest's r
	float _lastStep {0.0f};
	State _state {State::None};
	int32_t _destPending {0};
	int32_t _turnsPerUpdate {1};
	int32_t _context {0};
	DoneFn _done;
	StepFn _step;
	RadiusFn _radius;
	NodeIndex _current {k_None};
	Point2D _dest;
	float _destA {0.0f};
	float _destB {0.0f};
	float _destC {0.0f};
	int32_t _foundOnce {0}; ///< 1 at a found end; 0 at a new plan
	std::array<std::unique_ptr<RoutePlan>, k_MaxPlans> _plans {};
	std::array<NodeIndex, k_MaxPlans> _planAt {}; ///< The route node each re-plan starts from
	int32_t _planCount {0};
	int32_t _bestPlan {0}; ///< -1 after case 1

	// The adopted main plan: (openblack) its route's head / tail and the fields the original still reads
	bool _hasMainPlan {false};
	NodeIndex _routeHead {k_None};
	NodeIndex _routeTail {k_None};
	Point2D _mainDest;
	float _mainDestRadius {0.0f};
	float _mainNear {0.0f};

	std::vector<RouteNode> _nodes;
	std::vector<NodeIndex> _freeNodes;
};
} // namespace openblack::route_planner
