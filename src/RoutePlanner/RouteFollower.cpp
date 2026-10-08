/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RouteFollower.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <utility>

// The follower (RouteFollower.h). The FPU runs at 24-bit precision: every add / subtract / multiply / divide is a float
// operation here, one per statement; the arc tangent, sine and cosine keep their full precision (double) until the next
// rounded store.

namespace openblack::route_planner
{
namespace
{
constexpr float k_Pi = 3.14159265358979f; ///< Pi as a stored float
constexpr float k_StepFactor = 0.1f;      ///< MoveAlongRoute's step = 0.1 x speed
constexpr float k_MinLength = 0.01f;
constexpr float k_StraightMin = 0.001f;
constexpr float k_ReplanFraction = 0.25f;                          ///< SetDest case 3's spacing of the re-plans
constexpr float k_NewDestRatio = 0.5f;                             ///< SetDest case 2 / 4
constexpr float k_SubDestNear = std::bit_cast<float>(0x3A83126Fu); ///< 0.001, SpliceAndShortcut's SetDest
constexpr int32_t k_ShortcutTries = 4;                             ///< Targets the shortcut search tries
constexpr int32_t k_ShortcutTurns = 0x20;                          ///< Search turns per shortcut target
constexpr int32_t k_MaxReplans = 4;                                ///< SetDest case 3
} // namespace

RouteFollower::RouteFollower()
{
	// The fields' initial values are the members' (radius 1.0, _x6403C 5.0, one turn per update, the rest 0); the
	// holder's planner is this one
	_planAt.fill(k_None);
	SetPlan(this);
}

RouteFollower::~RouteFollower() = default;

void RouteFollower::Init(int32_t context, DoneFn done, StepFn step, RadiusFn radius, int32_t turnsPerUpdate)
{
	_context = context;
	_done = std::move(done);
	_step = std::move(step);
	_radius = std::move(radius);
	_turnsPerUpdate = turnsPerUpdate;
}

// ---- the node pool ---------------------------------------------------------------------------------------------------

NodeIndex RouteFollower::NewNode(const RouteNode& copy)
{
	NodeIndex index;
	if (!_freeNodes.empty())
	{
		index = _freeNodes.back();
		_freeNodes.pop_back();
		_nodes.at(static_cast<size_t>(index)) = copy;
	}
	else
	{
		index = static_cast<NodeIndex>(_nodes.size());
		_nodes.push_back(copy);
	}
	return index;
}

void RouteFollower::FreeNode(NodeIndex node)
{
	_freeNodes.push_back(node);
}

std::pair<NodeIndex, NodeIndex> RouteFollower::CopyBestRoute(const RoutePlan& plan)
{
	const auto best = plan.GetBestRoute();
	if (best == k_None)
	{
		return {k_None, k_None};
	}
	NodeIndex first = k_None;
	NodeIndex last = k_None;
	for (auto n = plan.GetRoute(best).first; n != k_None; n = plan.GetNode(n).child)
	{
		auto copy = plan.GetNode(n);
		copy.child = k_None;
		copy.parent = last;
		const auto index = NewNode(copy);
		if (last != k_None)
		{
			Node(last).child = index;
		}
		else
		{
			first = index;
		}
		last = index;
	}
	return {first, last};
}

std::vector<RouteNode> RouteFollower::GetRouteAhead() const
{
	std::vector<RouteNode> out;
	for (auto n = _current; n != k_None; n = Node(n).child)
	{
		out.push_back(Node(n));
	}
	return out;
}

float RouteFollower::RemainingLength() const
{
	if (_current == k_None)
	{
		return 0.0f;
	}
	float remaining = std::max(Length(_current) - _travelled, 0.0f);
	for (auto n = Node(_current).child; n != k_None; n = Node(n).child)
	{
		remaining += Length(n);
	}
	return remaining;
}

// ---- RouteNode ---------------------------------------------------------------------------------------------------------

float RouteFollower::ArcLength(NodeIndex node) const
{
	// The circle and the side are the PREVIOUS node's
	const auto& n = Node(node);
	if (n.parent == k_None)
	{
		return 0.0f;
	}
	const auto& prev = Node(n.parent);
	if (prev.obj == -1)
	{
		return 0.0f;
	}
	const auto& o = Object(prev.obj);
	// arc == 1 -> from this.from to this.to; else from prev.to to this.from (not reachable from Length)
	const Point2D a = n.arc == 1 ? n.from : prev.to;
	const Point2D b = n.arc == 1 ? n.to : n.from;
	double ta = std::atan2(static_cast<double>(a.z - o.c.z), static_cast<double>(a.x - o.c.x));
	double tb = std::atan2(static_cast<double>(b.z - o.c.z), static_cast<double>(b.x - o.c.x));
	constexpr float k_TwoPi = k_Pi + k_Pi;
	if (prev.side == 2)
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

float RouteFollower::Length(NodeIndex node) const
{
	// Type 1 -> the arc; else DistanceTo(from, to)
	const auto& n = Node(node);
	return n.arc == 1 ? ArcLength(node) : n.from.DistanceTo(n.to);
}

void RouteFollower::Cumulate(NodeIndex node)
{
	// The previous node's cost (0 without one) + the node's own length; type 0 straight, else the arc
	auto& n = Node(node);
	n.cost = n.parent != k_None ? Node(n.parent).cost : 0.0f;
	if (n.arc == 0)
	{
		n.cost = n.from.DistanceTo(n.to) + n.cost;
	}
	else
	{
		const float arc = ArcLength(node);
		Node(node).cost = arc + Node(node).cost;
	}
}

void RouteFollower::CutAfter(NodeIndex node)
{
	// Every node after `node` deleted, its child cleared, the list's tail = it
	for (auto n = Node(node).child; n != k_None;)
	{
		const auto next = Node(n).child;
		FreeNode(n);
		n = next;
	}
	Node(node).child = k_None;
	_routeTail = node;
}

bool RouteFollower::RouteMeets(NodeIndex from, const RouteObstacle& entry)
{
	// The nodes from `from` on
	for (auto n = from; n != k_None; n = Node(n).child)
	{
		const auto prev = Node(n).parent;
		if (Node(n).arc == 0)
		{
			// The straight node from its start (the follower's position for `from`) to its end; the first obstacle on it,
			// ignoring the circle before it
			const int32_t ignore = prev != k_None ? Node(prev).obj : -1;
			const Point2D start = n == from ? _position : Node(n).from;
			Point2D hit;
			int32_t side = 0;
			// The node's own end, passed by reference
			const int32_t index = GetFirstObject(start, Node(n).to, ignore, hit, side, 0.0f);
			// The hit is this entry
			if (index >= 0 && &Object(index) == &entry)
			{
				// n ends at the hit, round it, and the route is cut after it
				Node(n).to = hit;
				Node(n).obj = index;
				CutAfter(n);
				return true;
			}
			continue;
		}
		// An arc on the previous node's circle: the new circle overlaps it
		if (prev == k_None || Node(prev).obj == -1)
		{
			continue;
		}
		const auto& o = Object(Node(prev).obj);
		const float rr = o.r + entry.r;
		const Point2D d {o.c.x - entry.c.x, o.c.z - entry.c.z};
		const float dsq = d.LengthSquared();
		// Overlap when rr^2 > dsq
		if (rr * rr > dsq)
		{
			if (n == from)
			{
				// The first node ends at the follower
				Node(n).to = _position;
				CutAfter(n);
			}
			else
			{
				CutAfter(prev);
			}
			return true;
		}
	}
	return false;
}

bool RouteFollower::PlanRouteMeets(const RoutePlan& plan, NodeIndex from, const RouteObstacle& entry)
{
	// RouteMeets on a plan's route, without the cut; `to` is a copy: the plan is freed on a hit
	for (auto n = from; n != k_None; n = plan.GetNode(n).child)
	{
		const auto& node = plan.GetNode(n);
		const auto prev = node.parent;
		if (node.arc == 0)
		{
			const int32_t ignore = prev != k_None ? plan.GetNode(prev).obj : -1;
			const Point2D start = n == from ? _position : node.from;
			Point2D to = node.to;
			Point2D hit;
			int32_t side = 0;
			const int32_t index = GetFirstObject(start, to, ignore, hit, side, 0.0f);
			if (index >= 0 && &Object(index) == &entry)
			{
				return true;
			}
			continue;
		}
		if (prev == k_None || plan.GetNode(prev).obj == -1)
		{
			continue;
		}
		const auto& o = Object(plan.GetNode(prev).obj);
		const float rr = o.r + entry.r;
		const Point2D d {o.c.x - entry.c.x, o.c.z - entry.c.z};
		const float dsq = d.LengthSquared();
		if (rr * rr > dsq)
		{
			return true;
		}
	}
	return false;
}

void RouteFollower::PositionAt(float t)
{
	// The follower's position and heading at distance t along the current node
	auto& n = Node(_current);
	// An arc with no circle before it is straight
	if (n.arc == 1 && (n.parent == k_None || Node(n.parent).obj == -1))
	{
		n.arc = 0;
	}
	const float length = Length(_current);
	const auto& node = Node(_current);
	if (node.arc == 1)
	{
		const auto& prev = Node(node.parent);
		const auto& o = Object(prev.obj);
		double ta = std::atan2(static_cast<double>(node.from.z - o.c.z), static_cast<double>(node.from.x - o.c.x));
		double tb = std::atan2(static_cast<double>(node.to.z - o.c.z), static_cast<double>(node.to.x - o.c.x));
		constexpr float k_TwoPi = k_Pi + k_Pi;
		if (prev.side == 2)
		{
			if (tb < ta)
			{
				tb = static_cast<float>(tb + k_TwoPi);
			}
		}
		else if (ta < tb)
		{
			ta = static_cast<float>(ta + k_TwoPi);
		}
		// theta = ta + (tb - ta) x t / length
		const float span = static_cast<float>(tb - ta);
		const float f = t / length;
		const float theta = static_cast<float>(ta + static_cast<double>(span * f));
		// The heading turns a half when going the other way round
		_heading = prev.side == 2 ? theta + k_Pi : theta;
		// Cosine / sine in full precision, x r, + c
		const auto cx = static_cast<float>(std::cos(static_cast<double>(theta)) * static_cast<double>(o.r));
		const auto sz = static_cast<float>(std::sin(static_cast<double>(theta)) * static_cast<double>(o.r));
		_position.x = cx + o.c.x;
		_position.z = sz + o.c.z;
		return;
	}
	// Straight, only when longer than 0.001
	if (!(length > k_StraightMin))
	{
		return;
	}
	const Point2D d {node.to.x - node.from.x, node.to.z - node.from.z};
	const float f = t / length;
	const float dx = d.x * f;
	const float dz = d.z * f;
	_position.x = dx + node.from.x;
	_position.z = dz + node.from.z;
	_heading = d.GetHeading();
}

// ---- the plans' ends ---------------------------------------------------------------------------------------------------

void RouteFollower::FreePlans()
{
	// plan[0..count) deleted, count = 0
	for (int32_t i = 0; i < _planCount; ++i)
	{
		_plans.at(static_cast<size_t>(i)).reset();
	}
	_planCount = 0;
}

void RouteFollower::OnFailed()
{
	// States 0, 1 and 3 -> nothing
	if (_state == State::None || _state == State::Idle || _state == State::Following)
	{
		return;
	}
	_state = static_cast<State>(static_cast<int32_t>(_state) - 1);
	const auto planState = _plans.at(static_cast<size_t>(_bestPlan))->GetState();
	FreePlans();
	if (_destPending != 0)
	{
		// The pending dest planned now; done(2) unless the plan failed (1)
		_destPending = 0;
		SetDest(_dest, _destA, _destB, _destRadius, _destC);
		if (_done)
		{
			_done(_context, planState != RoutePlan::State::Failed ? 2 : 1);
		}
		return;
	}
	// done(0), or done(3) for a failed plan
	if (_done)
	{
		_done(_context, planState != RoutePlan::State::Failed ? 0 : 3);
	}
}

void RouteFollower::OnFound()
{
	// Found once, then by the state: 0, 1 and 3 return at once
	_foundOnce = 1;
	if (_state != State::Planning && _state != State::Replanning)
	{
		return;
	}
	if (_state == State::Planning)
	{
		// The main plan becomes the route
		const auto& plan = *_plans.at(0);
		const auto [first, last] = CopyBestRoute(plan);
		_hasMainPlan = true;
		_routeHead = first;
		_routeTail = last;
		_mainDest = plan.GetDest();
		_mainDestRadius = plan.GetDestRadius();
		_mainNear = plan.GetNearDistance();
		_plans.at(0).reset(); // (openblack) the original keeps plan[0] as the route's owner; its nodes are copied
		_planCount = 0;
		_state = State::Following;
		_current = _routeHead;
		_travelled = 0.0f;
		// The next plan's start = the route's end
		_planStart = Node(_routeTail).to;
		PositionAt(_travelled);
		if (_step)
		{
			// The first node's length, at least 0.01
			float length = Length(_current);
			if (length < k_MinLength)
			{
				length = k_MinLength;
			}
			_step(_context, 0.0f, length);
		}
	}
	else if (_state == State::Replanning)
	{
		SpliceAndShortcut(_planAt.at(static_cast<size_t>(_bestPlan)));
		_state = State::Following;
	}
	// No plan left and a pending dest -> planned now
	if (_planCount == 0 && _destPending != 0)
	{
		_destPending = 0;
		SetDest(_dest, _destA, _destB, _destRadius, _destC);
	}
}

void RouteFollower::SpliceAndShortcut(NodeIndex at)
{
	auto& sub = *_plans.at(static_cast<size_t>(_bestPlan));
	// 1. Every node after `at` deleted
	for (auto n = Node(at).child; n != k_None;)
	{
		const auto next = Node(n).child;
		FreeNode(n);
		n = next;
	}
	// 2. at -> the sub-plan's route, the route's tail = its tail
	const auto [subFirst, subLast] = CopyBestRoute(sub);
	Node(at).child = subFirst;
	if (subFirst != k_None)
	{
		Node(subFirst).parent = at;
	}
	_routeTail = subLast;
	// 3. The main plan's end = the sub-plan's
	_mainDest = sub.GetDest();
	_planStart = Node(_routeTail).to;
	_mainNear = sub.GetNearDistance();
	_mainDestRadius = sub.GetDestRadius();
	FreePlans();
	// 4. r = the radius ahead (0 without the callback); L = the current node's length
	const float r = _radius ? _radius(_context) : 0.0f;
	const float length = Length(_current);
	const float d = length - _travelled;
	// 5. The look-ahead point P
	Point2D p;
	bool lookAhead;
	if (r > k_MinLength && d > k_MinLength)
	{
		if (d > r + k_MinLength)
		{
			// r further along the current node
			const Point2D keep = _position;
			const float keepHeading = _heading;
			PositionAt(r + _travelled);
			p = _position;
			_position = keep;
			_heading = keepHeading;
		}
		else
		{
			p = Node(_current).to;
		}
		lookAhead = true;
	}
	else
	{
		p = _position;
		lookAhead = false;
	}
	// 6. The current node starts at the follower; so does the previous one (both its ends)
	Node(_current).from = _position;
	_travelled = 0.0f;
	if (_routeHead != _current)
	{
		auto& prev = Node(Node(_current).parent);
		prev.to = _position;
		prev.from = _position;
	}
	// 7. A local plan from P
	RoutePlan local;
	local.SetStart(p, _destRadius, *this, -1, -1, 0);
	int32_t tries = 0;
	for (auto target = Node(at).child; target != k_None && tries < k_ShortcutTries;)
	{
		// To the target's end, its cost from the current node + L
		const float diff = Node(target).cost - Node(_current).cost;
		const float maxLength = diff + length;
		local.SetDest(Node(target).to, 0.0f, k_SubDestNear, 0.0f, -1, 0, maxLength, false);
		// Up to 32 turns while searching
		for (int32_t turn = 0; turn < k_ShortcutTurns && local.GetState() == RoutePlan::State::Searching; ++turn)
		{
			local.StepSearch(0);
		}
		const auto after = Node(target).child;
		// 8. Found, and the plan's tip is the target's circle (or none): the shortcut
		if (local.GetState() == RoutePlan::State::Found)
		{
			const auto best = local.GetBestRoute();
			const auto tipObj = local.GetNode(local.GetRoute(best).last).obj;
			if (tipObj == Node(target).obj || tipObj == -1)
			{
				auto [first, last] = CopyBestRoute(local);
				// The new tip takes the target's side and circle and its place before what follows
				Node(last).side = Node(target).side;
				Node(last).obj = Node(target).obj;
				Node(last).child = after;
				if (after != k_None)
				{
					Node(after).parent = last;
					Node(target).child = k_None;
				}
				else
				{
					_routeTail = last;
				}
				// The route starts with the new nodes; the next plan's start = its tail's end
				const auto oldHead = _routeHead;
				_routeHead = first;
				_planStart = Node(_routeTail).to;
				// 9-10. With the look-ahead, a straight node from the follower to P first
				if (lookAhead)
				{
					RouteNode n {
					    .from = _position,
					    .to = p,
					    .obj = -1,
					    .side = 0,
					    .arc = 0,
					    .cost = 0.0f,
					    .child = _routeHead,
					    .parent = k_None,
					};
					const auto index = NewNode(n);
					Node(_routeHead).parent = index;
					_routeHead = index;
				}
				// 11. The current node is the head again; the old nodes up to the target go
				_current = _routeHead;
				_travelled = 0.0f;
				for (auto n = oldHead; n != k_None;)
				{
					const auto next = Node(n).child;
					FreeNode(n);
					n = next;
				}
			}
		}
		// 12. The next candidate
		++tries;
		target = after;
	}
	// 13. The local plan is destroyed
}

// ---- SetDest / Update / MoveAlongRoute ---------------------------------------------------------------------------------

void RouteFollower::SetDest(const Point2D& dest, float a, float b, float r, float c)
{
	// c is stored first; r is the radius
	_destC = c;
	while (true)
	{
		const auto s = static_cast<int32_t>(_state);
		if (s - 1 < 0 || s - 1 > 3)
		{
			return;
		}
		if (_state == State::Idle)
		{
			// The main plan
			_destRadius = r;
			_dest = dest;
			_destA = a;
			_destB = b;
			_planCount = 1;
			_bestPlan = -1;
			_plans.at(0) = std::make_unique<RoutePlan>();
			_planAt.at(0) = k_None;
			_plans.at(0)->SetStart(_planStart, _destRadius, *this, -1, -1, 0);
			_plans.at(0)->SetDest(_dest, _destA, _destB, 0.0f, -1, 0, _destC, false);
			_foundOnce = 0;
			_state = State::Planning;
			return;
		}
		if (_state == State::Following)
		{
			// Re-plans of the parts of the route ahead, every quarter of what is left
			_destRadius = r;
			_dest = dest;
			_destA = a;
			_destB = b;
			if (const auto prev = Node(_current).parent; prev != k_None)
			{
				Node(prev).cost = 0.0f;
			}
			for (auto n = _current; n != k_None; n = Node(n).child)
			{
				Cumulate(n);
			}
			const auto tail = _routeTail;
			const float left = Node(tail).cost - Node(_current).cost;
			const float step = left * k_ReplanFraction;
			float threshold = Node(_current).cost;
			for (auto n = _current; n != k_None; n = Node(n).child)
			{
				// The tail always; else while fewer than 4 and the node at least at the threshold
				if (n != tail && (_planCount >= k_MaxReplans || Node(n).cost < threshold))
				{
					continue;
				}
				auto& plan = _plans.at(static_cast<size_t>(_planCount));
				plan = std::make_unique<RoutePlan>();
				_planAt.at(static_cast<size_t>(_planCount)) = n;
				plan->SetStart(Node(n).to, _destRadius, *this, -1, -1, 0);
				plan->SetDest(_dest, _destA, _destB, Node(n).cost, -1, 0, _destC, false);
				threshold = threshold + step;
				++_planCount;
			}
			_state = State::Replanning;
			return;
		}
		// 2 / 4: the new dest much nearer the plan's than the follower -> plan again
		_destRadius = r;
		_dest = dest;
		_destA = a;
		_destB = b;
		_destPending = 1;
		// d1 = |plan[0] dest - follower|, d2 = |plan[0] dest - new dest|; re-planned when d1 x 0.5 < d2: the dest moved
		// by more than half the way left
		const float follower = _plans.at(0)->GetDest().DistanceTo(_position);
		const float moved = _plans.at(0)->GetDest().DistanceTo(_dest);
		const float half = follower * k_NewDestRatio;
		if (!(half < moved))
		{
			return;
		}
		_state = static_cast<State>(static_cast<int32_t>(_state) - 1);
		FreePlans();
		_destPending = 0;
	}
}

void RouteFollower::Update(ObstacleHook hook, int32_t turns)
{
	// The hook installed for this update only, with the creature as its context
	int32_t n;
	if (hook != nullptr)
	{
		SetObstacleHook(hook, reinterpret_cast<void*>(static_cast<intptr_t>(_context)));
		n = turns;
	}
	else
	{
		n = _turnsPerUpdate;
	}
	for (int32_t i = 0; _planCount != 0 && i < n; ++i)
	{
		// The plan with the lowest estimate (a later one only when strictly lower)
		_bestPlan = 0;
		float lowest = _plans.at(0)->GetEstimate();
		for (int32_t k = 1; k < _planCount; ++k)
		{
			if (_plans.at(static_cast<size_t>(k))->GetEstimate() < lowest)
			{
				_bestPlan = k;
				lowest = _plans.at(static_cast<size_t>(k))->GetEstimate();
			}
		}
		auto& plan = *_plans.at(static_cast<size_t>(_bestPlan));
		if (plan.GetState() == RoutePlan::State::Found)
		{
			OnFound();
			break;
		}
		if (plan.GetState() == RoutePlan::State::Searching)
		{
			plan.StepSearch(0);
		}
		const auto state = plan.GetState();
		if (state == RoutePlan::State::Searching)
		{
			continue;
		}
		if (state == RoutePlan::State::DestInside)
		{
			OnFailed();
		}
		else if (state == RoutePlan::State::Failed)
		{
			// With a hook and the dest farther than the worst rejected cost: given up (range <= worst -> the failure end)
			if (hook != nullptr && !(_dest.DistanceTo(_position) <= plan.GetWorstRejected()))
			{
				_state = State::GivenUp;
				FreePlans();
			}
			else
			{
				OnFailed();
			}
		}
		else if (state == RoutePlan::State::Found)
		{
			// While re-planning, the next turn
			if (_state == State::Replanning)
			{
				continue;
			}
			OnFound();
		}
		break;
	}
	// The hook cleared
	SetObstacleHook(nullptr, nullptr);
}

void RouteFollower::MoveAlongRoute()
{
	_lastStep = k_StepFactor * _speed;
	_travelled = _lastStep + _travelled;
	while (_current != k_None)
	{
		const float length = Length(_current);
		if (!(length < _travelled))
		{
			// Still on this node
			PositionAt(_travelled);
			return;
		}
		_travelled = _travelled - length;
		// The first re-plan starts from this node, which is now behind: it goes
		if (_planCount != 0 && _planAt.at(0) == _current && Node(_current).child != k_None)
		{
			_plans.at(0).reset();
			--_planCount;
			for (int32_t i = 0; i < _planCount; ++i)
			{
				_plans.at(static_cast<size_t>(i)) = std::move(_plans.at(static_cast<size_t>(i) + 1));
				_planAt.at(static_cast<size_t>(i)) = _planAt.at(static_cast<size_t>(i) + 1);
			}
			if (_planCount == 0)
			{
				_state = State::Following;
				if (_destPending != 0)
				{
					_destPending = 0;
					SetDest(_dest, _destA, _destB, _destRadius, _destC);
				}
			}
		}
		// The previous node deleted; this one is the head, straight, its start = its end
		auto& n = Node(_current);
		if (n.parent != k_None)
		{
			FreeNode(n.parent);
			n.parent = k_None;
			_routeHead = _current;
			n.arc = 0;
			n.from = n.to;
		}
		_current = Node(_current).child;
	}
	// The route's end
	_hasMainPlan = false;
	if (_routeHead != k_None)
	{
		for (auto n = _routeHead; n != k_None;)
		{
			const auto next = Node(n).child;
			FreeNode(n);
			n = next;
		}
	}
	_routeHead = _routeTail = k_None;
	_position = _planStart;
	if (_state == State::Following)
	{
		Empty();
		_state = State::Idle;
	}
	else if (_state == State::Replanning)
	{
		_state = State::Planning;
	}
}

void RouteFollower::OnObjectAdded(const RouteObstacle& entry)
{
	// State 1 -> nothing. (pending) The original also tests a box (in tens) whose bounds nothing writes: if they stay 0
	// the box is [0, 10] x [0, 10] and almost no obstacle makes the follower re-plan. Not tested here: every new obstacle
	// is taken as inside it (the tests' re-plans rely on that)
	if (_state == State::Idle || _state == State::None)
	{
		return;
	}
	const auto replanAll = [this]() {
		_state = static_cast<State>(static_cast<int32_t>(_state) - 1);
		FreePlans();
		_destPending = 0;
		SetDest(_dest, _destA, _destB, _destRadius, _destC);
	};
	// State 4 -> the main route, then the re-plans; 2 / 6 -> the re-plans only; 3 below
	if (_state == State::Replanning && RouteMeets(_current, entry))
	{
		// The main route meets it -> a fresh plan from its tail
		_planStart = Node(_routeTail).to;
		replanAll();
		return;
	}
	if (_state == State::Replanning || _state == State::Planning || _state == State::GivenUp)
	{
		for (int32_t i = 0; i < _planCount; ++i)
		{
			const auto& plan = *_plans.at(static_cast<size_t>(i));
			for (auto route = plan.GetRouteList(); route != k_None; route = plan.GetRoute(route).next)
			{
				// Each route of the plan, from its first node. The original also cuts the plan's route, which does not
				// matter: the plans are all freed on a hit
				if (PlanRouteMeets(plan, plan.GetRoute(route).first, entry))
				{
					replanAll();
					return;
				}
			}
		}
		return;
	}
	if (_state == State::Following)
	{
		// The route meets it -> its end becomes the next plan's start and SetDest re-plans ahead
		if (RouteMeets(_current, entry))
		{
			_planStart = Node(_routeTail).to;
			SetDest(_dest, _destA, _destB, _destRadius, _destC);
		}
	}
}
} // namespace openblack::route_planner
