/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RoutePlan.h"

#include <cmath>

#include "ObstacleGrid.h"

using namespace openblack::route_planner;

namespace
{
constexpr float k_Epsilon = 0.001f;
constexpr float k_Pi = 3.14159265358979f; ///< Stored as a float
constexpr int32_t k_MaxShortcutDepth = 2;
constexpr int32_t k_ShortcutSteps = 16; ///< Search steps per shortcut try
constexpr int32_t k_ShortcutTries = 4;  ///< Earlier ends tried per shortcut

int32_t OtherSide(int32_t side)
{
	return side == 1 ? 2 : 1;
}
} // namespace

// ---- pools ----------------------------------------------------------------------------------------------------------

NodeIndex RoutePlan::NewNode(NodeIndex parent, int32_t arc, const Point2D& from, const Point2D& to, int32_t obj, int32_t side)
{
	RouteNode node {
	    .from = from,
	    .to = to,
	    .obj = obj,
	    .side = side,
	    .startsOnObject = 0,
	    .arc = arc,
	    .child = k_None,
	    .parent = parent,
	};
	if (!_freeNodes.empty())
	{
		const auto index = _freeNodes.back();
		_freeNodes.pop_back();
		_nodes.at(static_cast<size_t>(index)) = node;
		return index;
	}
	_nodes.push_back(node);
	return static_cast<NodeIndex>(_nodes.size() - 1);
}

NodeIndex RoutePlan::CopyNode(NodeIndex source)
{
	const RouteNode& s = Node(source);
	const auto copy = NewNode(k_None, s.arc, s.from, s.to, s.obj, s.side);
	Node(copy).cost = Node(source).cost;
	return copy;
}

void RoutePlan::FreeNode(NodeIndex node)
{
	_freeNodes.push_back(node);
}

RouteIndex RoutePlan::NewRoute()
{
	const Route route; // no nodes, shortcutDone set
	if (!_freeRoutes.empty())
	{
		const auto index = _freeRoutes.back();
		_freeRoutes.pop_back();
		_routes.at(static_cast<size_t>(index)) = route;
		return index;
	}
	_routes.push_back(route);
	return static_cast<RouteIndex>(_routes.size() - 1);
}

RouteIndex RoutePlan::CopyRoute(RouteIndex source)
{
	const auto copy = NewRoute();
	// the first node, then every next one, chained both ways
	const auto first = CopyNode(RouteAt(source).first);
	RouteAt(copy).first = first;
	RouteAt(copy).last = first;
	for (NodeIndex at = Node(RouteAt(source).first).child; at != k_None; at = Node(at).child)
	{
		const auto node = CopyNode(at);
		const auto last = RouteAt(copy).last;
		Node(last).child = node;
		Node(node).parent = last;
		RouteAt(copy).last = node;
	}
	RouteAt(copy).next = k_None;
	RouteAt(copy).shortcutDone = RouteAt(source).shortcutDone;
	return copy;
}

void RoutePlan::DeleteRoute(RouteIndex route)
{
	for (NodeIndex at = RouteAt(route).first; at != k_None;)
	{
		const auto next = Node(at).child;
		FreeNode(at);
		at = next;
	}
	_freeRoutes.push_back(route);
}

// ---- costs ----------------------------------------------------------------------------------------------------------

float RoutePlan::GetArcLength(NodeIndex node) const
{
	const RouteNode& n = GetNode(node);
	if (n.parent == k_None)
	{
		return 0.0f;
	}
	const RouteNode& parent = GetNode(n.parent);
	if (parent.obj == -1)
	{
		return 0.0f;
	}
	const auto& o = _holder->Object(parent.obj);
	// arc == 1 -> from this.from to this.to; else from the parent's to to this.from
	const Point2D a = n.arc == 1 ? n.from : parent.to;
	const Point2D b = n.arc == 1 ? n.to : n.from;
	// the original's arctangent keeps its full precision (only + - x / sqrt are rounded to float): the angles stay
	// double until the first rounded operation
	double ta = std::atan2(static_cast<double>(a.z - o.c.z), static_cast<double>(a.x - o.c.x));
	double tb = std::atan2(static_cast<double>(b.z - o.c.z), static_cast<double>(b.x - o.c.x));
	constexpr float k_TwoPi = k_Pi + k_Pi;
	if (parent.side == 2)
	{
		if (tb < ta)
		{
			tb = static_cast<float>(tb + k_TwoPi);
		}
		return static_cast<float>(tb - ta) * o.r;
	}
	if (ta < tb)
	{
		ta = static_cast<float>(ta + k_TwoPi);
	}
	return static_cast<float>(ta - tb) * o.r;
}

void RoutePlan::SetCost(NodeIndex node)
{
	RouteNode& n = Node(node);
	n.cost = n.parent != k_None ? GetNode(n.parent).cost : 0.0f;
	if (n.arc == 0)
	{
		n.cost = n.from.DistanceTo(n.to) + n.cost;
	}
	else
	{
		const float arc = GetArcLength(node);
		Node(node).cost = arc + Node(node).cost;
	}
}

// ---- the route list -------------------------------------------------------------------------------------------------

void RoutePlan::DropRoute(RouteIndex route)
{
	if (_best == route)
	{
		_best = k_None;
	}
	if (route == _routeList)
	{
		const auto next = RouteAt(route).next;
		DeleteRoute(route);
		_routeList = next;
		if (next != k_None)
		{
			return;
		}
		// nothing left
		_state = State::Failed;
		FreeVisited();
		return;
	}
	for (RouteIndex at = _routeList; at != k_None; at = RouteAt(at).next)
	{
		if (RouteAt(at).next == route)
		{
			RouteAt(at).next = RouteAt(route).next;
			DeleteRoute(route);
			return;
		}
	}
}

void RoutePlan::CheckArrived()
{
	if (_best != k_None && GetNode(GetRoute(_best).last).to.DistanceTo(_dest) < _near)
	{
		Finish();
	}
}

void RoutePlan::Finish()
{
	for (RouteIndex at = _routeList; at != k_None;)
	{
		const auto next = RouteAt(at).next;
		if (at != _best)
		{
			DeleteRoute(at);
		}
		at = next;
	}
	_routeList = _best;
	_routeCount = 1;
	RouteAt(_best).next = k_None;
	_estimate = GetNode(GetRoute(_best).last).cost;
	_state = State::Found;
	FreeVisited();
}

bool RoutePlan::Visit(const Point2D& p, float cost, float limit)
{
	if (!(cost <= limit))
	{
		return true;
	}
	const float xLow = p.x - k_Epsilon;
	const float xHigh = p.x + k_Epsilon;
	const float zLow = p.z - k_Epsilon;
	const float zHigh = p.z + k_Epsilon;
	// the blocks from the newest, each from its first entry
	for (auto block = _visited.rbegin(); block != _visited.rend(); ++block)
	{
		for (int32_t i = 0; i < block->count; ++i)
		{
			const auto& e = block->entries.at(static_cast<size_t>(i));
			if (e.x <= xLow || !(e.x < xHigh) || e.z <= zLow || !(e.z < zHigh))
			{
				continue;
			}
			if (ObstacleGrid::HasObstacleHook() && cost > _worstRejected)
			{
				_worstRejected = cost;
			}
			return true;
		}
	}
	// at the newest block, a new one when it is full
	if (_visited.empty() || _visited.back().count == 256)
	{
		_visited.emplace_back();
	}
	auto& block = _visited.back();
	block.entries.at(static_cast<size_t>(block.count)) = {p.x, p.z, cost};
	++block.count;
	++_visitedCount;
	return false;
}

void RoutePlan::FreeVisited()
{
	_visited.clear();
	_visitedCount = 0;
}

void RoutePlan::FreeRoutes()
{
	for (RouteIndex at = _routeList; at != k_None;)
	{
		const auto next = RouteAt(at).next;
		DeleteRoute(at);
		at = next;
	}
	_routeList = k_None;
	_best = k_None;
	_routeCount = 0;
	_state = State::None;
	FreeVisited();
}

// ---- SetStart / SetDest ---------------------------------------------------------------------------------------------

void RoutePlan::SetStart(const Point2D& start, float r, ObstacleGrid& holder, int32_t objA, int32_t objB, int32_t sideB)
{
	FreeRoutes();
	_holder = &holder;
	_radius = r;
	_objA = objA;
	_objB = objB;
	_sideB = sideB;
	_dest = start;
	_start = start;
	_state = State::Set;
}

void RoutePlan::SetDest(const Point2D& dest, float r, float nearDistance, float startCost, int32_t destObj, int32_t destSide,
                        float maxLength, bool linkHolder)
{
	_startCost = startCost;
	_worstRejected = 0.0f;
	if (linkHolder)
	{
		_holder->SetEnds(&_dest, &_start);
	}
	FreeRoutes();
	const auto route = NewRoute();
	_routeList = route;
	_best = route;
	_dest = dest;
	_destRadius = r;
	_near = nearDistance;
	_destObj = destObj;
	_destSide = destSide;
	_maxLength = maxLength;
	_state = State::Searching;
	_routeCount = 0;

	Point2D p = _dest;
	if (_destObj == -1)
	{
		// the dest inside an obstacle -> DestInside
		for (int32_t i = 0; i < _holder->ObjectCount(); ++i)
		{
			if (_holder->Object(i).ContainsCircle(_dest, _near))
			{
				_state = State::DestInside;
				return;
			}
		}
	}
	else if (_holder->StartObstacleSidePoint(_destObj, _start, p, OtherSide(_destSide)) == 0)
	{
		_state = State::Failed;
		return;
	}
	// the first obstacle on the way to the dest
	Point2D q;
	int32_t side = 0;
	const int32_t obj = _holder->GetFirstObject(_start, p, -1, q, side, _destRadius);
	if (obj == -1)
	{
		if (_destObj != -1)
		{
			const auto& od = _holder->Object(_destObj);
			const float v =
			    ((_start.x - _destTo.x) * (od.c.x - _destTo.x) + (_start.z - _destTo.z) * (od.c.z - _destTo.z)) / od.r;
			if (v < k_Epsilon)
			{
				_state = State::Failed;
				return;
			}
		}
		// found at once
		_state = State::Found;
		const auto n = NewNode(k_None, 0, _start, p, -1, 0);
		RouteAt(route).first = n;
		RouteAt(route).last = n;
		Node(n).cost = _start.DistanceTo(p) + _startCost;
		_estimate = GetNode(n).to.DistanceTo(p) + GetNode(n).cost;
		return;
	}
	ObstacleGrid::NotifyObstacle(_holder->Object(obj).id);
	if (obj != _objA && obj != _objB)
	{
		// round the obstacle, one way in this route, the other way in a second one
		const float d = _start.DistanceTo(q);
		const auto n = NewNode(k_None, 0, _start, q, obj, OtherSide(side));
		RouteAt(route).first = n;
		RouteAt(route).last = n;
		Node(n).cost = d + _startCost;
		const float r2 = GetNode(n).to.DistanceTo(p);
		if (r2 < _near)
		{
			_state = State::Found;
			_estimate = r2 + GetNode(n).cost;
			return;
		}
		const auto second = NewRoute();
		RouteAt(second).next = _routeList;
		_routeList = second;
		const auto n2 = NewNode(k_None, 0, _start, q, obj, side);
		RouteAt(second).first = n2;
		RouteAt(second).last = n2;
		Node(n2).cost = d + _startCost;
		_estimate = r2 + GetNode(n2).cost;
		return;
	}
	// starting on an obstacle
	const auto n = NewNode(k_None, 0, _start, _start, _objA, OtherSide(_sideB));
	RouteAt(route).first = n;
	RouteAt(route).last = n;
	Node(n).cost = _startCost;
	Node(n).startsOnObject = 1;
	NodeIndex lastMade = n;
	if (_objB != -1)
	{
		const auto second = NewRoute();
		RouteAt(second).next = _routeList;
		_routeList = second;
		const auto n2 = NewNode(k_None, 0, _start, _start, _objB, _sideB);
		RouteAt(second).first = n2;
		RouteAt(second).last = n2;
		Node(n2).cost = _startCost;
		Node(n2).startsOnObject = 1;
		lastMade = n2;
	}
	_estimate = GetNode(lastMade).to.DistanceTo(p) + GetNode(lastMade).cost;
}

// ---- StepSearch -------------------------------------------------------------------------------------------------

void RoutePlan::StepSearch(int32_t depth)
{
	if (_state != State::Searching)
	{
		return;
	}
	// the best route by estimate (a later one must beat it by 0.001)
	_best = _routeList;
	_estimate = GetNode(GetRoute(_best).last).to.DistanceTo(_dest) + GetNode(GetRoute(_best).last).cost;
	_routeCount = 1;
	for (RouteIndex at = GetRoute(_best).next; at != k_None; at = GetRoute(at).next)
	{
		++_routeCount;
		const float estimate = GetNode(GetRoute(at).last).to.DistanceTo(_dest) + GetNode(GetRoute(at).last).cost;
		if (estimate < _estimate - k_Epsilon)
		{
			_estimate = estimate;
			_best = at;
		}
	}
	const RouteIndex best = _best;
	const NodeIndex tip = GetRoute(best).last;
	const int32_t tipObj = GetNode(tip).obj;
	if (tipObj == -1)
	{
		DropRoute(best);
		return;
	}
	const auto& o = _holder->Object(tipObj);
	if (o.active == 0)
	{
		DropRoute(best);
		return;
	}
	const int32_t tipSide = GetNode(tip).side;
	const Point2D tipTo = GetNode(tip).to;
	Point2D leave;
	Point2D target;
	int32_t straightToTarget = 0;
	if (_destObj == -1)
	{
		target = _dest;
		straightToTarget = _holder->StartObstacleSidePoint(tipObj, _dest, leave, tipSide);
	}
	else
	{
		if (tipObj == _destObj)
		{
			DropRoute(best);
			return;
		}
		if (tipSide != _destSide)
		{
			const auto& od = _holder->Object(_destObj);
			const float sum = od.r + o.r;
			const float dx = o.c.x - od.c.x;
			const float dz = o.c.z - od.c.z;
			if (sum * sum + k_Epsilon > dz * dz + dx * dx)
			{
				DropRoute(best);
				return;
			}
		}
		if (_holder->GetTangent(tipObj, tipSide, leave, _destObj, _destSide, target) == 0)
		{
			DropRoute(best);
			return;
		}
		const float v = ((o.c.x - tipTo.x) * (target.x - tipTo.x) + (o.c.z - tipTo.z) * (target.z - tipTo.z)) / o.r;
		if (v < k_Epsilon)
		{
			DropRoute(best);
			return;
		}
		straightToTarget = 1;
	}
	// the arc tip.to -> leave round the tip's object
	int32_t arcHit = -1;
	if (_holder->ArcBlocked(tipObj, tipSide, leave, tipTo, arcHit) != 0)
	{
		ObstacleGrid::NotifyObstacle(_holder->Object(arcHit).id);
		const auto n = NewNode(tip, 1, tipTo, leave, arcHit, tipSide);
		Node(tip).child = n;
		const float arc = GetArcLength(n);
		Node(n).cost = arc + GetNode(tip).cost;
		RouteAt(best).last = n;
		if (Visit(GetNode(n).to, GetNode(n).cost, _maxLength))
		{
			DropRoute(best);
			return;
		}
		RouteAt(best).shortcutDone = GetNode(tip).startsOnObject;
		Shortcut(best, depth);
		CheckArrived();
		return;
	}
	// the leg leave -> target
	Point2D hit;
	int32_t side2 = 0;
	const int32_t obj2 = _holder->GetFirstObject(leave, target, tipObj, hit, side2, _destRadius);
	if (obj2 == -1)
	{
		if (_destObj != -1)
		{
			const auto& od = _holder->Object(_destObj);
			const float v =
			    ((leave.x - _destTo.x) * (od.c.x - _destTo.x) + (leave.z - _destTo.z) * (od.c.z - _destTo.z)) / od.r;
			if (v < k_Epsilon)
			{
				DropRoute(best);
				return;
			}
			Point2D end = _destTo;
			int32_t ignored = 0;
			if (_holder->ArcBlocked(_destObj, _destSide, end, target, ignored) != 0)
			{
				DropRoute(best);
				return;
			}
		}
		RouteAt(best).shortcutDone = GetNode(tip).startsOnObject;
		const auto n1 = NewNode(tip, 1, tipTo, leave, -1, 0);
		Node(tip).child = n1;
		const float arc = GetArcLength(n1);
		Node(n1).cost = arc + GetNode(tip).cost;
		if (straightToTarget != 0)
		{
			const auto n2 = NewNode(n1, 0, leave, target, -1, 0);
			Node(n1).child = n2;
			Node(n2).cost = GetNode(n2).to.DistanceTo(GetNode(n2).from) + GetNode(n1).cost;
			RouteAt(best).last = n2;
		}
		else
		{
			RouteAt(best).last = n1;
		}
		Shortcut(best, depth);
		Finish();
		return;
	}
	ObstacleGrid::NotifyObstacle(_holder->Object(obj2).id);
	const auto& o2 = _holder->Object(obj2);
	const float sum = o2.r + o.r;
	const float dx = o.c.x - o2.c.x;
	const float dz = o.c.z - o2.c.z;
	if (sum * sum > dz * dz + dx * dx)
	{
		// obj2 overlaps the tip's circle: on round obj2 the same way
		RouteAt(best).shortcutDone = GetNode(tip).startsOnObject;
		const auto n1 = NewNode(tip, 1, tipTo, leave, -1, 0);
		Node(tip).child = n1;
		const float arc = GetArcLength(n1);
		Node(n1).cost = arc + GetNode(tip).cost;
		const auto n2 = NewNode(n1, 0, leave, hit, obj2, tipSide);
		Node(n1).child = n2;
		Node(n2).cost = GetNode(n2).to.DistanceTo(GetNode(n2).from) + GetNode(n1).cost;
		RouteAt(best).last = n2;
		Shortcut(best, depth);
		CheckArrived();
		return;
	}
	// apart; first the tip's whole way round may be cut by another circle
	Point2D around = tipTo;
	int32_t aroundHit = -1;
	if (_holder->ArcBlocked(tipObj, tipSide, around, tipTo, aroundHit) != 0)
	{
		ObstacleGrid::NotifyObstacle(_holder->Object(aroundHit).id);
		const auto copy = CopyRoute(best);
		RouteAt(copy).next = _routeList;
		_routeList = copy;
		RouteAt(copy).shortcutDone = GetNode(tip).startsOnObject;
		const auto copyTip = GetRoute(copy).last;
		const auto nb = NewNode(copyTip, 1, tipTo, around, aroundHit, tipSide);
		const float arc = GetArcLength(nb);
		Node(nb).cost = arc + GetNode(copyTip).cost;
		Node(copyTip).child = nb;
		RouteAt(copy).last = nb;
		if (Visit(GetNode(nb).to, GetNode(nb).cost, _maxLength))
		{
			DropRoute(copy);
		}
		else
		{
			Shortcut(copy, depth);
		}
	}
	// round obj2, one way here and the other in a copy
	if (_best == k_None)
	{
		return;
	}
	RouteAt(_best).shortcutDone = GetNode(tip).startsOnObject;
	const auto n1 = NewNode(tip, 1, tipTo, leave, -1, 0);
	Node(tip).child = n1;
	const float arc = GetArcLength(n1);
	Node(n1).cost = arc + GetNode(tip).cost;
	RouteAt(_best).last = n1;
	const float d = leave.DistanceTo(hit);
	const auto n2 = NewNode(n1, 0, leave, hit, obj2, OtherSide(side2));
	Node(n2).cost = d + GetNode(n1).cost;
	Node(n1).child = n2;
	RouteAt(_best).last = n2;
	Shortcut(_best, depth);
	const auto copy = CopyRoute(_best);
	RouteAt(copy).next = _routeList;
	const auto copyTip = GetRoute(copy).last;
	Node(copyTip).side = OtherSide(GetNode(copyTip).side);
	_routeList = copy;
	CheckArrived();
}

// ---- the shortcut ---------------------------------------------------------------------------------------------------

void RoutePlan::Shortcut(RouteIndex route, int32_t depth)
{
	if (RouteAt(route).shortcutDone != 0)
	{
		return;
	}
	if (depth > k_MaxShortcutDepth)
	{
		RouteAt(route).shortcutDone = 1;
		return;
	}
	// B (the tip, or its parent when the tip is not an arc) and E (up to the first arc, or the
	// route's first node)
	const auto fail = [this, route]() { RouteAt(route).shortcutDone = 1; };
	const NodeIndex tip = GetRoute(route).last;
	const NodeIndex b = GetNode(tip).arc == 1 ? tip : GetNode(tip).parent;
	if (b == k_None || GetNode(b).arc != 1)
	{
		fail();
		return;
	}
	NodeIndex e = GetNode(b).parent;
	if (e == k_None)
	{
		fail();
		return;
	}
	const NodeIndex routeFirst = GetRoute(route).first;
	while (e != routeFirst && GetNode(e).arc != 1)
	{
		e = GetNode(e).parent;
		if (e == k_None)
		{
			fail();
			return;
		}
	}
	if (GetNode(e).arc != 1 && GetNode(e).side == 0)
	{
		fail();
		return;
	}
	const int32_t objA = GetNode(GetNode(b).parent).obj;
	int32_t tries = 0;
	RoutePlan nested;
	for (;;)
	{
		// the nested plan starts at B's end
		const RouteNode& bn = GetNode(b);
		if (bn.arc == 1)
		{
			nested.SetStart(bn.to, _radius, *_holder, objA, bn.obj, GetNode(bn.parent).side);
		}
		else
		{
			nested.SetStart(bn.to, _radius, *_holder, bn.obj, -1, GetNode(GetNode(bn.parent).parent).side);
		}
		// and ends at E
		if (GetNode(e).arc == 0)
		{
			nested.SetDest(GetNode(e).from, 0.0f, 0.0f, 0.0f, -1, 0, GetNode(b).cost, false);
		}
		else
		{
			const NodeIndex p = GetNode(e).parent;
			if (p == k_None)
			{
				fail();
				return;
			}
			const float maxLength = GetNode(b).cost - GetNode(p).cost;
			if (GetNode(p).obj == -1)
			{
				fail();
				return;
			}
			nested._destFrom = GetNode(p).from;
			nested._destTo = GetNode(p).to;
			// the dest argument is a zeroed point with a destObj
			nested.SetDest(Point2D {}, 0.0f, 0.0f, 0.0f, GetNode(p).obj, OtherSide(GetNode(p).side), maxLength, false);
		}
		// at most 16 steps while it searches
		for (int32_t step = 0; step < k_ShortcutSteps; ++step)
		{
			const auto state = nested.GetState();
			if (state == State::Searching)
			{
				nested.StepSearch(depth + 1);
			}
			else if (state == State::DestInside || state == State::Failed || state == State::Found)
			{
				break;
			}
		}
		if (nested.GetState() == State::Found)
		{
			break;
		}
		// another, earlier E
		++tries;
		e = tries >= k_ShortcutTries ? k_None : GetNode(e).parent;
		if (e == k_None)
		{
			fail();
			return;
		}
		if (GetNode(e).arc != 0 || e == GetRoute(route).first)
		{
			continue;
		}
		e = GetNode(e).parent;
		if (e == k_None || GetNode(e).arc != 1)
		{
			fail();
			return;
		}
	}
	// the splice point
	const RouteNode tipCopy = GetNode(GetRoute(route).last);
	const NodeIndex nestedTip = nested.GetRoute(nested.GetBestRoute()).last;
	NodeIndex freeFrom = k_None;
	NodeIndex cur = k_None;
	{
		const RouteNode& n = nested.GetNode(nestedTip);
		if (GetNode(e).arc == 0)
		{
			freeFrom = e;
			if (n.arc == 1)
			{
				const RouteNode& np = nested.GetNode(n.parent);
				cur = NewNode(k_None, 0, n.to, n.to, np.obj, OtherSide(np.side));
				RouteAt(route).first = cur;
				Node(cur).cost = GetNode(cur).from.DistanceTo(GetNode(cur).to) + _startCost;
			}
		}
		else
		{
			freeFrom = GetNode(e).child;
			Node(e).to = n.to;
			if (n.arc == 0)
			{
				Node(e).obj = -1;
				Node(e).side = 0;
			}
			else
			{
				const RouteNode& np = nested.GetNode(n.parent);
				Node(e).obj = np.obj;
				Node(e).side = OtherSide(np.side);
			}
			SetCost(e);
			Node(e).child = k_None;
			cur = e;
		}
	}
	// the nested route reversed onto this one
	for (NodeIndex m = nestedTip; m != k_None;)
	{
		const RouteNode n = nested.GetNode(m);
		if (n.parent != k_None)
		{
			int32_t obj = -1;
			int32_t side = 0;
			const RouteNode& np = nested.GetNode(n.parent);
			if (n.arc == 0 || np.arc == 1)
			{
				const RouteNode& g = nested.GetNode(np.parent);
				obj = g.obj;
				side = g.side == 1 ? 2 : (g.side == 2 ? 1 : 0);
			}
			const auto made = NewNode(cur, n.arc, n.to, n.from, obj, side);
			if (cur != k_None)
			{
				Node(cur).child = made;
				SetCost(made);
			}
			else
			{
				RouteAt(route).first = made;
				Node(made).cost = GetNode(made).from.DistanceTo(GetNode(made).to) + _startCost;
			}
			cur = made;
			m = n.parent;
			continue;
		}
		// the nested route's first node
		if (n.to.x == tipCopy.to.x && n.to.z == tipCopy.to.z)
		{
			if (cur != k_None)
			{
				Node(cur).obj = tipCopy.obj;
				Node(cur).side = tipCopy.side;
			}
			else
			{
				cur = NewNode(k_None, n.arc, n.to, tipCopy.to, tipCopy.obj, tipCopy.side);
				RouteAt(route).first = cur;
				SetCost(cur);
			}
		}
		else
		{
			const auto made = NewNode(cur, n.arc, n.to, tipCopy.to, tipCopy.obj, tipCopy.side);
			SetCost(made);
			if (cur != k_None)
			{
				Node(cur).child = made;
			}
			else
			{
				RouteAt(route).first = made;
			}
			cur = made;
		}
		break;
	}
	// the new tip, the estimate, the replaced nodes freed
	RouteAt(route).last = cur;
	_estimate = GetNode(cur).to.DistanceTo(_dest) + GetNode(cur).cost;
	for (NodeIndex at = freeFrom; at != k_None;)
	{
		const auto next = GetNode(at).child;
		FreeNode(at);
		at = next;
	}
	RouteAt(route).shortcutDone = 1;
}
