/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootpathObstacles.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <iterator>
#include <limits>
#include <list>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/X87.h"

using namespace openblack;
using namespace openblack::ecs::footpath_obstacles;
namespace footpath_obstacles = openblack::ecs::footpath_obstacles;
using openblack::ecs::footpath_planning::PlanNode;

namespace
{

/// The nodes under a thing are those nearer than its radius, the square widened by this
constexpr float k_HideReachScale = std::bit_cast<float>(0x3F8147AEu); // 1.01f
/// A line that only touches the thing's circle, with less than this of the square radius left, is not sent round
constexpr double k_LeastOverlap = std::bit_cast<double>(0x3F1A36E2EB1C432Dull); // 0.0001
/// Nodes are stepped round the circle until they are this share of the radius from where the line leaves it
constexpr double k_ArcEndShare = std::bit_cast<double>(0x3FD6666666666666ull); // 0.35
/// Each step round the circle turns by about a tenth of a half turn
constexpr double k_StepCosine = std::bit_cast<double>(0x3FEE6F347A49E1E5ull); // 0.951074828
constexpr double k_StepSine = std::bit_cast<double>(0x3FD3C602CD539F28ull);   // 0.308960629
/// The nodes changed back are within the thing's radius grown by this share and this many metres
constexpr float k_ReturnScale = std::bit_cast<float>(0x3F83D70Au); // 1.03f
constexpr float k_ReturnExtra = std::bit_cast<float>(0x3ECCCCCDu); // 0.4f
/// A bend is cut out when its angle is less than this, as a cross product over a dot product
constexpr double k_StraightLimit = std::bit_cast<double>(0x3F847AE147AE147Bull); // 0.01

using Iterator = std::list<KeyedNode>::iterator;

[[nodiscard]] bool IsAnchor(const KeyedNode& node)
{
	return !node.node.planned && !node.node.hidden && !node.node.marker;
}

[[nodiscard]] bool IsPassedBy(const KeyedNode& node)
{
	return node.node.hidden || node.node.marker;
}

/// Equal to nothing, or not a number: the compare that the game makes with zero sees both as equal
[[nodiscard]] bool ZeroOrUnordered(float value)
{
	return !(value < 0.0f) && !(value > 0.0f);
}

/// The square about a thing that the rules look in, in whole map units. The sides wrap as the game's integers do.
struct Box
{
	uint32_t xMin;
	uint32_t xMax;
	uint32_t zMin;
	uint32_t zMax;

	[[nodiscard]] static Box Around(glm::ivec2 place, int32_t half)
	{
		const auto x = static_cast<uint32_t>(place.x);
		const auto z = static_cast<uint32_t>(place.y);
		const auto h = static_cast<uint32_t>(half);
		return {.xMin = x - h, .xMax = x + h, .zMin = z - h, .zMax = z + h};
	}

	/// Inside the square or on its edge, compared unsigned
	[[nodiscard]] bool HoldsOrTouches(glm::ivec2 place) const
	{
		const auto x = static_cast<uint32_t>(place.x);
		const auto z = static_cast<uint32_t>(place.y);
		return x >= xMin && x <= xMax && z >= zMin && z <= zMax;
	}

	/// Strictly inside the square, compared unsigned
	[[nodiscard]] bool Holds(glm::ivec2 place) const
	{
		const auto x = static_cast<uint32_t>(place.x);
		const auto z = static_cast<uint32_t>(place.y);
		return x > xMin && x < xMax && z > zMin && z < zMax;
	}
};

/// Whether the span between two values overlaps the span between two limits, compared signed and strictly
[[nodiscard]] bool Overlaps(int32_t a, int32_t b, int32_t low, int32_t high)
{
	return std::min(a, b) < high && low < std::max(a, b);
}

/// The path's nodes as the rules change them: the list keeps each node where it is as others come and go
class Path
{
public:
	Path(const std::vector<KeyedNode>& nodes, const Hooks& hooks)
	    : _nodes(nodes.begin(), nodes.end())
	    , _hooks(hooks)
	{
		for (const auto& node : nodes)
		{
			_nextKey = std::max(_nextKey, node.key + 1);
		}
	}

	[[nodiscard]] std::vector<KeyedNode> Nodes() const { return {_nodes.begin(), _nodes.end()}; }
	[[nodiscard]] Iterator Begin() { return _nodes.begin(); }
	[[nodiscard]] Iterator End() { return _nodes.end(); }
	[[nodiscard]] const Hooks& Hook() const { return _hooks; }

	/// The node after one that walkers do not pass by
	[[nodiscard]] Iterator NextShown(Iterator at)
	{
		do
		{
			++at;
		} while (at != _nodes.end() && IsPassedBy(*at));
		return at;
	}

	/// The first anchor from a node on, that node included
	[[nodiscard]] Iterator FirstAnchorFrom(Iterator at)
	{
		while (at != _nodes.end() && !IsAnchor(*at))
		{
			++at;
		}
		return at;
	}

	/// A new node, planned, before another
	Iterator InsertPlanned(Iterator before, glm::ivec2 place)
	{
		return _nodes.insert(before, {.node = {.place = place, .planned = true}, .key = _nextKey++});
	}

	/// A new node, planned, hidden or a marker as given, before another
	Iterator InsertNode(Iterator before, const PlanNode& node)
	{
		return _nodes.insert(before, {.node = node, .key = _nextKey++});
	}

	Iterator Erase(Iterator at) { return _nodes.erase(at); }

	/// The nodes between two anchors go, except those hidden or that are markers. The walkers of every node between,
	/// kept or not, are sent on to one of the two anchors.
	void EraseBetween(Iterator first, Iterator last)
	{
		for (auto at = std::next(first); at != last;)
		{
			_hooks.sendOn(at->key, first->key, first->node.place, last->key, last->node.place);
			at = IsPassedBy(*at) ? std::next(at) : _nodes.erase(at);
		}
	}

	/// The stretch between two anchors planned again at once. When the route planner finds a way, the walkers heading
	/// for the two anchors along the stretch look again, the stretch's old nodes go but those hidden and the markers,
	/// and the route's nodes come after the first anchor, then the marker if one is given. Whether it found a way.
	bool PlanAgain(Iterator first, Iterator last, std::optional<glm::ivec2> marker)
	{
		// The game only plans between two nodes that are there
		if (first == _nodes.end() || last == _nodes.end())
		{
			return false;
		}
		const auto route = _hooks.planRoute(first->node.place, last->node.place);
		if (!route.has_value())
		{
			return false;
		}
		_hooks.lookAgain(last->key, false);
		_hooks.lookAgain(first->key, true);
		EraseBetween(first, last);
		const auto at = std::next(first);
		for (const auto place : *route)
		{
			InsertPlanned(at, place);
		}
		if (marker.has_value())
		{
			InsertNode(at, PlanNode {.place = *marker, .planned = true, .marker = true});
		}
		return true;
	}

private:
	std::list<KeyedNode> _nodes;
	const Hooks& _hooks;
	uint32_t _nextKey {1};
};

/// Every anchor under the thing, but the path's ends, is hidden, and a planned node takes its place
void HideUnder(Path& path, const Obstacle& obstacle, const Box& box, float reachSq)
{
	if (path.Begin() == path.End())
	{
		return;
	}
	for (auto at = std::next(path.Begin()); at != path.End(); ++at)
	{
		if (!IsAnchor(*at) || std::next(at) == path.End() || !box.HoldsOrTouches(at->node.place))
		{
			continue;
		}
		const float dx = x87::Sub(static_cast<double>(static_cast<uint32_t>(at->node.place.x)), obstacle.place.x);
		const float dz = x87::Sub(static_cast<double>(static_cast<uint32_t>(at->node.place.y)), obstacle.place.y);
		if (dz * dz + dx * dx < reachSq)
		{
			at->node.hidden = true;
			at = path.InsertPlanned(std::next(at), at->node.place);
		}
	}
}

/// The nodes stepped round the thing's circle when the route planner can't plan the stretch: the line from the node
/// before to the node after crosses the circle at two points; nodes go round the circle from the first point, the side
/// the line passes the middle on, until near the second, then a node on the second point. The new nodes take the place
/// of every node between the two, hidden or a marker: those nodes are removed from the path.
void StepRound(Path& path, Iterator previous, Iterator next, glm::vec2 metresC, glm::vec2 nearest, glm::vec2 line,
               float overlapSq, float radius)
{
	std::vector<uint32_t> cutOff;
	for (auto at = std::next(previous); at != next;)
	{
		cutOff.push_back(at->key);
		at = path.Erase(at);
	}
	float ux = line.x;
	float uz = line.y;
	if (!ZeroOrUnordered(line.x) || !ZeroOrUnordered(line.y))
	{
		const float inverse = 1.0f / std::sqrt(line.x * line.x + line.y * line.y);
		ux = line.x * inverse;
		uz = line.y * inverse;
	}
	const float half = std::sqrt(overlapSq);
	const float wx = half * ux;
	const float wz = uz * half;
	const float v1x0 = (nearest.x - wx) - metresC.x;
	const float v1z0 = (nearest.y - wz) - metresC.y;
	const float v2x = (nearest.x + wx) - metresC.x;
	const float v2z = (wz + nearest.y) - metresC.y;
	const glm::ivec2 leave {map_coords::ToFixed(v2x + metresC.x), map_coords::ToFixed(v2z + metresC.y)};
	const float near = x87::Mul(radius, k_ArcEndShare);
	const float nearSq = near * near;
	const float side = v2x * v1z0 - v2z * v1x0;
	float v1x = v1x0;
	float v1z = v1z0;
	const float apartX = v1x - v2x;
	const float apartZ = v1z - v2z;
	const auto insertAt = next;
	if (apartZ * apartZ + apartX * apartX > nearSq)
	{
		const bool clockwise = side > 0.0f;
		float distanceSq = 0.0f;
		do
		{
			path.InsertPlanned(insertAt,
			                   glm::ivec2 {map_coords::ToFixed(metresC.x + v1x), map_coords::ToFixed(metresC.y + v1z)});
			const float cosX = x87::Mul(v1x, k_StepCosine);
			const float sinZ = x87::Mul(v1z, k_StepSine);
			const float cosZ = x87::Mul(v1z, k_StepCosine);
			const float sinX = x87::Mul(v1x, k_StepSine);
			v1x = clockwise ? cosX + sinZ : cosX - sinZ;
			v1z = clockwise ? cosZ - sinX : cosZ + sinX;
			const float ax = v1x - v2x;
			const float az = v1z - v2z;
			distanceSq = az * az + ax * ax;
		} while (distanceSq > nearSq);
	}
	const auto last = path.InsertPlanned(insertAt, leave);
	for (const auto key : cutOff)
	{
		path.Hook().cutOff(key, last->key, path.Begin()->key);
	}
}

/// Sends one stretch of the path round the thing: the first stretch between two shown nodes that comes near it
void SendStretchRound(Path& path, const Obstacle& obstacle, const Box& box, float reachSq)
{
	const auto& hooks = path.Hook();
	const double cx = static_cast<double>(static_cast<uint32_t>(obstacle.place.x));
	const float cz = x87::RoundToFloat(static_cast<double>(static_cast<uint32_t>(obstacle.place.y)));
	std::optional<Iterator> previous;
	bool haveAnchors = false;
	Iterator firstAnchor = path.End();
	Iterator lastAnchor = path.End();
	for (auto at = path.Begin(); at != path.End(); previous = at, at = path.NextShown(at))
	{
		if (!previous.has_value())
		{
			continue;
		}
		const auto before = *previous;
		if (!before->node.planned)
		{
			firstAnchor = before;
			lastAnchor = path.FirstAnchorFrom(at);
			haveAnchors = true;
		}
		const auto p = before->node.place;
		const auto n = at->node.place;
		if (!Overlaps(p.x, n.x, static_cast<int32_t>(box.xMin), static_cast<int32_t>(box.xMax)) ||
		    !Overlaps(p.y, n.y, static_cast<int32_t>(box.zMin), static_cast<int32_t>(box.zMax)))
		{
			continue;
		}
		// An end of the stretch under the thing: the stretch between its anchors is planned again round the thing
		const auto planAgainFrom = [&](const KeyedNode& end) {
			if (haveAnchors && end.node.planned)
			{
				if (path.PlanAgain(firstAnchor, lastAnchor, obstacle.place))
				{
					return true;
				}
				haveAnchors = false;
			}
			return false;
		};
		const float pdx = x87::Sub(static_cast<double>(static_cast<uint32_t>(p.x)), cx);
		const float pdz = x87::Sub(static_cast<double>(static_cast<uint32_t>(p.y)), cz);
		if (pdz * pdz + pdx * pdx <= reachSq)
		{
			if (planAgainFrom(*before))
			{
				return;
			}
			continue;
		}
		const float ndx = x87::Sub(static_cast<double>(n.x), cx);
		const float ndz = x87::Sub(static_cast<double>(static_cast<uint32_t>(n.y)), cz);
		if (ndz * ndz + ndx * ndx <= reachSq)
		{
			if (planAgainFrom(*at))
			{
				return;
			}
			continue;
		}
		// Both ends clear of it: how far into the circle the line comes, from the nearest point of the line to the middle
		const glm::vec2 metresP {map_coords::ToMetres(p.x), map_coords::ToMetres(p.y)};
		const glm::vec2 metresN {map_coords::ToMetres(n.x), map_coords::ToMetres(n.y)};
		const glm::vec2 metresC {map_coords::ToMetres(obstacle.place.x), map_coords::ToMetres(obstacle.place.y)};
		const float dx = metresN.x - metresP.x;
		const float dz = metresN.y - metresP.y;
		const float ax = metresC.x - metresP.x;
		const float az = metresC.y - metresP.y;
		const float along = (ax * dx + az * dz) / (dx * dx + dz * dz);
		const glm::vec2 nearest {along * dx + metresP.x, along * dz + metresP.y};
		const float ex = metresC.x - nearest.x;
		const float ez = metresC.y - nearest.y;
		const float overlapSq = obstacle.radius * obstacle.radius - (ex * ex + ez * ez);
		hooks.lookAgain(before->key, true);
		hooks.lookAgain(at->key, false);
		if (!x87::Greater(overlapSq, k_LeastOverlap))
		{
			continue;
		}
		if (haveAnchors && path.PlanAgain(firstAnchor, lastAnchor, obstacle.place))
		{
			return;
		}
		StepRound(path, before, at, metresC, nearest, {dx, dz}, overlapSq, obstacle.radius);
		return;
	}
}

/// Inside the circle that a thing removed changes the nodes back in: strictly inside the square, and nearer than
/// the reach by the game's distance
[[nodiscard]] bool InReturnReach(const KeyedNode& node, const Obstacle& obstacle, const Box& box, float reach)
{
	if (!box.Holds(node.node.place))
	{
		return false;
	}
	const map_coords::MapCoords at {.x = node.node.place.x, .z = node.node.place.y, .altitude = 0.0f};
	const map_coords::MapCoords centre {.x = obstacle.place.x, .z = obstacle.place.y, .altitude = 0.0f};
	return gutils::GetDistanceInMetres(at, centre) < reach;
}

[[nodiscard]] bool InReach(const KeyedNode& node, const Obstacle& obstacle, float reach)
{
	const map_coords::MapCoords at {.x = node.node.place.x, .z = node.node.place.y, .altitude = 0.0f};
	const map_coords::MapCoords centre {.x = obstacle.place.x, .z = obstacle.place.y, .altitude = 0.0f};
	return gutils::GetDistanceInMetres(at, centre) < reach;
}

/// A node as the bend test sees it: metres across the ground and the land's height
[[nodiscard]] glm::vec3 Point(const Hooks& hooks, glm::ivec2 place)
{
	return {map_coords::ToMetres(place.x), hooks.altitude(place) + 0.0f, map_coords::ToMetres(place.y)};
}

/// The anchors and walk state that the three passes of the way back share
struct ReturnState
{
	Iterator first;
	Iterator last;
	bool haveAnchors {false};
};

/// The hidden nodes under where the thing stood show again, and each stretch from one to the next is planned again.
/// Only the first stretch with such a node is done. Whether a node was shown again with more of the path after it.
[[nodiscard]] bool ShowHidden(Path& path, const Obstacle& obstacle, const Box& box, float reach, ReturnState& state)
{
	std::optional<Iterator> previous;
	for (auto at = path.Begin(); at != path.End(); previous = at, ++at)
	{
		if (!previous.has_value() || (*previous)->node.planned)
		{
			continue;
		}
		state.first = *previous;
		auto scan = at;
		bool changed = false;
		// Whether more of the path came after the last node shown again, even when that part has gone since
		bool more = false;
		while (scan != path.End() && !IsAnchor(*scan))
		{
			if (scan->node.hidden && InReturnReach(*scan, obstacle, box, reach) && state.first != scan)
			{
				scan->node.hidden = false;
				path.EraseBetween(state.first, scan);
				path.PlanAgain(state.first, scan, std::nullopt);
				state.first = scan;
				++scan;
				more = scan != path.End();
				changed = true;
				continue;
			}
			++scan;
		}
		state.last = scan;
		state.haveAnchors = true;
		if (changed)
		{
			path.EraseBetween(state.first, state.last);
			path.PlanAgain(state.first, state.last, std::nullopt);
			return more;
		}
	}
	return false;
}

/// The markers left where the thing stood go. The first one taken out with anchors known has its stretch planned again;
/// whether that found a way.
[[nodiscard]] bool RemoveMarkersAndPlan(Path& path, const Obstacle& obstacle, const Box& box, float reach, ReturnState& state)
{
	std::optional<Iterator> previous;
	for (auto at = path.Begin(); at != path.End();)
	{
		if (previous.has_value())
		{
			if (IsAnchor(**previous))
			{
				state.first = *previous;
				state.last = path.FirstAnchorFrom(at);
				state.haveAnchors = true;
			}
			if (at->node.marker && at->node.planned && InReturnReach(*at, obstacle, box, reach) && state.haveAnchors)
			{
				// The game reads the next node from the node it has just deleted; the next node is kept first here
				at = path.Erase(at);
				if (path.PlanAgain(state.first, state.last, std::nullopt))
				{
					return true;
				}
				continue;
			}
		}
		previous = at;
		++at;
	}
	return false;
}

/// Every marker left where the thing stood goes, with no planning
void RemoveMarkers(Path& path, const Obstacle& obstacle, const Box& box, float reach)
{
	bool havePrevious = false;
	for (auto at = path.Begin(); at != path.End();)
	{
		if (havePrevious && at->node.marker && at->node.planned && InReturnReach(*at, obstacle, box, reach))
		{
			at = path.Erase(at);
			continue;
		}
		havePrevious = true;
		++at;
	}
}

/// The planned nodes that bent round where the thing stood: the stretch between its anchors is planned again; when no
/// way is found, the bend is cut out where it leaves the path's line
void StraightenBends(Path& path, const Obstacle& obstacle, const Box& box, float reach, ReturnState& state)
{
	const auto& hooks = path.Hook();
	Iterator previous = path.End();
	for (auto at = path.Begin(); at != path.End();)
	{
		if (previous == path.End() || !at->node.planned || !InReturnReach(*at, obstacle, box, reach))
		{
			if (previous != path.End() && !previous->node.planned)
			{
				state.first = previous;
				state.last = path.FirstAnchorFrom(at);
				state.haveAnchors = true;
			}
			previous = at;
			at = path.NextShown(at);
			continue;
		}
		if (!previous->node.planned)
		{
			state.first = previous;
			state.last = path.FirstAnchorFrom(at);
			state.haveAnchors = true;
		}
		if (state.haveAnchors)
		{
			if (path.PlanAgain(state.first, state.last, std::nullopt))
			{
				return;
			}
			state.haveAnchors = false;
		}
		// The game looks at this node again for good when its anchors stand at one place, and hangs: the way back
		// stops here instead
		if (state.last == path.End() || state.last->node.place == state.first->node.place)
		{
			return;
		}
		// From this node on, the first node out of reach after the last run of nodes in reach, and the last node in
		// reach before it
		Iterator out = path.End();
		Iterator lastIn = path.End();
		Iterator last = path.End();
		const auto start = at;
		bool inside = true;
		while (inside || at->node.planned)
		{
			if (InReach(*at, obstacle, reach))
			{
				inside = true;
			}
			else if (inside)
			{
				inside = false;
				out = at;
				lastIn = last;
			}
			last = at;
			at = path.NextShown(at);
			if (at == path.End())
			{
				break;
			}
		}
		if (out == path.End())
		{
			continue;
		}
		auto cutFrom = start;
		if (!StraightEnough(Point(hooks, previous->node.place), Point(hooks, cutFrom->node.place),
		                    Point(hooks, out->node.place)))
		{
			previous = cutFrom;
			++cutFrom;
		}
		if (lastIn != path.End() && cutFrom != out &&
		    !StraightEnough(Point(hooks, previous->node.place), Point(hooks, lastIn->node.place),
		                    Point(hooks, out->node.place)))
		{
			out = lastIn;
		}
		if (cutFrom == out)
		{
			continue;
		}
		hooks.lookAgain(out->key, false);
		hooks.lookAgain(previous->key, true);
		path.EraseBetween(previous, out);
		return;
	}
}

} // namespace

float footpath_obstacles::HideReachSq(float radius)
{
	const auto half = static_cast<uint32_t>(gutils::ConvertMetersToWholeDistance(radius));
	const auto square = static_cast<int32_t>(half * half);
	return x87::Mul(static_cast<double>(square), k_HideReachScale);
}

float footpath_obstacles::ReturnReach(float radius)
{
	return radius * k_ReturnScale + k_ReturnExtra;
}

bool footpath_obstacles::StraightEnough(glm::vec3 first, glm::vec3 second, glm::vec3 corner)
{
	const float a0 = first.x - corner.x;
	const float a1 = first.y - corner.y;
	const float a2 = first.z - corner.z;
	const float b0 = second.x - corner.x;
	const float b1 = second.y - corner.y;
	const float b2 = second.z - corner.z;
	const float cross = b0 * a2 - b2 * a0;
	const float dot = (b2 * a2 + b0 * a0) + b1 * a1;
	// A result that is not a number counts as straight, as the game's compare sees it
	return !x87::GreaterEqual(cross / dot, k_StraightLimit);
}

bool footpath_obstacles::GoesClearOf(glm::ivec2 low, glm::ivec2 high, const Obstacle& obstacle)
{
	// The rules look only at the nodes in the square about the thing, and at the stretches that cross into it. When the
	// square does not wrap, the unsigned and the signed compares of the rules agree, and a path outside it is clear.
	const int64_t half = gutils::ConvertMetersToWholeDistance(obstacle.radius);
	const int64_t x = obstacle.place.x;
	const int64_t z = obstacle.place.y;
	constexpr int64_t k_Largest = std::numeric_limits<int32_t>::max();
	if (half < 0 || x - half < 0 || z - half < 0 || x + half > k_Largest || z + half > k_Largest)
	{
		return false;
	}
	return high.x < x - half || low.x > x + half || high.y < z - half || low.y > z + half;
}

footpath_obstacles::ReturnSquare::ReturnSquare(const Obstacle& obstacle)
{
	const auto box = Box::Around(obstacle.place, gutils::ConvertMetersToWholeDistance(ReturnReach(obstacle.radius)));
	_xMin = box.xMin;
	_xMax = box.xMax;
	_zMin = box.zMin;
	_zMax = box.zMax;
}

bool footpath_obstacles::ReturnSquare::Holds(glm::ivec2 place) const
{
	return Box {.xMin = _xMin, .xMax = _xMax, .zMin = _zMin, .zMax = _zMax}.Holds(place);
}

void footpath_obstacles::GoRound(std::vector<KeyedNode>& nodes, const Obstacle& obstacle, const Hooks& hooks)
{
	Path path(nodes, hooks);
	const auto box = Box::Around(obstacle.place, gutils::ConvertMetersToWholeDistance(obstacle.radius));
	const float reachSq = HideReachSq(obstacle.radius);
	HideUnder(path, obstacle, box, reachSq);
	SendStretchRound(path, obstacle, box, reachSq);
	nodes = path.Nodes();
}

void footpath_obstacles::StopGoingRound(std::vector<KeyedNode>& nodes, const Obstacle& obstacle, const Hooks& hooks)
{
	Path path(nodes, hooks);
	if (path.Begin() == path.End())
	{
		return;
	}
	const float reach = ReturnReach(obstacle.radius);
	const auto box = Box::Around(obstacle.place, gutils::ConvertMetersToWholeDistance(reach));
	ReturnState state {.first = path.End(), .last = path.End()};
	// Hidden nodes shown again, or a marker's stretch planned again: then only the markers go
	if (ShowHidden(path, obstacle, box, reach, state) || RemoveMarkersAndPlan(path, obstacle, box, reach, state))
	{
		RemoveMarkers(path, obstacle, box, reach);
	}
	else
	{
		StraightenBends(path, obstacle, box, reach, state);
	}
	nodes = path.Nodes();
}
