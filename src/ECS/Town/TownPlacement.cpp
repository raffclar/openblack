/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownPlacement.h"

#include <optional>

#include <glm/vec2.hpp>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Town/TownStats.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

// Where a town may put a fixed building (TownPlacement.h)

namespace openblack::ecs::town_placement
{
using namespace components;

namespace
{
/// SetTownArea's start values: min INT32_MAX, max 0
constexpr int32_t k_AreaMinStart = 0x7FFFFFFF;
constexpr int32_t k_AreaMaxStart = 0;
/// The collide descriptor's reach = scale x mesh half diagonal + 1 (as map_cells' descriptor, MapCells.cpp)
constexpr float k_DescriptorReachAdd = 1.0f;

/// The object's radius (its 2D radius) around its position, in metres, widening the rectangle
void ExtendTownAreaBy(Town& town, entt::entity object, float r)
{
	// The object's x and z as they are: no ground height is asked
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return;
	}
	const map_coords::MapCoords at {map_coords::ToFixed(transform->position.x), map_coords::ToFixed(transform->position.z),
	                                0.0f};
	// On x: the position in metres (map_coords::ToMetres' one rounding) - r; below the
	// minimum in metres -> the new minimum, back to fixed point
	const auto widen = [r](int32_t position, int32_t& minimum, int32_t& maximum) {
		const float low = map_coords::ToMetres(position) - r;
		if (low < map_coords::ToMetres(minimum))
		{
			minimum = map_coords::ToFixedGUtils(low);
		}
		// position + r above the maximum (skipped when <=)
		const float high = map_coords::ToMetres(position) + r;
		if (high > map_coords::ToMetres(maximum))
		{
			maximum = map_coords::ToFixedGUtils(high);
		}
	};
	widen(at.x, town.areaMin.x, town.areaMax.x);
	// The same on z
	widen(at.z, town.areaMin.y, town.areaMax.y);
}
} // namespace

void ExtendTownArea(entt::entity town, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* t = registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr; t != nullptr)
	{
		ExtendTownAreaBy(*t, object, object::GetRadius(object));
	}
}

void SetTownArea(entt::entity town, entt::entity newcomer)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* t = registry.Valid(town) ? registry.TryGet<Town>(town) : nullptr;
	if (t == nullptr)
	{
		return;
	}
	t->areaMin = {k_AreaMinStart, k_AreaMinStart};
	t->areaMax = {k_AreaMaxStart, k_AreaMaxStart};
	// The abodes, then the fields, which openblack keeps as abodes of the town: AbodesOf has them once (a second pass
	// over them would not move the rectangle)
	for (const auto abode : town_stats::AbodesOf(town))
	{
		// Joining the town happens while the abode is created, before its 3D object exists: its radius is 0, the
		// newcomer is a point
		ExtendTownAreaBy(*t, abode, abode == newcomer ? 0.0f : object::GetRadius(abode));
	}
	// (not ported) a town camera point, from the town centre's position (else the storage pit's, else the rectangle's
	// centre) at a distance from the rectangle's size. Nothing but the town's save / load reads it
}

map_coords::MapCoords GetTownAreaCentre(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* t = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (t == nullptr)
	{
		return {};
	}
	// Max and min in metres, summed, times a half. (approximate) the original's extended-precision sum and product
	// in float
	const float x = (map_coords::ToMetres(t->areaMax.x) + map_coords::ToMetres(t->areaMin.x)) * 0.5f;
	const float z = (map_coords::ToMetres(t->areaMax.y) + map_coords::ToMetres(t->areaMin.y)) * 0.5f;
	// Back to fixed point, altitude 0
	return {map_coords::ToFixedGUtils(x), map_coords::ToFixedGUtils(z), 0.0f};
}

bool PositionCellIsBuildable(const map_coords::MapCoords& pos, bool refuseCoast)
{
	// The POSITION's landscape cell on the map, a block, not water. sea_cells::IsWater: no landscape cell is water (1)
	// too
	if (!map_coords::InBounds(pos) || sea_cells::IsWater(map_coords::ToWorld(pos)))
	{
		return false;
	}
	// The object check only: a land cell on the coast line -> 0
	return !(refuseCoast && sea_cells::IsCoastal(map_coords::ToWorld(pos)));
}

namespace
{
/// The position check (refuseCoast false, no `under`) and the object check (refuseCoast true, `under` skipped) share
/// this loop
bool SuitableForFixed(const map_coords::MapCoords& pos, entt::id_type meshId, float yAngle, float scale, bool refuseCoast,
                      entt::entity under)
{
	// The original fills a temporary object with the mesh, angle and scale at the position; openblack builds its
	// collide shape directly (map_collide::FromMesh). No mesh: 0
	map_collide::Shape shape;
	if (!map_collide::FromMesh(meshId, map_coords::ToMetres(pos), yAngle, scale, shape))
	{
		return false;
	}
	const float reach = scale * object::MeshHalfDiagonal(meshId) + k_DescriptorReachAdd;
	// R = scale x max(half x, half z) of the mesh, the inline 2D radius
	const float radius = object::MeshRadius2D(meshId, scale);
	// The cell walk stops at a marked cell off the map and the loop then ends with 1: DescriptorCells is cut there the
	// same way; the cell lookup can never fail (dead test)
	for (const auto cell : map_cells::DescriptorCells(shape, reach))
	{
		// the POSITION's landscape cell, re-tested for every descriptor cell
		if (!PositionCellIsBuildable(pos, refuseCoast))
		{
			return false;
		}
		// The cell's fixed list, unfiltered; a multi-map fixed object that is solid to a new abode is exactly the
		// MultiMapFixed class
		for (auto obj = map_cells::FirstFixed(cell); obj != entt::null; obj = map_cells::GetMapChild(obj, cell))
		{
			// The object check also skips `under` (the building under the touching scaffold)
			if (!map_cells::IsMultiCellStaticClass(obj) || (under != entt::null && obj == under))
			{
				continue;
			}
			// The distance in metres and the object's 2D radius (a field or fish farm 5.0); reject when
			// R_obj + R > d: d == R_obj + R is accepted
			const float d = gutils::GetDistanceInMetres(pos, object::MapCoordsOf(obj));
			if (object::Get2DRadius(obj) + radius > d)
			{
				return false;
			}
		}
	}
	return true;
}
} // namespace

bool IsSuitableForFixed(const map_coords::MapCoords& pos, entt::id_type meshId, float yAngle, float scale)
{
	return SuitableForFixed(pos, meshId, yAngle, scale, false, entt::null);
}

bool IsSuitableForFixedObject(const map_coords::MapCoords& pos, entt::id_type meshId, float yAngle, float scale,
                              entt::entity under)
{
	// The object's own placement (the ground + altitude) is the shape's; R the same scale x max(half x, half z)
	return SuitableForFixed(pos, meshId, yAngle, scale, true, under);
}

uint32_t IsSuitableForFixedAbodeInTown(const map_coords::MapCoords& pos, entt::id_type meshId, entt::entity town, float yAngle,
                                       float scale)
{
	// Only with a town
	if (town != entt::null)
	{
		// The nearest OTHER town of any tribe by the octagonal cell distance and its rectangle's code
		// (map_cells::GetNearestTownCells; the rectangles as they are kept)
		// code 1 -> k_InsideOtherTown, which IsOkToCreateAtPos turns into "accepted" (literal: no fixed tests)
		if (map_cells::GetNearestTownCells(pos, town, std::nullopt).code == 1)
		{
			return k_InsideOtherTown;
		}
	}
	return IsSuitableForFixed(pos, meshId, yAngle, scale) ? 1u : 0u;
}

bool IsOkToCreateAtPos(const GAbodeInfo& info, const map_coords::MapCoords& pos, float yAngle, float scale, entt::entity town)
{
	// The info's mesh; any non-zero result is accepted
	return IsSuitableForFixedAbodeInTown(pos, resources::HashIdentifier(info.meshId), town, yAngle, scale) != 0;
}
} // namespace openblack::ecs::town_placement
