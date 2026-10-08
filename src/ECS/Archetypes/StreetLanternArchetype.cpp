/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StreetLanternArchetype.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The map cell (the high words of the 1/6553.6 fixed point, 10 units a cell)
glm::ivec2 MapCell(const glm::vec3& position)
{
	return map_coords::CellOf(position);
}

/// The mobile statics (OBJECT_TYPE_MOBILE_STATIC 0x1C) of the position's map cell (type 28 counts as fixed: the cell's
/// fixed list from its head, ecs::map_cells), each one's GetDistanceInMetres (the table hypotenuse on the two map
/// coordinates) against 0.5 m. Every GMobileStaticInfo has that type (info.dat), so it is anything made from one:
/// rocks and mobile statics, bonfires, lanterns, dead trees; a multi-cell one in every cell of its footprint
bool MobileStaticWithinHalfMetre(const glm::vec3& position)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto cell = MapCell(position);
	for (auto entity = ecs::map_cells::FindType(cell, ObjectType::MobileStatic); entity != entt::null;
	     entity = ecs::map_cells::FindType(cell, ObjectType::MobileStatic, entity))
	{
		if (gutils::GetDistanceInMetres(registry.Get<const Transform>(entity).position, position) < 0.5f)
		{
			return true;
		}
	}
	return false;
}
} // namespace

entt::entity StreetLanternArchetype::Create(const glm::vec3& position, MobileStaticInfo info)
{
	// nothing if another MobileStatic is within 0.5 m
	if (MobileStaticWithinHalfMetre(position))
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// country = not the street lantern info: mesh 148 (MSH_B_CAMPFIRE) for a country lantern, else 398
	// (MSH_O_TOWNLIGHT), at (x, ground altitude + y, z), angle 0, scale 1, and its light. It also creates the sound tag
	// of sample 0x93 for either kind of lantern: the looping G_Lantern_01 that audio::lantern_sounds::ProcessTurn starts
	// at night.
	const bool country = info != MobileStaticInfo::StreetLantern;
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	const auto resourceId = resources::HashIdentifier(country ? MeshId::BuildingCampfire : MeshId::ObjectTownLight);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	registry.Assign<StreetLantern>(entity, country);
	registry.Assign<LanternLight>(entity, static_cast<uint8_t>(country ? 1 : 0));
	// into the map: a street lantern is an object of type 28 (counted as fixed), at the tail of its cell's fixed list
	ecs::map_cells::InsertMapObject(entity);
	return entity;
}
