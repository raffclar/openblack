/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalWallHug.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Common/GUtilsDistance.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

/// The circle hug for the animals (docs/bw1-notes/animals.md). Positions in metres (the original's map coordinates /
/// 6553.6), angles in 2048ths.
namespace openblack::ecs::animal_ai::detail
{
using components::AnimalBrain;
using components::Fixed;
using components::Transform;

namespace
{
/// The circle of a water cell, round its centre
constexpr float k_WaterCircleRadius = 7.2f;
/// A tree's trunk
constexpr float k_TreeCircleRadius = 0.3f;
/// A box more than 1.4 times longer than wide is a row of circles
constexpr float k_LongBox = 1.4f;
/// The sweeps' "behind me" bound
constexpr double k_Behind = -0.2;
/// Arc length (map coordinates) / radius (m) -> game angle, 2048 / (2 pi x 6553.6)
constexpr float k_ArcToAngle = 0.0497359186f;
/// cos / sin of 0.1 degrees: the goal point is taken that much before the goal
constexpr float k_Cos01 = 0.99999845f;
constexpr float k_Sin01 = 0.00174532842f;
/// The circle sweep's recursion budget
constexpr int k_SweepDepth = 3;

/// A collide circle of the cell: owner null = a water cell's
struct Circle
{
	glm::vec2 centre;
	float radius;
	entt::entity owner;
};

bool Trace()
{
	static const bool trace = debug_env::AnimalTrace();
	return trace;
}

void SetMoveState(Context& ctx, uint8_t state)
{
	if (Trace() && state != ctx.brain.moveState)
	{
		const auto& c = ctx.brain.hugCircle;
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Animal {}: move {:#x} -> {:#x} at ({:.2f}, {:.2f}) circle ({:.2f}, {:.2f}) r {:.2f}",
		                   static_cast<uint32_t>(ctx.entity), ctx.brain.moveState, state, ctx.transform.position.x,
		                   ctx.transform.position.z, c.centre.x, c.centre.y, c.radius);
	}
	ctx.brain.moveState = state;
}

glm::vec2 StepMetres(glm::ivec2 step)
{
	return glm::vec2(step) / k_MapCoordsPerMetre;
}

/// The circle being hugged. A circle whose fixed object was deleted is none [inferred: the original drops the huggers of
/// a deleted object]
const AnimalBrain::HugCircle* HugObj(Context& ctx)
{
	auto& circle = ctx.brain.hugCircle;
	if (circle.set && circle.owner != entt::null && !ecs::IsAvailable(circle.owner))
	{
		circle.set = false;
	}
	return circle.set ? &circle : nullptr;
}

/// Sets the circle being hugged; the rest of the original's version is bookkeeping
void SetObjectPtr(Context& ctx, const Circle* circle)
{
	ctx.brain.hugCircle = circle != nullptr ? AnimalBrain::HugCircle {circle->centre, circle->radius, circle->owner, true}
	                                        : AnimalBrain::HugCircle {};
	if (Trace() && circle != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: hug circle ({:.2f}, {:.2f}) r {:.2f} {} turns {}",
		                   static_cast<uint32_t>(ctx.entity), circle->centre.x, circle->centre.y, circle->radius,
		                   circle->owner == entt::null ? "water" : "object", ctx.brain.turnsToObj);
	}
}

/// The heading, and the step along it
void SetGameAngle(Context& ctx, int32_t angle)
{
	ctx.brain.angle = static_cast<uint16_t>(angle & 0x7FF);
	FaceAngle(ctx.transform, ctx.brain.angle);
}

void RebuildMoveByStep(Context& ctx)
{
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
}

/// FINAL_STEP (4) from a hug handler: the step is Pos - goal (unused: FINAL_STEP snaps to the goal next turn)
void SetFinalStep(Context& ctx)
{
	ctx.brain.step = glm::ivec2((Xz(ctx.transform) - ctx.brain.goal) * k_MapCoordsPerMetre);
	SetMoveState(ctx, k_MoveFinalStep);
}

// ---- the collide circles of a map cell ----

/// A collide shape for a box longer than 1.4 times its width: a row of int(long / short) + 1 circles of the short
/// half-extent along the long axis
void RowOfCircles(entt::entity entity, const Fixed& fixed, const Transform& transform, std::vector<Circle>& out)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const components::Mesh>(entity);
	if (mesh == nullptr || !Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		return;
	}
	const auto box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
	// The half-extents x the scale, at least 1 m (as archetypes::GetFixedObstacleBoundingCircle)
	const glm::vec2 size(box.Size().x * transform.scale.x, box.Size().z * transform.scale.z);
	const glm::vec2 half = glm::max(glm::vec2(1.0f), size * 0.5f);
	const bool alongX = half.x > half.y;
	const float longHalf = alongX ? half.x : half.y;
	const float shortHalf = alongX ? half.y : half.x;
	if (!(longHalf / shortHalf > k_LongBox))
	{
		return;
	}
	const int count = static_cast<int>(longHalf / shortHalf) + 1;
	const float spacing = 2.0f * longHalf / static_cast<float>(count);
	for (int i = 0; i < count; ++i)
	{
		const float offset = (static_cast<float>(i) + 0.5f) * spacing - longHalf;
		const glm::vec3 local = alongX ? glm::vec3(offset, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, offset);
		const glm::vec3 world = transform.rotation * local;
		out.push_back({fixed.boundingCenter + glm::vec2(world.x, world.z), shortHalf, entity});
	}
}

/// The land cell under a cell index is water, or there is none (the neighbours' test is the same hasWater bit)
bool WaterCell(glm::ivec2 cell)
{
	if (!map_coords::InBounds(cell)) // out of bounds: the unsigned compare with 512
	{
		return true;
	}
	return Collides(glm::vec2(cell) * 10.0f + 5.0f, 1u);
}

/// The collide circles of the map cell of p: those of the objects of the cell's fixed list in its order that have any
/// (an object without collide data and a field are skipped), then the water circles of the cell and of its 8
/// neighbours in the original's order. Which objects are in the cell is the list's: a multi-cell object is in the cells
/// its collide shape covers.
std::vector<Circle> CellCircles(glm::vec2 p)
{
	std::vector<Circle> out;
	const auto cell = CellOf(p);
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<Circle> row;
	map_cells::ForEachFixed(glm::ivec2(cell), [&](entt::entity entity) {
		// No collide data: a plain object (pots and piles, street lanterns; inferred), a fish farm and a big forest; a
		// field is skipped by its class
		const auto kind = map_cells::KindOf(entity);
		if (kind == map_cells::InsertKind::Object || kind == map_cells::InsertKind::FishFarm ||
		    registry.AnyOf<components::Field, components::BigForest>(entity))
		{
			return true;
		}
		const auto& transform = registry.Get<const Transform>(entity);
		if (registry.AllOf<components::Tree>(entity))
		{
			// Its trunk
			out.push_back({Xz(transform), k_TreeCircleRadius, entity});
			return true;
		}
		const auto* fixed = registry.TryGet<const Fixed>(entity);
		if (fixed == nullptr)
		{
			// (approximate) a mobile static, rock, dead tree or fragment (fixed in the original, without openblack's Fixed):
			// one circle of its Get2DRadius at its position, not its mesh's collide shape
			out.push_back({Xz(transform), object::Get2DRadius(entity), entity});
			return true;
		}
		row.clear();
		RowOfCircles(entity, *fixed, transform, row);
		if (row.empty())
		{
			out.push_back({fixed->boundingCenter, fixed->boundingRadius, entity});
		}
		else
		{
			out.insert(out.end(), row.begin(), row.end());
		}
		return true;
	});
	// After the objects: the cell itself, then its 8 neighbours
	static constexpr std::array<glm::ivec2, 9> k_Neighbours = {
	    {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}}};
	for (const auto& offset : k_Neighbours)
	{
		const glm::ivec2 c = glm::ivec2(cell) + offset;
		if (WaterCell(c))
		{
			out.push_back({glm::vec2(c) * 10.0f + 5.0f, k_WaterCircleRadius, entt::null});
		}
	}
	return out;
}

// ---- the linear sweep ----

/// lo and hi bound the distance along the step to the circle's edge (dot - r and dot, equal once resolved), disc the
/// ray/circle discriminant, then the circle
struct LinearSweep
{
	float lo;
	float hi;
	float disc;
	const Circle* circle;
};

/// The true entry distance dot - sqrt(disc); behind (< -0.2) it is FLT_MAX
void Resolve(LinearSweep& s)
{
	if (s.lo == s.hi)
	{
		return;
	}
	const float t = s.hi - std::sqrt(s.disc);
	s.lo = s.hi = t;
	if (static_cast<double>(t) < k_Behind)
	{
		s.lo = s.hi = std::numeric_limits<float>::max();
	}
}

/// The sweep entry of a circle along the step's ray
LinearSweep MakeLinearSweep(const Circle& circle, glm::vec2 pos, glm::vec2 dir)
{
	const glm::vec2 rel = circle.centre - pos;
	const float dot = glm::dot(dir, rel);
	LinearSweep s {dot - circle.radius, dot, dot * dot + (circle.radius * circle.radius - glm::dot(rel, rel)), &circle};
	if (static_cast<double>(s.lo) < k_Behind && s.disc > 0.0f)
	{
		Resolve(s);
	}
	return s;
}

/// The candidate is nearer than the best
bool Nearer(LinearSweep& candidate, LinearSweep& best)
{
	if (candidate.hi < best.lo)
	{
		return true;
	}
	if (candidate.lo > best.hi)
	{
		return false;
	}
	Resolve(candidate);
	Resolve(best);
	return candidate.hi < best.lo;
}

/// The nearest collide circle of dest's cell the step's ray enters; its countdown TurnsToObj (0xFF: none, or more than
/// 255 turns away)
void LinearSquareSweep(Context& ctx, glm::vec2 dest)
{
	// The step in metres, normalised; its length (0 for no step) divides the distance into turns
	glm::vec2 dir = StepMetres(ctx.brain.step);
	float stepLength = 0.0f;
	if (dir.x != 0.0f || dir.y != 0.0f)
	{
		stepLength = glm::length(dir);
		dir /= stepLength;
	}
	SetObjectPtr(ctx, nullptr);
	const auto circles = CellCircles(dest);
	const glm::vec2 pos = Xz(ctx.transform);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: linear sweep of cell ({}, {}): {} circles, dir ({:.2f}, {:.2f})",
		                   static_cast<uint32_t>(ctx.entity), CellOf(dest).x, CellOf(dest).y, circles.size(), dir.x, dir.y);
	}
	size_t i = 0;
	LinearSweep best {};
	for (; i < circles.size(); ++i)
	{
		best = MakeLinearSweep(circles[i], pos, dir);
		if (best.disc > 0.0f)
		{
			break;
		}
	}
	if (i >= circles.size())
	{
		ctx.brain.turnsToObj = 0xFF;
		return;
	}
	for (++i; i < circles.size(); ++i)
	{
		auto candidate = MakeLinearSweep(circles[i], pos, dir);
		if (candidate.disc > 0.0f && Nearer(candidate, best))
		{
			best = candidate;
		}
	}
	Resolve(best);
	if (best.lo == std::numeric_limits<float>::max())
	{
		ctx.brain.turnsToObj = 0xFF;
		return;
	}
	// Turns = distance / |step|; above 255 none, not above 0 (or unordered) 0
	float turns = best.lo / stepLength;
	if (turns > 255.0f)
	{
		ctx.brain.turnsToObj = 0xFF;
		return;
	}
	if (!(turns > 0.0f))
	{
		turns = 0.0f;
	}
	ctx.brain.turnsToObj = static_cast<uint8_t>(static_cast<int32_t>(turns));
	if (ctx.brain.turnsToObj != 0xFF)
	{
		SetObjectPtr(ctx, best.circle);
	}
}

// ---- the circle sweep (CW / CCW) ----

/// A point on the orbit and its half relative to where the animal is
struct SidePoint
{
	glm::vec2 p {0.0f};
	bool side {false};
};

/// Where another circle cuts the orbit, bounded by lo and hi until resolved; a, h2 and h the chord's terms in the frame
/// of d = its centre - the orbit's centre (the cut is a d + h perp(d), valid when h2 > 0); the circle, none for the goal
struct Intersect
{
	SidePoint lo;
	SidePoint hi;
	float a {0.0f};
	float h2 {0.0f};
	float h {0.0f};
	const Circle* circle {nullptr};
};

/// The angular order along the orbit, starting from the animal (g = its position - the centre)
struct OrbitOrder
{
	bool cw;
	glm::vec2 g;
	float r;

	/// The half the orbit reaches first
	[[nodiscard]] bool Side(glm::vec2 p) const
	{
		const float c = g.x * p.y - g.y * p.x;
		return cw ? c <= 0.0f : c >= 0.0f;
	}

	/// a is reached before b
	[[nodiscard]] bool Lt(const SidePoint& a, const SidePoint& b) const
	{
		if (a.side != b.side)
		{
			return a.side;
		}
		const float c = b.p.y * a.p.x - a.p.y * b.p.x;
		return cw ? c < 0.0f : c > 0.0f;
	}

	/// The exact cut
	void Exact(Intersect& s) const
	{
		if (s.lo.p == s.hi.p)
		{
			return;
		}
		s.h = std::sqrt(s.h2);
		const glm::vec2 d = s.hi.p;
		s.lo.p = cw ? glm::vec2(s.a * d.x - s.h * d.y, s.a * d.y + s.h * d.x)
		            : glm::vec2(s.a * d.x + s.h * d.y, s.a * d.y - s.h * d.x);
		s.lo.side = Side(s.lo.p);
		s.hi = s.lo;
	}

	/// hi = d, lo = d + perp(d) (x the radii's ratio when the other circle is not bigger); resolved at once when the bounds
	/// come out in the wrong order
	[[nodiscard]] Intersect Make(const Circle& circle, glm::vec2 centre) const
	{
		Intersect s;
		s.circle = &circle;
		const glm::vec2 d = circle.centre - centre;
		s.hi = {d, Side(d)};
		glm::vec2 v = cw ? glm::vec2(-d.y, d.x) : glm::vec2(d.y, -d.x);
		if (!(circle.radius > r))
		{
			v *= circle.radius / r;
		}
		s.lo.p = d + v;
		s.lo.side = Side(s.lo.p);
		const float dd = glm::dot(d, d);
		s.a = 0.5f * (dd + r * r - circle.radius * circle.radius) / r;
		s.h2 = dd - s.a * s.a;
		if (s.h2 > 0.0f && Lt(s.hi, s.lo))
		{
			Exact(s);
		}
		return s;
	}
};

/// The end of the sweep: the heading along the orbit, towards the centre + / - 90 degrees, less 64 per radius the
/// animal is off 0.9 x the radius
void AimAlongOrbit(Context& ctx, bool cw, float numCircles)
{
	const auto* circle = HugObj(ctx);
	if (circle == nullptr)
	{
		return;
	}
	const int32_t toCentre = gutils::GetAngleFromXZ(Xz(ctx.transform), circle->centre);
	const int32_t angle = cw ? toCentre + 0x200 - static_cast<int32_t>(numCircles * 64.0f)
	                         : toCentre - 0x200 - static_cast<int32_t>(numCircles * -64.0f);
	SetGameAngle(ctx, angle);
	RebuildMoveByStep(ctx);
}

/// Along the orbit of the current circle, the first place where another circle of
/// dest's cell cuts it (or the goal, when the goal is inside this circle); TurnsToObj = the turns to get there at 2/3 of
/// the speed, then the heading along the orbit. Closer than a turn: the goal -> STEP_THROUGH; another circle -> it
/// becomes the circle and the sweep runs again from it (3 levels at most, then TurnsToObj 10).
int CircleSquareSweep(Context& ctx, glm::vec2 dest, bool cw, int depth)
{
	const auto* current = HugObj(ctx);
	if (current == nullptr)
	{
		// The original always has one here (it reads the circle unchecked); openblack only gets here when the circle's
		// object was deleted (HugObj) [inferred guard, not in the original]
		ctx.brain.turnsToObj = 0xFF;
		return 1;
	}
	const glm::vec2 centre = current->centre;
	const float r = current->radius;
	const glm::vec2 pos = Xz(ctx.transform);
	const OrbitOrder order {cw, pos - centre, r};
	glm::vec2 u = order.g;
	float length = 0.0f;
	if (u.x != 0.0f || u.y != 0.0f)
	{
		length = glm::length(u);
		u /= length;
	}
	const float numCircles = static_cast<float>(static_cast<double>(length / r) - 0.9);
	const glm::vec2 goal = ctx.brain.goal;
	const bool goalInside = glm::distance2(goal, centre) < r * r;

	const auto circles = CellCircles(dest);
	if (circles.empty() && !goalInside)
	{
		ctx.brain.turnsToObj = 0xFF;
		AimAlongOrbit(ctx, cw, numCircles);
		return 1;
	}

	Intersect best;
	SidePoint goalPoint;
	size_t i = 0;
	if (goalInside)
	{
		// The goal, 0.1 degrees before it along the orbit
		const glm::vec2 p = goal - centre;
		glm::vec2 q = u;
		if (p.x != 0.0f || p.y != 0.0f)
		{
			q = cw ? glm::vec2(p.x * k_Cos01 - p.y * k_Sin01, p.y * k_Cos01 + p.x * k_Sin01)
			       : glm::vec2(p.y * k_Sin01 + p.x * k_Cos01, p.y * k_Cos01 - p.x * k_Sin01);
		}
		best.lo = {q, order.Side(q)};
		best.hi = best.lo;
		best.h2 = 1.0f;
		goalPoint = best.hi;
	}
	else
	{
		for (; i < circles.size(); ++i)
		{
			best = order.Make(circles[i], centre);
			if (best.h2 > 0.0f)
			{
				break;
			}
		}
		++i;
	}
	for (; i < circles.size(); ++i)
	{
		auto candidate = order.Make(circles[i], centre);
		if (!(candidate.h2 > 0.0f))
		{
			continue;
		}
		bool first;
		if (order.Lt(candidate.hi, best.lo))
		{
			first = true;
		}
		else if (order.Lt(best.hi, candidate.lo))
		{
			first = false;
		}
		else
		{
			order.Exact(candidate);
			order.Exact(best);
			first = order.Lt(candidate.hi, best.lo);
		}
		// A water circle never replaces the goal
		if (first && !(circles[i].owner == entt::null && best.circle == nullptr))
		{
			best = candidate;
		}
	}

	if (!(best.h2 > 0.0f))
	{
		ctx.brain.turnsToObj = 0xFF;
		AimAlongOrbit(ctx, cw, numCircles);
		return 1;
	}
	order.Exact(best);
	if (goalInside)
	{
		// The goal still wins when it comes before the circle's far cut
		const glm::vec2 p = best.hi.p;
		const float a = best.a;
		const float h = best.h;
		glm::vec2 q;
		if (cw)
		{
			const float t1 = a * p.x + h * p.y;
			const float t2 = a * p.y - h * p.x;
			q = {a * t1 + h * t2, a * t2 - h * t1};
		}
		else
		{
			const float t1 = a * p.x - h * p.y;
			const float t2 = h * p.x + a * p.y;
			q = {a * t1 - h * t2, h * t1 + a * t2};
		}
		if (order.Lt(goalPoint, {q, order.Side(q)}))
		{
			best.hi = goalPoint;
			best.lo = best.hi;
			best.circle = nullptr;
		}
	}
	glm::vec2 n = best.hi.p;
	if (n.x != 0.0f || n.y != 0.0f)
	{
		n = glm::normalize(n);
	}
	float angle = std::acos(std::clamp(glm::dot(n, u), -1.0f, 1.0f));
	// The 2D cross product
	const float cross = n.y * u.x - u.y * n.x;
	if (cw ? cross > 0.0f : cross < 0.0f)
	{
		angle = glm::two_pi<float>() - angle;
	}
	const double speed = static_cast<double>(ctx.brain.speed) * 10.0 * 1.5;
	const auto turns = static_cast<float>(static_cast<double>(angle) * r * 65536.0 / speed);
	if (turns > 255.0f)
	{
		ctx.brain.turnsToObj = 0xFF;
	}
	else if (turns < 1.0f)
	{
		if (best.circle == nullptr)
		{
			// At the goal's place on the orbit: straight at it (the hug is reset; a flag it sets is not kept: not
			// identified [inferred: not read on the animals' path])
			InitStepsXZ(ctx);
			SetObjectPtr(ctx, nullptr);
			ctx.brain.turnsToObj = 0xFF;
			ctx.brain.hugGoalDistance = 0;
			SetMoveState(ctx, k_MoveStepThrough);
			return 1;
		}
		SetObjectPtr(ctx, best.circle);
		if (k_SweepDepth - depth > 0)
		{
			return CircleSquareSweep(ctx, dest, cw, depth + 1);
		}
		ctx.brain.turnsToObj = 0xA;
		return 1;
	}
	else if (turns < 4.0f)
	{
		ctx.brain.turnsToObj = 0;
	}
	else
	{
		ctx.brain.turnsToObj = static_cast<uint8_t>(static_cast<int32_t>(turns));
	}
	AimAlongOrbit(ctx, cw, numCircles);
	return 1;
}

// ---- the MoveTo handlers ----

/// The LINEAR step. A new map cell re-aims (InitStepsXZ) and sweeps again; TurnsToObj
/// running out starts the orbit, on the side the step passes the centre (LINEAR_CW / CCW keep theirs)
int MoveToCircleHug(Context& ctx)
{
	const glm::ivec2 step = ctx.brain.step;
	const glm::vec2 pos = Xz(ctx.transform);
	const glm::vec2 next = pos + StepMetres(step);
	if (CellOf(next) != CellOf(pos))
	{
		InitStepsXZ(ctx);
		LinearSquareSweep(ctx, next);
	}
	if (ctx.brain.turnsToObj != 0xFF)
	{
		const uint8_t was = ctx.brain.turnsToObj--;
		// The original reads the circle unchecked; null here only after its object was deleted [inferred guard]
		const auto* circle = HugObj(ctx);
		if (was == 0 && circle != nullptr)
		{
			const glm::vec2 rel = pos - circle->centre;
			const glm::vec2 s = StepMetres(ctx.brain.step);
			const float cross = rel.y * s.x - s.y * rel.x;
			uint8_t orbit;
			if (ctx.brain.moveState == k_MoveLinear)
			{
				orbit = cross > 0.0f ? k_MoveOrbitCw : k_MoveOrbitCcw;
			}
			else
			{
				orbit = ctx.brain.moveState == k_MoveLinearCw ? k_MoveOrbitCw : k_MoveOrbitCcw;
			}
			SetMoveState(ctx, orbit);
			CircleSquareSweep(ctx, pos, orbit == k_MoveOrbitCw, 1);
			// The distance to the goal x 128, less 1. The exact square distance and a square root on it; the original
			// loads double constants but computes at float precision, so it is all float
			const float squared =
			    gutils::GetMetresDistanceSq(map_coords::FromMetres(pos), map_coords::FromMetres(ctx.brain.goal));
			const float v = std::sqrt(squared) * 128.0f - 1.0f;
			ctx.brain.hugGoalDistance = v > 0.0f ? static_cast<uint32_t>(v) : 0u;
		}
	}
	return MoveBy(ctx, step) ? 7 : 6;
}

int Linear(Context& ctx)
{
	if (ctx.brain.turnsToObj != 0xFF && HugObj(ctx) == nullptr)
	{
		LinearSquareSweep(ctx, Xz(ctx.transform));
	}
	const int r = MoveToCircleHug(ctx);
	if (AreWeThere(ctx))
	{
		SetFinalStep(ctx);
	}
	return r;
}

/// ORBIT_CW / ORBIT_CCW: turn by the arc of one step on the circle, sweep again on a new cell or
/// when TurnsToObj runs out; leave (EXIT_CIRCLE, straight out from the centre) once nearer the goal than when the orbit
/// began, with the goal on the inner side and ahead
int Orbit(Context& ctx, bool cw)
{
	const auto* circle = HugObj(ctx);
	if (circle == nullptr)
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, cw ? k_MoveLinearCw : k_MoveLinearCcw);
		LinearSquareSweep(ctx, Xz(ctx.transform));
		return 1;
	}
	const auto arc = static_cast<int32_t>(static_cast<float>(ctx.brain.speed) / circle->radius * k_ArcToAngle);
	SetGameAngle(ctx, cw ? ctx.brain.angle - arc - 1 : ctx.brain.angle + arc + 1);
	RebuildMoveByStep(ctx);
	const glm::ivec2 step = ctx.brain.step;
	const glm::vec2 pos = Xz(ctx.transform);
	const glm::vec2 next = pos + StepMetres(step);
	if (CellOf(next) != CellOf(pos))
	{
		CircleSquareSweep(ctx, next, cw, 1);
	}
	if (ctx.brain.turnsToObj != 0xFF)
	{
		const uint8_t was = ctx.brain.turnsToObj--;
		if (was == 0)
		{
			CircleSquareSweep(ctx, next, cw, 1);
		}
	}
	const int r = MoveBy(ctx, step) ? 7 : 6;
	if (AreWeThere(ctx))
	{
		SetFinalStep(ctx);
		return r;
	}
	const auto x = static_cast<double>(ctx.brain.hugGoalDistance);
	const auto threshold = static_cast<float>(x * static_cast<double>(6.103515625e-05f) * x);
	const glm::vec2 now = Xz(ctx.transform);
	if (!(glm::distance2(now, ctx.brain.goal) < threshold))
	{
		return r;
	}
	// The step as it is now (the sweeps above may have re-aimed it), not the one it moved by
	const glm::vec2 d = now - ctx.brain.goal;
	const glm::vec2 s(ctx.brain.step);
	const float cross = s.x * d.y - s.y * d.x;
	if (cw ? !(cross < 0.0f) : !(cross > 0.0f))
	{
		return r;
	}
	if (!(d.x * s.x + d.y * s.y < 0.0f))
	{
		return r;
	}
	if (const auto* exit = HugObj(ctx); exit != nullptr)
	{
		SetGameAngle(ctx, gutils::GetAngleFromXZ(exit->centre, now));
		RebuildMoveByStep(ctx);
	}
	SetMoveState(ctx, cw ? k_MoveExitCircleCw : k_MoveExitCircleCcw);
	return r;
}

/// EXIT_CIRCLE: straight on until out of the circle, then LINEAR_CW / CCW again
int ExitCircle(Context& ctx)
{
	const uint8_t linear = ctx.brain.moveState == k_MoveExitCircleCw ? k_MoveLinearCw : k_MoveLinearCcw;
	if (HugObj(ctx) == nullptr)
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, linear);
		LinearSquareSweep(ctx, Xz(ctx.transform));
		return 1;
	}
	const int r = MoveBy(ctx, ctx.brain.step) ? 7 : 6;
	if (AreWeThere(ctx))
	{
		SetFinalStep(ctx);
		return r;
	}
	const auto* circle = HugObj(ctx);
	if (circle != nullptr && circle->radius * circle->radius < glm::distance2(circle->centre, Xz(ctx.transform)))
	{
		InitStepsXZ(ctx);
		SetMoveState(ctx, linear);
		LinearSquareSweep(ctx, Xz(ctx.transform));
	}
	return r;
}
} // namespace

bool IsHugMoveState(uint8_t state)
{
	return state >= k_MoveLinear && state <= k_MoveExitCircleCw;
}

void SetupMoveToWithHug(Context& ctx, glm::vec2 p, AnimalState final)
{
	if (!SetCurrentAndDestinationState(ctx, final))
	{
		return;
	}
	// Set up the move to `p` as LINEAR: the hug is not reset, the sweep drops the circle. hugGoalDistance = 0 always here;
	// the original writes it only when the hug had an entry and leaves it stale otherwise, which nothing observes: only
	// ORBIT reads it, after MoveToCircleHug wrote it
	ctx.brain.goal = p;
	InitStepsXZ(ctx);
	ctx.brain.hugGoalDistance = 0;
	if (AreWeThere(ctx))
	{
		ctx.brain.moveState = k_MoveArrived;
		return;
	}
	LinearSquareSweep(ctx, Xz(ctx.transform));
	SetMoveState(ctx, k_MoveLinear);
}

int HugMoveTo(Context& ctx)
{
	switch (ctx.brain.moveState)
	{
	case k_MoveLinear:
	case k_MoveLinearCw:
	case k_MoveLinearCcw:
		return Linear(ctx);
	case k_MoveOrbitCw:
		return Orbit(ctx, true);
	case k_MoveOrbitCcw:
		return Orbit(ctx, false);
	case k_MoveExitCircleCcw:
	case k_MoveExitCircleCw:
		return ExitCircle(ctx);
	default:
		return 0;
	}
}

} // namespace openblack::ecs::animal_ai::detail
