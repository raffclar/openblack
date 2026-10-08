/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Footpaths.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "3D/LandIslandInterface.h"
#include "Common/GUtilsDistance.h"
#include "Debug/StateHash.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Town.h"
#include "ECS/LivingFootpath.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/Systems/RoutePlanStateSystemInterface.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "Enums.h"
#include "Locator.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RoutePlan.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
using footpaths::k_NoNode;
using footpaths::NodeId;

/// Two spots closer than this squared distance (in metres) count as the same spot
constexpr double k_SameSpotSq = 0.0025000000000000005;

/// The node's distance in metres to `at`
float NodeDistance(const Footpath::Node& node, const map_coords::MapCoords& at)
{
	return gutils::GetDistanceInMetres(node.coords, at);
}

bool Hidden(const Footpath::Node& node)
{
	return (node.flags & footpaths::k_HiddenMask) != 0;
}

Footpath* FootpathOf(entt::entity footpath)
{
	auto& registry = Locator::entitiesRegistry::value();
	return footpath != entt::null && registry.Valid(footpath) ? registry.TryGet<Footpath>(footpath) : nullptr;
}

/// The node's place in the list (0 = the head), -1 for none
int32_t IndexOf(const Footpath& f, NodeId node)
{
	if (node == k_NoNode)
	{
		return -1;
	}
	const auto it = std::find_if(f.nodes.begin(), f.nodes.end(), [node](const auto& n) { return n.id == node; });
	return it != f.nodes.end() ? static_cast<int32_t>(it - f.nodes.begin()) : -1;
}

bool ValidIndex(const Footpath& f, int32_t index)
{
	return index >= 0 && static_cast<size_t>(index) < f.nodes.size();
}

NodeId IdAt(const Footpath& f, int32_t index)
{
	return ValidIndex(f, index) ? f.nodes.at(static_cast<size_t>(index)).id : k_NoNode;
}

/// The next non-hidden node's place in the list, -1 for none
int32_t NextIndex(const Footpath& f, int32_t index, int32_t direction)
{
	if (!ValidIndex(f, index))
	{
		return -1;
	}
	int32_t at = index;
	for (;;)
	{
		// direction != 0 walks toward the head (the head has none before it), 0 away from it
		at = direction != 0 ? at - 1 : at + 1;
		if (!ValidIndex(f, at))
		{
			return -1;
		}
		// hidden nodes are skipped
		if (!Hidden(f.nodes.at(static_cast<size_t>(at))))
		{
			return at;
		}
	}
}

/// The nearest non-hidden node's place in the list, or the one after it in `direction`
int32_t NearestIndex(const Footpath& f, const map_coords::MapCoords& from, int32_t direction)
{
	// the nearest non-hidden node from the head, strictly closer than 1e7
	float best = 10000000.0f;
	int32_t nearest = -1;
	for (size_t i = 0; i < f.nodes.size(); ++i)
	{
		const auto& node = f.nodes.at(i);
		if (Hidden(node))
		{
			continue;
		}
		const float d = NodeDistance(node, from);
		if (d < best)
		{
			best = d;
			nearest = static_cast<int32_t>(i);
		}
	}
	if (nearest == -1)
	{
		return -1;
	}
	// the next node in `direction` wins when it is farther from the nearest node than from `from`
	const int32_t next = NextIndex(f, nearest, direction);
	if (next == -1)
	{
		return nearest;
	}
	const auto& nextNode = f.nodes.at(static_cast<size_t>(next));
	const float toFrom = NodeDistance(nextNode, from);
	const float toNearest = NodeDistance(nextNode, f.nodes.at(static_cast<size_t>(nearest)).coords);
	return toNearest > toFrom ? next : nearest;
}

Footpath::Node* NodeOf(entt::entity footpath, NodeId node)
{
	auto* f = FootpathOf(footpath);
	const int32_t index = f != nullptr ? IndexOf(*f, node) : -1;
	return index != -1 ? &f->nodes.at(static_cast<size_t>(index)) : nullptr;
}

/// Destroys a link; the footpaths it listed stay
void DeleteLink(entt::entity link)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (link != entt::null && registry.Valid(link))
	{
		registry.Destroy(link);
	}
}
} // namespace

NodeId footpaths::GetNextNode(entt::entity footpath, NodeId node, int32_t direction)
{
	const auto* f = FootpathOf(footpath);
	return f != nullptr ? IdAt(*f, NextIndex(*f, IndexOf(*f, node), direction)) : k_NoNode;
}

NodeId footpaths::GetNearestPos(entt::entity footpath, const map_coords::MapCoords& from, int32_t direction)
{
	const auto* f = FootpathOf(footpath);
	return f != nullptr ? IdAt(*f, NearestIndex(*f, from, direction)) : k_NoNode;
}

std::optional<map_coords::MapCoords> footpaths::GetEndNonHiddenNode(entt::entity footpath, int32_t direction)
{
	const auto* f = FootpathOf(footpath);
	if (f == nullptr)
	{
		return std::nullopt;
	}
	// direction != 0: the first non-hidden node from the head; direction == 0: the last one
	std::optional<map_coords::MapCoords> found;
	for (const auto& node : f->nodes)
	{
		if (Hidden(node))
		{
			continue;
		}
		found = node.coords;
		if (direction != 0)
		{
			break;
		}
	}
	return found;
}

map_coords::MapCoords footpaths::NodeCoords(entt::entity footpath, NodeId node)
{
	const auto* n = NodeOf(footpath, node);
	return n != nullptr ? n->coords : map_coords::MapCoords {};
}

NodeId footpaths::NearestNodeBelow(entt::entity footpath, const map_coords::MapCoords& from, float& best)
{
	const auto* f = FootpathOf(footpath);
	if (f == nullptr)
	{
		return k_NoNode;
	}
	// each non-hidden node from the head strictly below `best` is kept, and `best` becomes its distance
	NodeId nearest = k_NoNode;
	for (const auto& node : f->nodes)
	{
		if (Hidden(node))
		{
			continue;
		}
		const float d = NodeDistance(node, from);
		if (d < best)
		{
			best = d;
			nearest = node.id;
		}
	}
	return nearest;
}

int32_t footpaths::NextNodeNear(entt::entity footpath, const map_coords::MapCoords& pos, NodeId& node,
                                map_coords::MapCoords& out, int32_t direction, float maxDistance)
{
	const auto* f = FootpathOf(footpath);
	if (f == nullptr)
	{
		return 0;
	}
	// no nearest node -> 0
	const int32_t nearest = NearestIndex(*f, pos, direction);
	if (nearest == -1)
	{
		return 0;
	}
	// the nearest node farther than maxDistance -> 0
	if (NodeDistance(f->nodes.at(static_cast<size_t>(nearest)), pos) > maxDistance)
	{
		return 0;
	}
	// no next node -> the end of the footpath
	const int32_t next = NextIndex(*f, nearest, direction);
	if (next == -1)
	{
		return k_EndOfFootpath;
	}
	// the next node's coords and id
	out = f->nodes.at(static_cast<size_t>(next)).coords;
	node = f->nodes.at(static_cast<size_t>(next)).id;
	return 1;
}

int32_t footpaths::StepNextNode(entt::entity footpath, NodeId& node, map_coords::MapCoords& out, int32_t direction)
{
	const auto* f = FootpathOf(footpath);
	// no next node -> the end of the footpath
	const int32_t next = f != nullptr ? NextIndex(*f, IndexOf(*f, node), direction) : -1;
	if (next == -1)
	{
		return k_EndOfFootpath;
	}
	// the next node's coords and id
	out = f->nodes.at(static_cast<size_t>(next)).coords;
	node = f->nodes.at(static_cast<size_t>(next)).id;
	return 1;
}

NodeId footpaths::HeadNode(entt::entity footpath)
{
	const auto* f = FootpathOf(footpath);
	return f != nullptr && !f->nodes.empty() ? f->nodes.front().id : k_NoNode;
}

NodeId footpaths::TailNode(entt::entity footpath)
{
	const auto* f = FootpathOf(footpath);
	return f != nullptr && !f->nodes.empty() ? f->nodes.back().id : k_NoNode;
}

void footpaths::AddOccupant(entt::entity footpath, NodeId node, entt::entity living)
{
	if (auto* n = NodeOf(footpath, node); n != nullptr)
	{
		n->occupants.insert(n->occupants.begin(), living);
	}
}

void footpaths::RemoveOccupant(entt::entity footpath, NodeId node, entt::entity living)
{
	if (auto* n = NodeOf(footpath, node); n != nullptr)
	{
		if (const auto it = std::find(n->occupants.begin(), n->occupants.end(), living); it != n->occupants.end())
		{
			n->occupants.erase(it);
		}
	}
}

std::optional<footpaths::PathChoice> footpaths::GetNearestPathTo(entt::entity link, const map_coords::MapCoords& from,
                                                                 const map_coords::MapCoords& to, float& best)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* l = link != entt::null && registry.Valid(link) ? registry.TryGet<const FootpathLink>(link) : nullptr;
	if (l == nullptr)
	{
		return std::nullopt;
	}
	std::optional<PathChoice> result;
	// the link's footpaths, from its list head
	for (const auto id : l->footpaths)
	{
		const auto footpath = static_cast<entt::entity>(id);
		const auto* f = FootpathOf(footpath);
		// only active footpaths with nodes
		if (f == nullptr || !f->active || f->nodes.empty())
		{
			continue;
		}
		const float toHead = NodeDistance(f->nodes.front(), to);
		const float toTail = NodeDistance(f->nodes.back(), to);
		// toward the head (1) when the tail is farther from `to`, else away from it (0)
		const int32_t direction = toTail > toHead ? 1 : 0;
		const int32_t node = NearestIndex(*f, from, direction);
		if (node == -1)
		{
			continue;
		}
		// a node strictly closer than `best` is kept
		const float d = NodeDistance(f->nodes.at(static_cast<size_t>(node)), from);
		if (d < best)
		{
			best = d;
			result = PathChoice {footpath, IdAt(*f, node), direction};
		}
	}
	return result;
}

std::optional<footpaths::PathChoice> footpaths::GetNearestPathToQuick(entt::entity link, const map_coords::MapCoords& from,
                                                                      const map_coords::MapCoords& to, float& best)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* l = link != entt::null && registry.Valid(link) ? registry.TryGet<const FootpathLink>(link) : nullptr;
	if (l == nullptr)
	{
		return std::nullopt;
	}
	std::optional<PathChoice> result;
	for (const auto id : l->footpaths)
	{
		const auto footpath = static_cast<entt::entity>(id);
		const auto* f = FootpathOf(footpath);
		if (f == nullptr || !f->active || f->nodes.empty())
		{
			continue;
		}
		// the ends' squared distances to `to`: the entry is the tail when it is farther than the head, else the head
		const float headSq = gutils::GetMetresDistanceSq(f->nodes.front().coords, to);
		const float tailSq = gutils::GetMetresDistanceSq(f->nodes.back().coords, to);
		const bool towardHead = tailSq > headSq;
		const auto& entry = towardHead ? f->nodes.back() : f->nodes.front();
		// an entry strictly closer than `best` is kept
		const float d = NodeDistance(entry, from);
		if (d < best)
		{
			best = d;
			result = PathChoice {footpath, entry.id, towardHead ? 1 : 0};
		}
	}
	return result;
}

entt::entity footpaths::GetFootpathLink(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* of = thing != entt::null && registry.Valid(thing) ? registry.TryGet<const FootpathLinkOf>(thing) : nullptr;
	return of != nullptr ? of->link : entt::null;
}

void footpaths::AttachLoadedLink(entt::entity link, const map_coords::MapCoords& coords)
{
	auto& registry = Locator::entitiesRegistry::value();
	// no link -> nothing
	if (link == entt::null || !registry.Valid(link))
	{
		return;
	}
	// a multi-map fixed object in the coords' cell, at the same spot, takes the link
	const auto cell = map_coords::Cell(coords);
	for (auto obj = map_cells::FindType(cell, ObjectType::Any); obj != entt::null;
	     obj = map_cells::FindType(cell, ObjectType::Any, obj))
	{
		const float d = gutils::GetMetresDistanceSq(object::MapCoordsOf(obj), coords);
		if (static_cast<double>(d) < k_SameSpotSq && map_cells::IsMultiCellStaticClass(obj))
		{
			// its old link is deleted
			DeleteLink(GetFootpathLink(obj));
			registry.AssignOrReplace<FootpathLinkOf>(obj, link);
			return;
		}
	}
	// else a planned abode at the same spot, the towns newest first
	for (const auto town : town_queries::TownsNewestFirst())
	{
		for (auto& plan : registry.Get<Town>(town).plannedAbodes)
		{
			// (approximate) the plan's map coords from its world position
			const float d = gutils::GetMetresDistanceSq(map_coords::FromWorld(plan.position), coords);
			if (static_cast<double>(d) < k_SameSpotSq)
			{
				// its old link is deleted
				DeleteLink(plan.footpathLink);
				plan.footpathLink = link;
				return;
			}
		}
	}
	// nothing at the spot -> the link is deleted
	DeleteLink(link);
}

uint32_t footpaths::UseFootpathIfNecessary(entt::entity link, entt::entity living, const map_coords::MapCoords& pos,
                                           uint8_t final, entt::entity owner)
{
	// the Living's position
	const auto from = living_footpath::PosOf(living);
	// the nearest footpath of this link within 40 metres
	float best = 40.0f;
	if (const auto choice = GetNearestPathTo(link, from, pos, best); choice.has_value())
	{
		// a move onto it that starts -> 1
		if (living_footpath::SetupMoveOnFootpath(living, choice->footpath, choice->direction, final, choice->node) != 0)
		{
			return 1;
		}
	}
	// else the owner's town (an abode's; (pending) a forest's or a dance's), or the nearest town to pos. (openblack,
	// guard) registry.Valid(owner): the original reads the owner's town with no null check
	auto& registry = Locator::entitiesRegistry::value();
	auto town = registry.Valid(owner) && registry.AllOf<Abode>(owner) ? abode_villagers::TownOf(owner) : entt::null;
	if (town == entt::null)
	{
		town = map_cells::GetNearestTown(pos, std::numeric_limits<float>::max());
	}
	if (town != entt::null)
	{
		// the town's storage pit; none, or the owner itself -> the hug
		const auto pit = town_queries::GetStoragePit(town);
		if (pit != entt::null && pit != owner)
		{
			// the path out: this link's footpath with an entry within 10 metres of the pit's arrive pos
			const auto pitArrive = living_footpath::ArrivePosOf(pit, living);
			float outBest = 10.0f;
			const auto out = GetNearestPathToQuick(link, pos, pitArrive, outBest);
			// the pit's own link
			const auto pitLink = GetFootpathLink(pit);
			if (out.has_value() && pitLink != entt::null)
			{
				// the path in: the pit link's footpath with an entry within 40 metres of the Living
				float inBest = 40.0f;
				const auto in = GetNearestPathToQuick(pitLink, from, living_footpath::ArrivePosOf(pit, living), inBest);
				if (in.has_value())
				{
					// the move onto the path in (its result not read), then the path out queued as the next footpath
					static_cast<void>(
					    living_footpath::SetupMoveOnFootpath(living, in->footpath, in->direction, final, in->node));
					living_footpath::SetNextFootpath(living, out->footpath);
					return 1;
				}
			}
		}
	}
	// else the hug toward pos
	return living_footpath::SetupMoveToWithHug(living, pos, final);
}

namespace
{
/// The footpaths in list order: the newest (highest creationStamp) first
std::vector<entt::entity> FootpathsFromHead()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<uint32_t, entt::entity>> all;
	registry.Each<const Footpath>([&all](entt::entity e, const Footpath& f) { all.emplace_back(f.creationStamp, e); });
	// stable: the footpaths with the same stamp (0: made without Create, e.g. by hand in tests) keep the registry's order
	std::stable_sort(all.begin(), all.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<entt::entity> out;
	out.reserve(all.size());
	for (const auto& [stamp, e] : all)
	{
		out.push_back(e);
	}
	return out;
}
} // namespace

entt::entity footpaths::Create()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// no nodes, active
	auto& f = registry.Assign<Footpath>(entity);
	// the newest footpath goes at the head of the list
	f.creationStamp = ++registry.Context().footpathCreationCounter;
	return entity;
}

int32_t footpaths::PositionFromHead(entt::entity footpath)
{
	// the place in the list from the head; not found -> -1
	const auto list = FootpathsFromHead();
	const auto it = std::find(list.begin(), list.end(), footpath);
	return it == list.end() ? -1 : static_cast<int32_t>(it - list.begin());
}

entt::entity footpaths::AtPositionFromHead(int32_t position)
{
	const auto list = FootpathsFromHead();
	return position >= 0 && static_cast<size_t>(position) < list.size() ? list.at(static_cast<size_t>(position)) : entt::null;
}

namespace
{
/// Each route planning call needs a large ObstacleGrid; two are live at once (GenerateRoutesBetween's and PushOutOfThing's),
/// so a small stack of reusable ones, each Emptied when taken
class HolderLease
{
public:
	HolderLease()
	{
		auto& pool = Pool();
		if (pool.free.empty())
		{
			pool.all.push_back(std::make_unique<route_planner::ObstacleGrid>());
			pool.free.push_back(pool.all.back().get());
		}
		_holder = pool.free.back();
		pool.free.pop_back();
		_holder->Empty();
		_holder->SetPlan(nullptr);
	}
	~HolderLease() { Pool().free.push_back(_holder); }
	HolderLease(const HolderLease&) = delete;
	HolderLease& operator=(const HolderLease&) = delete;
	route_planner::ObstacleGrid& Get() { return *_holder; }

private:
	/// The pool (Locator::routePlanStateSystem)
	static systems::RoutePlanStateSystemInterface::HolderPool& Pool()
	{
		if (!Locator::routePlanStateSystem::has_value())
		{
			std::fputs("ecs::footpaths: no route plan state in the locator (Locator::routePlanStateSystem)\n", stderr);
			std::abort();
		}
		return Locator::routePlanStateSystem::value().GetHolderPool();
	}
	route_planner::ObstacleGrid* _holder;
};

constexpr int32_t k_MaxPlanSteps = 128; ///< Planner turns at most while searching
constexpr float k_PlanRadius = 0.5f;    ///< The start's radius
constexpr float k_RouteLengthFactor = 5.0f;
constexpr float k_CosStep = 0.951075f;   ///< About cos 18 degrees
constexpr float k_SinStep = 0.308961f;   ///< About sin 18 degrees
constexpr double k_ArcStepFactor = 0.35; ///< The arc stops within 0.35 r of its end
constexpr float k_PushOutStep = 0.01f;   ///< The nudge off an object's own point
constexpr float k_PushOutMargin = 0.1f;  ///< Added to each circle's radius when pushing out

/// PushOutOfThing's state: the origin O and the point p being pushed
struct PushOut
{
	route_planner::Point2D origin;
	route_planner::Point2D* point;
};

/// p, inside the circle (c, r + 0.1), moved out along the ray from O
void PushOutOfCircle(void* context, entt::entity /*object*/, const route_planner::Point2D& c, float r, int32_t /*notify*/)
{
	auto& push = *static_cast<PushOut*>(context);
	auto& p = *push.point;
	const float bigR = r + k_PushOutMargin;
	const route_planner::Point2D d {p.x - push.origin.x, p.z - push.origin.z};
	const float fx = c.x - push.origin.x;
	const float fz = c.z - push.origin.z;
	const float b = (fx * d.x + fz * d.z) * -2.0f;
	const float a = d.x * d.x + d.z * d.z;
	const float disc = b * b - (((fz * fz + fx * fx) - bigR * bigR) * a) * 4.0f;
	if (disc < 0.0f)
	{
		return;
	}
	const float t = (std::sqrt(disc) - b) / (a + a);
	if (t < 1.0f)
	{
		return;
	}
	const float zt = d.z * t;
	p.x = d.x * t + push.origin.x;
	p.z = zt + push.origin.z;
}

/// p pushed out of the thing's own circles (a holder of its own, margin 0.5)
void PushOutOfThing(route_planner::Point2D& p, entt::entity thing, const route_planner::Point2D& other)
{
	HolderLease lease;
	auto& holder = lease.Get();
	holder.SetMargin(0.5f);
	PushOut push {route_plan_world::ToPoint2D(object::MapCoordsOf(thing)), &p};
	// exactly on the thing's point -> 0.01 toward the other end
	if (p.x == push.origin.x && p.z == push.origin.z)
	{
		float dx = other.x - p.x;
		float dz = other.z - p.z;
		if (!(dx == 0.0f && dz == 0.0f))
		{
			const float f = 1.0f / std::sqrt(dz * dz + dx * dx);
			dx = f * dx;
			dz = dz * f;
		}
		const float stepZ = dz * k_PushOutStep;
		p.x = dx * k_PushOutStep + p.x;
		p.z = stepZ + p.z;
	}
	route_plan_world::AddToRoutePlan(thing, holder, 0, &PushOutOfCircle, &push);
}

glm::vec3 WorldPointOf(const map_coords::MapCoords& coords)
{
	// (openblack) the node's draw point, as FotFile makes it: the land's height under it
	glm::vec3 position(map_coords::ToMetres(coords.x), coords.altitude, map_coords::ToMetres(coords.z));
	if (Locator::terrainSystem::has_value())
	{
		position.y += Locator::terrainSystem::value().GetHeightAt({position.x, position.z});
	}
	return position;
}

/// A new node at coords (flag bits 0 and 1 from p and q), with the footpath's next id
Footpath::Node MakeNode(Footpath& f, const map_coords::MapCoords& coords, uint8_t p, uint8_t q)
{
	const auto nodePosition = WorldPointOf(coords);
	Footpath::Node node {
	    .position = nodePosition,
	    .coords = coords,
	    .flags = static_cast<uint8_t>((p & 1u) | ((q & 1u) << 1u)),
	    .id = f.nextNodeId++,
	};
	return node;
}

/// The planner run the footpaths' way: start radius 0.5, the route at most 5 x (range + 1) long, at most 128 turns
/// while searching
bool RunPlan(route_planner::RoutePlan& plan, route_planner::ObstacleGrid& holder, const route_planner::Point2D& start,
             const route_planner::Point2D& dest)
{
	plan.SetStart(start, k_PlanRadius, holder, -1, -1, 0);
	plan.SetDest(dest, 0.0f, 0.0f, 0.0f, -1, 0, (start.DistanceTo(dest) + 1.0f) * k_RouteLengthFactor, true);
	for (int32_t i = 0; i < k_MaxPlanSteps && plan.GetState() == route_planner::RoutePlan::State::Searching; ++i)
	{
		plan.StepSearch(0);
	}
	return plan.GetState() == route_planner::RoutePlan::State::Found && plan.GetBestRoute() != route_planner::k_None;
}

/// The occupants whose Living no longer follows this node are removed; the rest are returned
std::vector<entt::entity> PurgeFollowerList(entt::entity footpath, NodeId node)
{
	auto* n = NodeOf(footpath, node);
	if (n == nullptr)
	{
		return {};
	}
	std::vector<entt::entity> stale;
	for (const auto living : n->occupants)
	{
		if (!living_footpath::IsFollowingNode(living, footpath, node))
		{
			stale.push_back(living);
		}
	}
	for (const auto living : stale)
	{
		footpaths::RemoveOccupant(footpath, node, living);
	}
	n = NodeOf(footpath, node);
	return n->occupants;
}

/// After the purge, the followers going away from the head re-plan their hug (their entries stay)
void ClearFromPreviousNode(entt::entity footpath, NodeId node)
{
	for (const auto living : PurgeFollowerList(footpath, node))
	{
		if (!living_footpath::GoesTowardHead(living))
		{
			living_footpath::RePlanHug(living);
		}
	}
}

/// After the purge, the followers going toward the head re-plan their hug
void ClearFromNextNode(entt::entity footpath, NodeId node)
{
	for (const auto living : PurgeFollowerList(footpath, node))
	{
		if (living_footpath::GoesTowardHead(living))
		{
			living_footpath::RePlanHug(living);
		}
	}
}

/// The node's followers move onto a or b by their way (MoveToNode puts them in that node's occupants); its own
/// entries go
void ClearForDeletion(entt::entity footpath, NodeId node, NodeId a, NodeId b)
{
	for (const auto living : PurgeFollowerList(footpath, node))
	{
		living_footpath::MoveToNode(living, footpath, living_footpath::GoesTowardHead(living) ? a : b);
	}
	if (auto* n = NodeOf(footpath, node); n != nullptr)
	{
		n->occupants.clear();
	}
}
} // namespace

void footpaths::ConvertCreaturePlanToFootpath(const route_planner::ObstacleGrid& holder, const route_planner::RoutePlan& plan,
                                              entt::entity footpath, NodeId a, NodeId b, const map_coords::MapCoords* c)
{
	auto* f = FootpathOf(footpath);
	if (f == nullptr)
	{
		return;
	}
	const int32_t ia = IndexOf(*f, a);
	const int32_t ib = IndexOf(*f, b);
	if (ia == -1 || ib == -1 || ib < ia)
	{
		return;
	}
	// b's followers going away from the head re-plan
	ClearFromPreviousNode(footpath, b);
	// a's followers going toward the head re-plan
	ClearFromNextNode(footpath, a);
	// each node between a and b is cleared for deletion; the hidden ones are kept, in order
	f = FootpathOf(footpath);
	std::vector<NodeId> between;
	for (int32_t i = IndexOf(*f, a) + 1; i < IndexOf(*f, b); ++i)
	{
		between.push_back(f->nodes.at(static_cast<size_t>(i)).id);
	}
	for (const auto node : between)
	{
		ClearForDeletion(footpath, node, a, b);
	}
	f = FootpathOf(footpath);
	const int32_t ia2 = IndexOf(*f, a);
	const int32_t ib2 = IndexOf(*f, b);
	std::vector<Footpath::Node> kept;
	for (int32_t i = ia2 + 1; i < ib2; ++i)
	{
		auto& node = f->nodes.at(static_cast<size_t>(i));
		if ((node.flags & k_HiddenMask) != 0)
		{
			kept.push_back(std::move(node));
		}
	}
	std::vector<Footpath::Node> made;
	const auto add = [&](const route_planner::Point2D& p) {
		made.push_back(MakeNode(*f, route_plan_world::ToMapCoords(p), 1, 1));
	};
	// the plan's start
	add(plan.GetStart());
	const auto& route = plan.GetRoute(plan.GetBestRoute());
	route_planner::NodeIndex prev = route_planner::k_None;
	for (auto at = route.first; at != route_planner::k_None; prev = at, at = plan.GetNode(at).child)
	{
		const auto& n = plan.GetNode(at);
		if (n.arc == 1 && prev != route_planner::k_None && plan.GetNode(prev).obj != -1)
		{
			// the arc round prev's object in 18-degree steps while more than 0.35 r from the end
			const auto& o = holder.Object(plan.GetNode(prev).obj);
			route_planner::Point2D u {n.from.x - o.c.x, n.from.z - o.c.z};
			route_planner::Point2D w {n.to.x - o.c.x, n.to.z - o.c.z};
			if (!(w.x == 0.0f && w.z == 0.0f))
			{
				const float inv = 1.0f / std::sqrt(w.z * w.z + w.x * w.x);
				w.x = inv * w.x;
				w.z = w.z * inv;
			}
			const float length = u.GetLength();
			w.x = length * w.x;
			w.z = w.z * length;
			// (r x 0.35)^2, each product rounded to float
			const auto r35 = static_cast<float>(static_cast<double>(o.r) * k_ArcStepFactor);
			const float limit = r35 * r35;
			const bool side1 = plan.GetNode(prev).side == 1;
			const auto farFromEnd = [&]() {
				const float dx = u.x - w.x;
				const float dz = u.z - w.z;
				const float dsq = dz * dz + dx * dx;
				// `dsq > limit`: a NaN stops the arc, as the original's compare does
				return dsq > limit;
			};
			while (farFromEnd())
			{
				add({o.c.x + u.x, o.c.z + u.z});
				const float x = side1 ? u.z * k_SinStep + u.x * k_CosStep : u.x * k_CosStep - u.z * k_SinStep;
				const float z = side1 ? u.z * k_CosStep - u.x * k_SinStep : u.z * k_CosStep + u.x * k_SinStep;
				u = {x, z};
			}
			add({w.x + o.c.x, o.c.z + w.z});
		}
		else if (n.child != route_planner::k_None)
		{
			// the node's end point, but not the last node's
			add(n.to);
		}
	}
	// the dest, then c (hidden, bit 3) when given, then the kept nodes and b
	add(plan.GetDest());
	if (c != nullptr)
	{
		auto node = MakeNode(*f, *c, 1, 1);
		node.flags = static_cast<uint8_t>(node.flags | 8u);
		node.position = WorldPointOf(*c);
		made.push_back(node);
	}
	std::vector<Footpath::Node> nodes;
	nodes.reserve(f->nodes.size() + made.size());
	for (int32_t i = 0; i <= ia2; ++i)
	{
		nodes.push_back(std::move(f->nodes.at(static_cast<size_t>(i))));
	}
	for (auto& node : made)
	{
		nodes.push_back(std::move(node));
	}
	for (auto& node : kept)
	{
		nodes.push_back(std::move(node));
	}
	for (size_t i = static_cast<size_t>(ib2); i < f->nodes.size(); ++i)
	{
		nodes.push_back(std::move(f->nodes.at(i)));
	}
	f->nodes = std::move(nodes);
}

entt::entity footpaths::GenerateRoutesBetween(entt::entity a, entt::entity b)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a new footpath with a's arrive pos, then b's: the head is b's
	const auto footpath = Create(); // the newest footpath, at the head of the list
	auto& f = registry.Get<Footpath>(footpath);
	const auto arriveA = living_footpath::ArrivePosOf(a);
	const auto arriveB = living_footpath::ArrivePosOf(b);
	f.nodes.insert(f.nodes.begin(), MakeNode(f, arriveA, 1, 0));
	f.nodes.insert(f.nodes.begin(), MakeNode(f, arriveB, 1, 0));
	const NodeId head = f.nodes.front().id;
	const NodeId tail = f.nodes.back().id;
	auto start = route_plan_world::ToPoint2D(f.nodes.front().coords);
	auto dest = route_plan_world::ToPoint2D(f.nodes.back().coords);
	// each end pushed out of its own multi-map fixed object toward the other
	if (map_cells::IsMultiCellStaticClass(b))
	{
		PushOutOfThing(start, b, dest);
	}
	if (map_cells::IsMultiCellStaticClass(a))
	{
		PushOutOfThing(dest, a, start);
	}
	HolderLease lease;
	route_planner::RoutePlan plan;
	if (!RunPlan(plan, lease.Get(), start, dest))
	{
		// no route: the footpath is deleted
		registry.Destroy(footpath);
		return entt::null;
	}
	ConvertCreaturePlanToFootpath(lease.Get(), plan, footpath, head, tail, nullptr);
	// every node not hidden loses bit 1
	for (auto& node : registry.Get<Footpath>(footpath).nodes)
	{
		if ((node.flags & k_HiddenMask) == 0)
		{
			node.flags = static_cast<uint8_t>(node.flags & 0xFDu);
		}
	}
	AddFootpath(a, footpath);
	AddFootpath(b, footpath);
	return footpath;
}

entt::entity footpaths::MakeAbodeFootpath(entt::entity abode, entt::entity pit)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a new footpath with the abode's arrive pos, then the pit's
	const auto footpath = Create(); // the newest footpath, at the head of the list
	auto& f = registry.Get<Footpath>(footpath);
	f.nodes.insert(f.nodes.begin(), MakeNode(f, living_footpath::ArrivePosOf(abode), 1, 0));
	f.nodes.insert(f.nodes.begin(), MakeNode(f, living_footpath::ArrivePosOf(pit), 1, 0));
	const NodeId head = f.nodes.front().id;
	const NodeId tail = f.nodes.back().id;
	// a route from the head to the last node; none -> the footpath is deleted
	if (!AttemptRerender(footpath, head, tail, nullptr))
	{
		registry.Destroy(footpath);
		return entt::null;
	}
	// the footpath added to the abode's link, then to the pit's
	AddFootpath(abode, footpath);
	AddFootpath(pit, footpath);
	return footpath;
}

bool footpaths::AttemptRerender(entt::entity footpath, NodeId a, NodeId b, const map_coords::MapCoords* c)
{
	const auto* f = FootpathOf(footpath);
	if (f == nullptr || IndexOf(*f, a) == -1 || IndexOf(*f, b) == -1)
	{
		return false;
	}
	const auto start = route_plan_world::ToPoint2D(NodeCoords(footpath, a));
	const auto dest = route_plan_world::ToPoint2D(NodeCoords(footpath, b));
	HolderLease lease;
	route_planner::RoutePlan plan;
	if (!RunPlan(plan, lease.Get(), start, dest))
	{
		return false;
	}
	ConvertCreaturePlanToFootpath(lease.Get(), plan, footpath, a, b, c);
	return true;
}

void footpaths::AddFootpath(entt::entity thing, entt::entity footpath)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the link is made on first use
	auto link = GetFootpathLink(thing);
	if (link == entt::null)
	{
		link = registry.Create();
		// (openblack) FootpathLink's position (the .fot loader's look-up key) has no counterpart in the original's link
		const auto at = object::MapCoordsOf(thing);
		registry.Assign<FootpathLink>(link, glm::vec3(map_coords::ToMetres(at.x), 0.0f, map_coords::ToMetres(at.z)),
		                              std::vector<Footpath::Id> {});
		registry.AssignOrReplace<FootpathLinkOf>(thing, link);
	}
	// the newest footpath at the head of the link's list
	auto& footpaths = registry.Get<FootpathLink>(link).footpaths;
	footpaths.insert(footpaths.begin(), static_cast<Footpath::Id>(footpath));
}

void footpaths::RegisterStateHash()
{
	// the live footpaths, read each turn (MakeFunctional makes new ones during play)
	state_hash::Register("footpaths", [](state_hash::Hasher& h) {
		auto& registry = Locator::entitiesRegistry::value();
		// the list's order (the newest first), each footpath named by its creation stamp
		for (const auto entity : FootpathsFromHead())
		{
			const auto& f = registry.Get<const Footpath>(entity);
			h.U32(f.creationStamp);
			// the node count: two lists that only differ in how the nodes split between footpaths hash apart
			h.U32(static_cast<uint32_t>(f.nodes.size()));
			for (const auto& node : f.nodes)
			{
				h.U32(static_cast<uint32_t>(node.coords.x));
				h.U32(static_cast<uint32_t>(node.coords.z));
				h.U32(node.flags);
			}
		}
	});
}

// ---- The footpaths round an obstacle -------------------------------------------------------------------------------

namespace
{
constexpr float k_AroundSlack = 1.01f;         ///< RerouteFootpathsAroundObstacle's R2 = R x R x 1.01
constexpr double k_AroundCos = 0.951074828;    ///< About cos 18 degrees: the arc's step
constexpr double k_AroundSin = 0.308960629;    ///< About sin 18 degrees
constexpr double k_DetourMin = 0.0001;         ///< h above it makes a detour
constexpr double k_AroundArcFactor = 0.35;     ///< The arc stops within 0.35 r of the exit
constexpr float k_ToMetres = 10.0f / 65536.0f; ///< Map units to metres
constexpr float k_ToFixed = 6553.6f;           ///< Metres to map units
constexpr float k_StopScale = 1.03f;           ///< StopReroutingAroundObstacle's L = 1.03 r + 0.4
constexpr float k_StopAdd = 0.4f;
constexpr double k_AngleTanMax = 0.01; ///< SmallTurn's limit on the turn's tangent

/// min(a, b) < hi && lo < max(a, b), signed and strict
bool SpanMeets(int32_t a, int32_t b, int32_t lo, int32_t hi)
{
	const int32_t low = a < b ? a : b;
	const int32_t high = a < b ? b : a;
	return low < hi && lo < high;
}

/// The first node from n (n included) with none of the flag bits 1-3; k_NoNode for none
NodeId FirstClean(const Footpath& f, NodeId from)
{
	for (int32_t i = IndexOf(f, from); i >= 0 && ValidIndex(f, i); ++i)
	{
		if ((f.nodes.at(static_cast<size_t>(i)).flags & 0xEu) == 0)
		{
			return f.nodes.at(static_cast<size_t>(i)).id;
		}
	}
	return k_NoNode;
}

/// An unsigned coordinate minus a signed one, rounded once to float
float Diff(uint32_t a, int32_t b)
{
	return static_cast<float>(static_cast<double>(a) - static_cast<double>(b));
}

/// Every node strictly between a and b is cleared for deletion; the hidden ones (bits 2 and 3) are kept in their
/// order, the rest are deleted. b == k_NoNode: up to the tail
void EraseNodesBetween(entt::entity footpath, NodeId a, NodeId b)
{
	auto* f = FootpathOf(footpath);
	if (f == nullptr)
	{
		return;
	}
	const int32_t ia = IndexOf(*f, a);
	const int32_t ib = b == k_NoNode ? static_cast<int32_t>(f->nodes.size()) : IndexOf(*f, b);
	if (ia < 0 || ib < 0 || ib <= ia)
	{
		return;
	}
	std::vector<NodeId> between;
	for (int32_t i = ia + 1; i < ib; ++i)
	{
		between.push_back(f->nodes.at(static_cast<size_t>(i)).id);
	}
	for (const auto node : between)
	{
		ClearForDeletion(footpath, node, a, b);
	}
	f = FootpathOf(footpath);
	for (const auto node : between)
	{
		const int32_t i = IndexOf(*f, node);
		if (i >= 0 && (f->nodes.at(static_cast<size_t>(i)).flags & footpaths::k_HiddenMask) == 0)
		{
			f->nodes.erase(f->nodes.begin() + i);
		}
	}
}

/// The node out of its footpath
void DeleteNode(entt::entity footpath, NodeId node)
{
	auto* f = FootpathOf(footpath);
	if (f == nullptr)
	{
		return;
	}
	if (const int32_t i = IndexOf(*f, node); i >= 0)
	{
		f->nodes.erase(f->nodes.begin() + i);
	}
}

/// On the world points A, B, C (the ground + altitude for y): u = A - C, v = B - C,
/// (vx uz - vz ux) / ((vz uz + vx ux) + vy uy) < 0.01
bool SmallTurn(const map_coords::MapCoords& a, const map_coords::MapCoords& b, const map_coords::MapCoords& c)
{
	const auto pa = map_coords::ToWorld(a);
	const auto pb = map_coords::ToWorld(b);
	const auto pc = map_coords::ToWorld(c);
	const float ux = pa.x - pc.x;
	const float uy = pa.y - pc.y;
	const float uz = pa.z - pc.z;
	const float vx = pb.x - pc.x;
	const float vy = pb.y - pc.y;
	const float vz = pb.z - pc.z;
	const float cross = vx * uz - vz * ux;
	const float zz = vz * uz;
	const float xx = vx * ux;
	const float yy = vy * uy;
	const float dot = (zz + xx) + yy;
	return static_cast<double>(cross / dot) < k_AngleTanMax;
}

/// StopReroutingAroundObstacle's near test: strictly inside the box (unsigned) and GetDistanceInMetres < L
bool NearForStop(const Footpath::Node& n, const map_coords::MapCoords& pos, std::span<const int32_t, 4> box, float limit)
{
	const auto x = static_cast<uint32_t>(n.coords.x);
	const auto z = static_cast<uint32_t>(n.coords.z);
	if (!(x > static_cast<uint32_t>(box[0]) && x < static_cast<uint32_t>(box[1]) && z > static_cast<uint32_t>(box[2]) &&
	      z < static_cast<uint32_t>(box[3])))
	{
		return false;
	}
	return gutils::GetDistanceInMetres(n.coords, pos) < limit;
}

/// The next node after `node` with neither hidden bit (the walk's "visible" step)
NodeId NextVisible(const Footpath& f, NodeId node)
{
	for (int32_t i = IndexOf(f, node) + 1; i > 0 && ValidIndex(f, i); ++i)
	{
		if ((f.nodes.at(static_cast<size_t>(i)).flags & footpaths::k_HiddenMask) == 0)
		{
			return f.nodes.at(static_cast<size_t>(i)).id;
		}
	}
	return k_NoNode;
}

NodeId NextOf(const Footpath& f, NodeId node)
{
	const int32_t i = IndexOf(f, node) + 1;
	return i > 0 && ValidIndex(f, i) ? f.nodes.at(static_cast<size_t>(i)).id : k_NoNode;
}

/// RerouteFootpathsAroundObstacle on one footpath
void SendAround(entt::entity footpath, float r, const map_coords::MapCoords& pos)
{
	auto* f = FootpathOf(footpath);
	if (f == nullptr || f->nodes.empty())
	{
		return;
	}
	// 1. the obstacle's box and squared radius in map units
	const int32_t bigR = gutils::ConvertMetersToWholeDistance(r);
	const int32_t xMin = pos.x - bigR;
	const int32_t xMax = pos.x + bigR;
	const int32_t zMin = pos.z - bigR;
	const int32_t zMax = pos.z + bigR;
	const auto bigR2 = static_cast<int32_t>(static_cast<uint32_t>(bigR) * static_cast<uint32_t>(bigR));
	const float r2 = static_cast<float>(bigR2) * k_AroundSlack;
	const auto inBox = [&](const map_coords::MapCoords& c) {
		const auto x = static_cast<uint32_t>(c.x);
		const auto z = static_cast<uint32_t>(c.z);
		return !(x < static_cast<uint32_t>(xMin) || x > static_cast<uint32_t>(xMax) || z < static_cast<uint32_t>(zMin) ||
		         z > static_cast<uint32_t>(zMax));
	};
	// 2. hide the inner nodes inside the circle, a visible copy after each
	for (size_t i = 1; i < f->nodes.size(); ++i)
	{
		const auto n = f->nodes.at(i);
		if ((n.flags & 0xEu) != 0 || i + 1 >= f->nodes.size() || !inBox(n.coords))
		{
			continue;
		}
		const float dx = Diff(static_cast<uint32_t>(n.coords.x), pos.x);
		const float dz = Diff(static_cast<uint32_t>(n.coords.z), pos.z);
		const float dz2 = dz * dz;
		const float dx2 = dx * dx;
		if (!(dz2 + dx2 < r2))
		{
			continue;
		}
		f->nodes.at(i).flags = static_cast<uint8_t>(f->nodes.at(i).flags | 4u);
		auto copy = MakeNode(*f, n.coords, 1, 1);
		f->nodes.insert(f->nodes.begin() + static_cast<std::ptrdiff_t>(i) + 1, std::move(copy));
		f = FootpathOf(footpath);
		++i; // the walk goes on from the copy, which has bit 1
	}
	// 3. the pairs of visible nodes
	NodeId prev = k_NoNode;
	NodeId cur = f->nodes.front().id;
	NodeId from = k_NoNode;
	NodeId to = k_NoNode;
	bool pending = false;
	const float posZ = static_cast<float>(static_cast<uint32_t>(pos.z));
	while (cur != k_NoNode)
	{
		f = FootpathOf(footpath);
		if (prev != k_NoNode)
		{
			const auto prevNode = *NodeOf(footpath, prev);
			const auto curNode = *NodeOf(footpath, cur);
			if ((prevNode.flags & 2u) == 0)
			{
				from = prev;
				to = FirstClean(*f, cur);
				pending = true;
			}
			const auto& p = prevNode.coords;
			const auto& c = curNode.coords;
			if (SpanMeets(p.x, c.x, xMin, xMax) && SpanMeets(p.z, c.z, zMin, zMax))
			{
				// |prev - pos|^2, both unsigned; prev.z - float(pos.z)
				const float pdx = static_cast<float>(static_cast<double>(static_cast<uint32_t>(p.x)) -
				                                     static_cast<double>(static_cast<uint32_t>(pos.x)));
				const float pdz =
				    static_cast<float>(static_cast<double>(static_cast<uint32_t>(p.z)) - static_cast<double>(posZ));
				const float pIn = pdz * pdz + pdx * pdx;
				// |cur - pos|^2: cur.x signed; cur.z unsigned - float(pos.z)
				const float cdx =
				    static_cast<float>(static_cast<double>(c.x) - static_cast<double>(static_cast<uint32_t>(pos.x)));
				const float cdz =
				    static_cast<float>(static_cast<double>(static_cast<uint32_t>(c.z)) - static_cast<double>(posZ));
				const float cIn = cdz * cdz + cdx * cdx;
				if (pIn <= r2 || cIn <= r2)
				{
					// one end inside: a pending rerender when that end has bit 1
					const auto& end = pIn <= r2 ? prevNode : curNode;
					if (pending && (end.flags & 2u) != 0)
					{
						if (from != k_NoNode && to != k_NoNode && footpaths::AttemptRerender(footpath, from, to, &pos))
						{
							return;
						}
						pending = false;
					}
				}
				else
				{
					// both outside: the closest point F of the segment, h = r^2 - |Q - F|^2
					const float ax = static_cast<float>(p.x) * k_ToMetres;
					const float az = static_cast<float>(p.z) * k_ToMetres;
					const float bx = static_cast<float>(c.x) * k_ToMetres;
					const float bz = static_cast<float>(c.z) * k_ToMetres;
					const float qx = static_cast<float>(pos.x) * k_ToMetres;
					const float qz = static_cast<float>(pos.z) * k_ToMetres;
					float vx = bx - ax;
					float vz = bz - az;
					const float wx = qx - ax;
					const float wz = qz - az;
					const float dotW = wx * vx + wz * vz;
					const float len2 = vx * vx + vz * vz;
					const float t = dotW / len2;
					const float fx = t * vx + ax;
					const float fz = t * vz + az;
					const float ex = qx - fx;
					const float ez = qz - fz;
					const float h = r * r - ((ex * ex + 0.0f) + ez * ez);
					// the followers of both ends re-plan
					ClearFromNextNode(footpath, prev);
					ClearFromPreviousNode(footpath, cur);
					if (static_cast<double>(h) > k_DetourMin)
					{
						// a pending rerender first
						if (pending && from != k_NoNode && to != k_NoNode &&
						    footpaths::AttemptRerender(footpath, from, to, &pos))
						{
							return;
						}
						// the segment's direction
						if (!(vx == 0.0f && vz == 0.0f))
						{
							const float inv = 1.0f / std::sqrt(vx * vx + vz * vz);
							vx = vx * inv;
							vz = vz * inv;
						}
						// the arc's entry and exit, relative to the centre
						const float s = std::sqrt(h);
						const float ux = vx * s;
						const float uz = vz * s;
						float aX = (fx - ux) - qx;
						float aZ = (fz - uz) - qz;
						const float bX = (fx + ux) - qx;
						const float bZ = (fz + uz) - qz;
						const map_coords::MapCoords exit {static_cast<int32_t>((bX + qx) * k_ToFixed),
						                                  static_cast<int32_t>((bZ + qz) * k_ToFixed), 0.0f};
						// (r x 0.35)^2, each product rounded to float (the same as ConvertCreaturePlanToFootpath's
						// limit)
						const auto r35 = static_cast<float>(static_cast<double>(r) * k_AroundArcFactor);
						const float limit = r35 * r35;
						// the side of the turn
						const float side = bX * aZ - bZ * aX;
						const auto isFar = [&]() {
							const float dx = aX - bX;
							const float dz = aZ - bZ;
							const float dsq = dz * dz + dx * dx;
							return dsq > limit;
						};
						std::vector<Footpath::Node> arc;
						f = FootpathOf(footpath);
						while (isFar())
						{
							arc.push_back(MakeNode(*f,
							                       {static_cast<int32_t>((qx + aX) * k_ToFixed),
							                        static_cast<int32_t>((qz + aZ) * k_ToFixed), 0.0f},
							                       1, 1));
							const auto pc = static_cast<float>(static_cast<double>(aX) * k_AroundCos);
							const auto ps = static_cast<float>(static_cast<double>(aZ) * k_AroundSin);
							const auto zc = static_cast<float>(static_cast<double>(aZ) * k_AroundCos);
							const auto xs = static_cast<float>(static_cast<double>(aX) * k_AroundSin);
							if (side > 0.0f)
							{
								aX = pc + ps;
								aZ = zc - xs;
							}
							else
							{
								aX = pc - ps;
								aZ = zc + xs;
							}
						}
						arc.push_back(MakeNode(*f, exit, 1, 1));
						// prev -> arc -> exit -> cur. (approximate) the nodes between prev and cur (only hidden ones,
						// the walk skips them) are dropped and leaked by the original; here they are freed, their
						// followers moved as for an erased node (ClearForDeletion)
						f = FootpathOf(footpath);
						for (int32_t i = IndexOf(*f, prev) + 1; i < IndexOf(*f, cur);)
						{
							ClearForDeletion(footpath, f->nodes.at(static_cast<size_t>(i)).id, prev, cur);
							f = FootpathOf(footpath);
							f->nodes.erase(f->nodes.begin() + i);
						}
						f->nodes.insert(f->nodes.begin() + IndexOf(*f, cur), arc.begin(), arc.end());
						return; // this footpath is done
					}
				}
			}
		}
		// on to the next visible node
		prev = cur;
		cur = NextVisible(*FootpathOf(footpath), cur);
	}
}

/// StopReroutingAroundObstacle on one footpath
void StopGoing(entt::entity footpath, float r, const map_coords::MapCoords& pos)
{
	auto* f = FootpathOf(footpath);
	if (f == nullptr || f->nodes.empty())
	{
		return;
	}
	// the near test: within L = 1.03 r + 0.4
	const float l = r * k_StopScale + k_StopAdd;
	const int32_t bigR = gutils::ConvertMetersToWholeDistance(l);
	const std::array<int32_t, 4> box {pos.x - bigR, pos.x + bigR, pos.z - bigR, pos.z + bigR};
	const auto isNear = [&](NodeId node) {
		const auto* n = NodeOf(footpath, node);
		return n != nullptr && NearForStop(*n, pos, box, l);
	};
	// pass 3b's run: the distance only, no box
	const auto isNearByDistance = [&](NodeId node) {
		const auto* n = NodeOf(footpath, node);
		return n != nullptr && gutils::GetDistanceInMetres(n->coords, pos) < l;
	};
	const auto flagsOf = [&](NodeId node) {
		const auto* n = NodeOf(footpath, node);
		return n != nullptr ? n->flags : static_cast<uint8_t>(0);
	};
	NodeId p = k_NoNode;
	NodeId to = k_NoNode; // pass 1's inner walker, then pass 2's end
	bool pending = false;
	// pass 1
	NodeId prev = k_NoNode;
	NodeId cur = f->nodes.front().id;
	bool found = false;
	while (cur != k_NoNode)
	{
		found = false;
		if (prev != k_NoNode && (flagsOf(prev) & 2u) == 0)
		{
			p = prev;
			NodeId e = cur;
			while (e != k_NoNode && (flagsOf(e) & 0xEu) != 0)
			{
				if ((flagsOf(e) & 4u) != 0 && isNear(e) && p != e)
				{
					NodeOf(footpath, e)->flags = static_cast<uint8_t>(flagsOf(e) & ~4u);
					EraseNodesBetween(footpath, p, e);
					static_cast<void>(footpaths::AttemptRerender(footpath, p, e, nullptr));
					p = e;
					e = NextOf(*FootpathOf(footpath), e);
					cur = e;
					found = true;
					continue;
				}
				e = NextOf(*FootpathOf(footpath), e);
			}
			to = e; // pass 2 starts with the inner walker's last node
			pending = true;
			if (found)
			{
				// a node was found: the nodes between p and e erased and re-planned
				EraseNodesBetween(footpath, p, e);
				static_cast<void>(footpaths::AttemptRerender(footpath, p, e, nullptr));
				break;
			}
		}
		prev = cur;
		cur = NextOf(*FootpathOf(footpath), cur);
	}
	if (cur == k_NoNode)
	{
		// pass 2
		prev = k_NoNode;
		cur = FootpathOf(footpath)->nodes.empty() ? k_NoNode : FootpathOf(footpath)->nodes.front().id;
		while (cur != k_NoNode)
		{
			if (prev != k_NoNode)
			{
				if ((flagsOf(prev) & 0xEu) == 0)
				{
					p = prev;
					to = FirstClean(*FootpathOf(footpath), cur);
					pending = true;
				}
				const auto fl = flagsOf(cur);
				if ((fl & 8u) != 0 && (fl & 2u) != 0 && isNear(cur) && pending)
				{
					const auto next = NextOf(*FootpathOf(footpath), cur);
					DeleteNode(footpath, cur); // prev.next = cur.next
					if (p != k_NoNode && to != k_NoNode && footpaths::AttemptRerender(footpath, p, to, nullptr))
					{
						cur = next == k_NoNode ? prev : next; // (openblack) the original ends the pass with cur non-null
						break;
					}
					cur = next;
					continue;
				}
			}
			prev = cur;
			cur = NextOf(*FootpathOf(footpath), cur);
		}
	}
	if (cur != k_NoNode)
	{
		// pass 3a: the near nodes with bits 8 and 2 (and a node before them) deleted; next footpath
		prev = k_NoNode;
		cur = FootpathOf(footpath)->nodes.empty() ? k_NoNode : FootpathOf(footpath)->nodes.front().id;
		while (cur != k_NoNode)
		{
			const auto fl = flagsOf(cur);
			if (prev != k_NoNode && (fl & 8u) != 0 && (fl & 2u) != 0 && isNear(cur))
			{
				const auto next = NextOf(*FootpathOf(footpath), cur);
				DeleteNode(footpath, cur);
				cur = next;
				continue;
			}
			prev = cur;
			cur = NextOf(*FootpathOf(footpath), cur);
		}
		return;
	}
	// pass 3b
	NodeId prevV = k_NoNode;
	cur = FootpathOf(footpath)->nodes.empty() ? k_NoNode : FootpathOf(footpath)->nodes.front().id;
	while (cur != k_NoNode)
	{
		f = FootpathOf(footpath);
		if (prevV != k_NoNode)
		{
			if ((flagsOf(prevV) & 2u) == 0)
			{
				p = prevV;
				to = FirstClean(*f, cur);
				pending = true;
			}
			if ((flagsOf(cur) & 2u) != 0 && isNear(cur))
			{
				if (pending)
				{
					if (p != k_NoNode && to != k_NoNode && footpaths::AttemptRerender(footpath, p, to, nullptr))
					{
						return;
					}
					pending = false;
				}
				const auto* pn = NodeOf(footpath, p);
				const auto* tn = NodeOf(footpath, to);
				if (pn != nullptr && tn != nullptr && !(tn->coords == pn->coords))
				{
					// the run from cur: the first node no longer near, and the last near one
					NodeId start = cur;
					NodeId exitN = k_NoNode;
					NodeId last = k_NoNode;
					NodeId lastNear = k_NoNode;
					bool inside = true;
					NodeId e = cur; // the outer walk goes on from where this one stops
					while (e != k_NoNode)
					{
						if (!inside && (flagsOf(e) & 2u) == 0)
						{
							break;
						}
						if (isNearByDistance(e))
						{
							inside = true;
						}
						else if (inside)
						{
							lastNear = last;
							inside = false;
							exitN = e;
						}
						last = e;
						e = NextVisible(*FootpathOf(footpath), e);
					}
					cur = e;
					if (exitN != k_NoNode)
					{
						// a run end where the path turns too much is kept
						if (!SmallTurn(NodeOf(footpath, prevV)->coords, NodeOf(footpath, start)->coords,
						               NodeOf(footpath, exitN)->coords))
						{
							prevV = start;
							start = NextOf(*FootpathOf(footpath), start);
						}
						if (lastNear != k_NoNode && start != exitN)
						{
							if (!SmallTurn(NodeOf(footpath, prevV)->coords, NodeOf(footpath, lastNear)->coords,
							               NodeOf(footpath, exitN)->coords))
							{
								exitN = lastNear;
							}
							if (start != exitN)
							{
								// the ends' followers re-plan, the nodes between prevV and exitN erased
								ClearFromPreviousNode(footpath, exitN);
								ClearFromNextNode(footpath, prevV);
								EraseNodesBetween(footpath, prevV, exitN);
								return;
							}
						}
					}
					// the outer walk goes on where the run stopped
					continue;
				}
				// (openblack, guard) P and `to` at the same coords: the original goes round again with the same
				// node, which only ends if a rerender succeeds; here the walk moves on
				cur = NextVisible(*FootpathOf(footpath), cur);
				continue;
			}
		}
		// on to the next visible node
		prevV = cur;
		cur = NextVisible(*FootpathOf(footpath), cur);
	}
}
} // namespace

void footpaths::RerouteFootpathsAroundObstacle(float r, const map_coords::MapCoords& pos)
{
	// (pending) nothing while the game is loading: taken as clear (openblack loads the .fot after the map script, so
	// the land's own objects do not move its footpaths)
	for (const auto footpath : FootpathsFromHead())
	{
		SendAround(footpath, r, pos);
	}
}

void footpaths::StopReroutingAroundObstacle(float r, const map_coords::MapCoords& pos)
{
	// the same guards (pending)
	for (const auto footpath : FootpathsFromHead())
	{
		StopGoing(footpath, r, pos);
	}
}
