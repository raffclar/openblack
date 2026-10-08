/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RoutePlanWorld.h"

#include <cmath>

#include <array>
#include <optional>

#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MapShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MapShield.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RouteFollower.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using route_planner::Point2D;

namespace
{
constexpr float k_FixedToMetres = 10.0f / 65536.0f; ///< MapCoords to metres
constexpr float k_MetresToFixed = 6553.6f;          ///< metres to MapCoords
constexpr float k_LandAvoidRadius = 7.1f;           ///< the land-avoid circle at a cell's centre
constexpr float k_MinRadius = 0.05f;                ///< the smallest circle
constexpr float k_LongSide = 1.2f;                  ///< a side this much longer than the other is the long side
constexpr float k_FifthOfLong = 0.2f;               ///< a fifth of the long side
constexpr double k_CircleRadiusFactor = 1.15;       ///< the circles along the long side, a bit wider than the short side
constexpr float k_QuarterTurn = 1.57079637f;        ///< pi / 2

const Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

void Emit(entt::entity object, route_planner::ObstacleGrid& holder, const Point2D& centre, float radius, int32_t notify,
          route_plan_world::CircleFn callback, void* context)
{
	// the callback when given (the push-out's), else AddObject
	if (callback != nullptr)
	{
		callback(context, object, centre, radius, notify);
	}
	else
	{
		holder.AddObject(route_plan_world::IdOf(object), centre, radius, notify);
	}
}

bool IsPileFood(entt::entity object)
{
	// food piles (and magic food / puzzle grain): the pots whose info potType is PileFood
	const auto* pot = Entities().TryGet<const Pot>(object);
	if (pot == nullptr || pot->type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return false;
	}
	return Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).potType == PotType::PileFood;
}

/// with no creature: one circle of GetRoutePlanRadius + margin at the position
void SimpleAddToRoutePlan(entt::entity object, route_planner::ObstacleGrid& holder, int32_t notify,
                          route_plan_world::CircleFn callback, void* context)
{
	// the 5.0 extra only with a creature
	constexpr float k_Extra = 0.0f;
	float r = object::GetRoutePlanRadius(object) + holder.GetMargin();
	r = r + k_Extra;
	if (r < k_MinRadius)
	{
		r = k_MinRadius;
	}
	// the position in metres (the altitude dropped)
	const auto centre = route_plan_world::ToPoint2D(object::MapCoordsOf(object));
	Emit(object, holder, centre, r, notify, callback, context);
}

/// the generic case with no creature: the mesh box as one circle, or 2..5 along its long side
void ObjectAddToRoutePlan(entt::entity object, route_planner::ObstacleGrid& holder, int32_t notify,
                          route_plan_world::CircleFn callback, void* context)
{
	const auto& registry = Entities();
	constexpr float k_Extra = 0.0f;
	const float margin = holder.GetMargin();
	// no mesh -> nothing
	const auto* mesh = registry.TryGet<const Mesh>(object);
	const auto* transform = registry.TryGet<const Transform>(object);
	if (mesh == nullptr || transform == nullptr || !Locator::resources::has_value() ||
	    !Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		return;
	}
	const auto box = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox();
	const auto half = object::MeshHalfExtents(mesh->id);
	if (!half.has_value())
	{
		return;
	}
	// the box centre through the world matrix. (approximate) openblack's
	// rotation x scale + position stands for the original's matrix product, in glm's order
	const glm::vec3 boxCentre = (box.minima + box.maxima) * 0.5f;
	const glm::vec3 world = transform->position + transform->rotation * (boxCentre * transform->scale);
	const Point2D centre {world.x, world.z};
	// the half sizes x the scale, + margin + extra
	const float scale = object::GetScale(object);
	const float hx = ((scale * half->x) + margin) + k_Extra;
	const float hz = ((scale * half->z) + margin) + k_Extra;
	int32_t n = 1;
	float shortSide = 0.0f;
	float step = 0.0f;
	bool xLong = false;
	if (hx > hz * k_LongSide)
	{
		// x is the long side
		n = static_cast<int32_t>(hx / hz) + 1;
		float zSide = hz;
		if (n > 5)
		{
			n = 5;
			zSide = hx * k_FifthOfLong;
		}
		shortSide = zSide;
		step = hx / static_cast<float>(n);
		xLong = true;
	}
	else if (hx * k_LongSide < hz)
	{
		// z is the long side
		n = static_cast<int32_t>(hz / hx) + 1;
		shortSide = hx;
		if (n > 5)
		{
			n = 5;
			shortSide = hz * k_FifthOfLong;
		}
		step = hz / static_cast<float>(n);
	}
	if (n == 1)
	{
		// one circle of Get2DRadius + margin + extra
		float r = (object::Get2DRadius(object) + margin) + k_Extra;
		if (r < k_MinRadius)
		{
			r = k_MinRadius;
		}
		Emit(object, holder, centre, r, notify, callback, context);
		return;
	}
	// along the Y angle, + pi / 2 when x is the long side
	const float theta = map_cells::detail::YAngleOf(transform->rotation) + (xLong ? k_QuarterTurn : 0.0f);
	const float s = static_cast<float>(std::sin(static_cast<double>(theta))) * step;
	const float c = -(static_cast<float>(std::cos(static_cast<double>(theta))) * step);
	const float nm1 = static_cast<float>(n - 1);
	Point2D p {centre.x - s * nm1, centre.z - c * nm1};
	const float s2 = s + s;
	const float c2 = c + c;
	// short x 1.15, at least 0.05
	float r = static_cast<float>(static_cast<double>(shortSide) * k_CircleRadiusFactor);
	if (r < k_MinRadius)
	{
		r = k_MinRadius;
	}
	for (int32_t i = 0; i < n; ++i)
	{
		Emit(object, holder, p, r, notify, callback, context);
		p.x = s2 + p.x;
		p.z = p.z + c2;
	}
}
/// the citadel heart with no creature: the centre circle, then 7 arms from the
/// Y angle + 3.83, 2 pi / 7 apart; arms 0 and 1 six circles each, the others 2, 3 or 4 by the owner's alignment
void CitadelHeartAddToRoutePlan(entt::entity heart, route_planner::ObstacleGrid& holder, int32_t notify,
                                route_plan_world::CircleFn callback, void* context)
{
	constexpr float k_Size = 21.5f;                              ///< the heart's size
	constexpr float k_CentreFactor = 0.64f;                      ///< the centre circle's share of the size
	constexpr float k_CentreExtra = 1.1f;                        ///< added to the centre circle
	constexpr float k_ArmStart = 3.83f;                          ///< the first arm's angle from the heart's heading
	constexpr float k_ArmRadius = 0.11f;                         ///< the arm circles' share of the size
	constexpr float k_ArmStep = 0.897598f;                       ///< 2 pi / 7
	constexpr std::array<float, 3> k_Turn {0.06f, 0.16f, 0.27f}; ///< the turns of the last three circles of arms 0 and 1
	const auto& registry = Entities();
	// the position in metres (the altitude dropped)
	const auto at = route_plan_world::ToPoint2D(object::MapCoordsOf(heart));
	const float centreRadius = k_Size * k_CentreFactor;
	Emit(heart, holder, at, centreRadius + k_CentreExtra, notify, callback, context);
	// the owner's alignment: < -0.7 -> 2, < -0.3 -> 3, else 4
	int32_t count = 4;
	if (const auto* temple = registry.TryGet<const Temple>(heart); temple != nullptr)
	{
		// the alignment is kept with the player for the whole game, not on its entity
		const float a = effects::alignment::Get(temple->owner);
		count = a < -0.7f ? 2 : (a < -0.3f ? 3 : 4);
	}
	const auto* transform = registry.TryGet<const Transform>(heart);
	float theta = (transform != nullptr ? map_cells::detail::YAngleOf(transform->rotation) : 0.0f) + k_ArmStart;
	const float armRadius = k_Size * k_ArmRadius;
	for (int32_t arm = 0; arm < 7; ++arm)
	{
		const float s = static_cast<float>(std::sin(static_cast<double>(theta)));
		const float c = static_cast<float>(std::cos(static_cast<double>(theta)));
		float dist = k_Size * 0.1f + centreRadius;
		for (int32_t k = 0; k < 6; ++k)
		{
			if (arm >= 2)
			{
				// the first `count` circles along the arm
				if (k >= count)
				{
					continue;
				}
				const Point2D p {dist * c + at.x, dist * s + at.z};
				Emit(heart, holder, p, armRadius, notify, callback, context);
				dist = k_Size * 0.2f + dist;
				continue;
			}
			// arms 0 and 1, three straight, then three turned (+ for arm 0, - for arm 1)
			Point2D p;
			if (k < 3)
			{
				p = {dist * c + at.x, dist * s + at.z};
				Emit(heart, holder, p, armRadius, notify, callback, context);
				dist = k_Size * 0.2f + dist;
				continue;
			}
			const float turn = arm == 0 ? k_Turn.at(static_cast<size_t>(k - 3)) : -k_Turn.at(static_cast<size_t>(k - 3));
			const double a = static_cast<double>(turn + theta);
			p = {static_cast<float>(std::cos(a)) * dist + at.x, static_cast<float>(std::sin(a)) * dist + at.z};
			Emit(heart, holder, p, armRadius, notify, callback, context);
			if (k == 3)
			{
				dist = k_Size * 0.1f + dist;
			}
			else if (k == 4)
			{
				dist = k_Size * 0.07f + dist;
			}
		}
		// with a creature, the arm's worship site circle. (pending, the creature port)
		theta = theta + k_ArmStep;
	}
}
} // namespace

namespace openblack::ecs::route_plan_world
{
Point2D ToPoint2D(const map_coords::MapCoords& coords)
{
	return {static_cast<float>(coords.x) * k_FixedToMetres, static_cast<float>(coords.z) * k_FixedToMetres};
}

map_coords::MapCoords ToMapCoords(const Point2D& point)
{
	return {static_cast<int32_t>(point.x * k_MetresToFixed), static_cast<int32_t>(point.z * k_MetresToFixed), 0.0f};
}

int32_t IdOf(entt::entity object)
{
	return static_cast<int32_t>(entt::to_integral(object));
}

void OnLandLoaded()
{
	// at game creation the system gets the square check and the special objects' adder; the second is the
	// creature's (pending, the creature port). LandAvoid itself is the central land_avoid (Game::LoadLandscape)
	route_planner::ObstacleGrid::InstallCallbacks(&CheckSquareFunction, nullptr);
}

void CheckSquareFunction(int32_t cellX, int32_t cellZ, route_planner::ObstacleGrid& holder)
{
	// the cell on the map
	if (cellX < 0 || cellX >= 0x200 || cellZ < 0 || cellZ >= 0x200 || !map_coords::InBounds(glm::ivec2(cellX, cellZ)))
	{
		return;
	}
	const bool noPlan = holder.GetPlan() == nullptr;
	// x - 1 .. x + 1 outer, z - 1 .. z + 1 inner: 1, or 6 without a plan, -> a 7.1 m circle at the
	// cell's centre
	for (int32_t cx = cellX - 1; cx <= cellX + 1; ++cx)
	{
		if (cx < 0 || cx >= 0x200)
		{
			continue;
		}
		for (int32_t cz = cellZ - 1; cz <= cellZ + 1; ++cz)
		{
			if (cz < 0 || cz >= 0x200)
			{
				continue;
			}
			const auto value = land_avoid::At(cx, cz);
			if (value == 1 || (noPlan && value == 6))
			{
				const Point2D centre {static_cast<float>(cx) * 10.0f + 5.0f, static_cast<float>(cz) * 10.0f + 5.0f};
				holder.AddObject(-1, centre, k_LandAvoidRadius, 0);
			}
		}
	}
	// the cell's fixed list, then its mobile list; the creature is the plan's (none for a footpath's holder)
	const auto creature = noPlan ? entt::entity(entt::null) : entt::entity(holder.GetPlan()->GetContext());
	const glm::ivec2 cell(cellX, cellZ);
	const auto visit = [&](entt::entity object) {
		const bool avoid = creature == entt::null ? CreatureMustAvoid(object) : CreatureMustAvoid(object, creature);
		if (avoid && holder.IsNotInSquare(IdOf(object), cellX, cellZ))
		{
			AddToRoutePlan(object, holder, 0);
		}
	};
	for (auto object = map_cells::FirstFixed(cell); object != entt::null; object = map_cells::GetMapChild(object, cell))
	{
		visit(object);
	}
	for (const auto object : map_cells::MobileInCell(cell))
	{
		visit(object);
	}
}

bool CreatureMustAvoid(entt::entity object)
{
	const auto& registry = Entities();
	if (!registry.Valid(object))
	{
		return false;
	}
	// 0 for the livings (villagers, animals) and the creature (no creature given), fields,
	// fish farms, food piles, map shields, street lanterns and fragments
	if (registry.AnyOf<Villager, Animal, Creature, Field, FishFarm, MapShield, StreetLantern, Fragment>(object) ||
	    IsPileFood(object))
	{
		return false;
	}
	// a dead tree: 0 without a creature; with one, on fire and not script-controlled (not asked
	// here)
	if (registry.AllOf<DeadTree>(object))
	{
		return false;
	}
	// the football pitch, arena spell icons, prayer sites, town desire flags and field crops
	// also give 0: (pending) openblack tells them apart by type only where it can
	const auto type = map_cells::TypeOf(object);
	if (type == ObjectType::Football || type == ObjectType::ArenaSpellIcon || type == ObjectType::Prayer)
	{
		return false;
	}
	// a big forest: only on fire
	if (type == ObjectType::BigForest)
	{
		return fire::IsOnFire(object);
	}
	// the rest (multi-cell fixed objects, trees, mobile statics, chess pieces, the landscape vortex): 1
	return true;
}

bool CreatureMustAvoid(entt::entity object, entt::entity creature)
{
	const auto& registry = Entities();
	if (!registry.Valid(object))
	{
		return false;
	}
	// a map shield: unless it is the creature's own player's
	if (registry.AllOf<MapShield>(object))
	{
		const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
		const auto player = body != nullptr && body->owner != PlayerNames::NEUTRAL ? std::optional(body->owner) : std::nullopt;
		return magic::map_shield::CreatureMustAvoid(object, creature, player);
	}
	// the livings, the creatures, fields, fish farms, food piles, street lanterns and fragments: 0, as with no creature
	if (registry.AnyOf<Villager, Animal, Creature, Field, FishFarm, StreetLantern, Fragment>(object) || IsPileFood(object))
	{
		return false;
	}
	// a dead tree: on fire and not controlled by a script
	if (registry.AllOf<DeadTree>(object))
	{
		return fire::IsOnFire(object) && !script_held::IsControlledByScript(object);
	}
	const auto type = map_cells::TypeOf(object);
	if (type == ObjectType::Football || type == ObjectType::ArenaSpellIcon || type == ObjectType::Prayer)
	{
		return false;
	}
	if (type == ObjectType::BigForest)
	{
		return fire::IsOnFire(object);
	}
	return true;
}

void AddToRoutePlan(entt::entity object, route_planner::ObstacleGrid& holder, int32_t notify, CircleFn callback, void* context)
{
	const auto& registry = Entities();
	const auto type = map_cells::TypeOf(object);
	// trees, mobile objects and script highlights: SimpleAddToRoutePlan
	if (registry.AnyOf<Tree, ScriptHighlight>(object) || type == ObjectType::MobileObject)
	{
		SimpleAddToRoutePlan(object, holder, notify, callback, context);
		return;
	}
	// a mobile static: a fence -> ObjectAddToRoutePlan, else Simple
	if (type == ObjectType::MobileStatic)
	{
		if (physics::from_hand::IsFence(object))
		{
			ObjectAddToRoutePlan(object, holder, notify, callback, context);
		}
		else
		{
			SimpleAddToRoutePlan(object, holder, notify, callback, context);
		}
		return;
	}
	// the citadel heart: its centre circle and seven arms
	if (registry.AllOf<CitadelHeart>(object))
	{
		CitadelHeartAddToRoutePlan(object, holder, notify, callback, context);
		return;
	}
	ObjectAddToRoutePlan(object, holder, notify, callback, context);
}
} // namespace openblack::ecs::route_plan_world
