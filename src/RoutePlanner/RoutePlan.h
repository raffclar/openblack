/*******************************************************************************
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
#include <vector>

#include "Point2D.h"

// The LH route planner. A best-first search over routes that go straight between the obstacle circles of an
// ObstacleGrid and round them along their arcs. The original allocates its Routes and RouteNodes on the heap; here they
// live in per-plan pools named by index, linked exactly as the original links them (the walks' order is the
// original's; the indices never matter).

namespace openblack::route_planner
{
class ObstacleGrid;

using NodeIndex = int32_t;
using RouteIndex = int32_t;
inline constexpr int32_t k_None = -1;

/// One stretch of a route: straight from -> to, or round an object
struct RouteNode
{
	Point2D from;
	Point2D to;
	int32_t obj {-1}; ///< An ObstacleGrid object index
	int32_t side {0}; ///< The way round obj: 1 or 2 (0 none)
	int32_t startsOnObject {0};
	int32_t arc {0};   ///< 1 when the node runs round its parent's object
	float cost {0.0f}; ///< The length so far
	NodeIndex child {k_None};
	NodeIndex parent {k_None};
};

/// A route: its chain of nodes and its place in the plan's list
struct Route
{
	NodeIndex first {k_None};
	NodeIndex last {k_None};  ///< The route's tip
	RouteIndex next {k_None}; ///< The plan's list
	int32_t shortcutDone {1}; ///< Shortcut skips the route while set
};

class RoutePlan
{
public:
	enum class State : int32_t
	{
		None = 0,
		Set = 1,
		Searching = 2,
		DestInside = 3,
		Failed = 4,
		Found = 5,
	};

	RoutePlan() = default;
	RoutePlan(const RoutePlan&) = delete;
	RoutePlan& operator=(const RoutePlan&) = delete;
	/// Frees the routes
	~RoutePlan() { FreeRoutes(); }

	void SetStart(const Point2D& start, float r, ObstacleGrid& holder, int32_t objA, int32_t objB, int32_t sideB);
	/// Starts the search towards the dest
	void SetDest(const Point2D& dest, float r, float nearDistance, float startCost, int32_t destObj, int32_t destSide,
	             float maxLength, bool linkHolder);
	/// One step of the search
	void StepSearch(int32_t depth);
	void FreeRoutes();

	[[nodiscard]] State GetState() const { return _state; }
	/// The best route (k_None when none)
	[[nodiscard]] RouteIndex GetBestRoute() const { return _best; }
	[[nodiscard]] const Route& GetRoute(RouteIndex route) const { return _routes.at(static_cast<size_t>(route)); }
	[[nodiscard]] const RouteNode& GetNode(NodeIndex node) const { return _nodes.at(static_cast<size_t>(node)); }
	[[nodiscard]] const Point2D& GetStart() const { return _start; }
	[[nodiscard]] const Point2D& GetDest() const { return _dest; }
	/// Read by RouteFollower
	[[nodiscard]] float GetDestRadius() const { return _destRadius; }
	[[nodiscard]] float GetNearDistance() const { return _near; }
	[[nodiscard]] float GetEstimate() const { return _estimate; }
	[[nodiscard]] float GetWorstRejected() const { return _worstRejected; }
	[[nodiscard]] RouteIndex GetRouteList() const { return _routeList; }

private:
	struct Visited
	{
		float x;
		float z;
		float cost;
	};
	/// A block of 256 visited points and its count
	struct VisitBlock
	{
		int32_t count {0};
		std::array<Visited, 256> entries {};
	};

	NodeIndex NewNode(NodeIndex parent, int32_t arc, const Point2D& from, const Point2D& to, int32_t obj, int32_t side);
	/// The node copied, unlinked and with startsOnObject 0
	NodeIndex CopyNode(NodeIndex source);
	void FreeNode(NodeIndex node);
	RouteIndex NewRoute();
	/// The nodes copied in order
	RouteIndex CopyRoute(RouteIndex source);
	/// The route's nodes and the route freed
	void DeleteRoute(RouteIndex route);
	RouteNode& Node(NodeIndex node) { return _nodes.at(static_cast<size_t>(node)); }
	Route& RouteAt(RouteIndex route) { return _routes.at(static_cast<size_t>(route)); }

	[[nodiscard]] float GetArcLength(NodeIndex node) const;
	/// cost = parent's + range (arc 0) or arc length
	void SetCost(NodeIndex node);
	/// The route out of the list; none left -> Failed
	void DropRoute(RouteIndex route);
	/// The best route's tip within `near` of the dest -> Finish
	void CheckArrived();
	/// Every route but the best freed, Found
	void Finish();
	/// true rejects (too long, or the point seen already); else added
	bool Visit(const Point2D& p, float cost, float limit);
	void FreeVisited();
	/// The route's last stretch planned again from its tip back, spliced in when found
	void Shortcut(RouteIndex route, int32_t depth);

	ObstacleGrid* _holder {nullptr};
	float _radius {0.0f};
	int32_t _objA {-1};
	int32_t _objB {-1};
	int32_t _sideB {0};
	int32_t _destObj {-1};
	int32_t _destSide {0};
	Point2D _destFrom; ///< Written by a parent plan's Shortcut only
	Point2D _destTo;   ///< Likewise (SetDest / StepSearch test against it with a destObj)
	Point2D _start;
	Point2D _dest; ///< SetStart writes the start here too
	float _destRadius {0.0f};
	float _near {0.0f};
	float _maxLength {0.0f};
	float _startCost {0.0f};
	float _estimate {0.0f};
	State _state {State::None};
	int32_t _visitedCount {0};
	std::vector<VisitBlock> _visited; ///< The newest block at the back
	float _worstRejected {0.0f};      ///< Only with the obstacle hook
	RouteIndex _routeList {k_None};
	int32_t _routeCount {0};
	RouteIndex _best {k_None};

	std::vector<RouteNode> _nodes;
	std::vector<NodeIndex> _freeNodes;
	std::vector<Route> _routes;
	std::vector<RouteIndex> _freeRoutes;
};
} // namespace openblack::route_planner
