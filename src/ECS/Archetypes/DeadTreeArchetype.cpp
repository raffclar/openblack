/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DeadTreeArchetype.h"

#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MobileStaticArchetype.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity DeadTreeArchetype::Create(const glm::vec3& position, TreeInfo type, float life, float xAngleRadians,
                                       float yAngleRadians, float zAngleRadians)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(type));

	// A rock (the dead tree's mobile static info, angle 0, scale 1) with the given life; after its creation, the x, y,
	// z angles and scale 1 give the mobile static matrix. The mesh is the tree type's normal mesh (no override here).
	// No Tree, no forest: it is a mobile object that can be picked up (the hand's thrown tree is the same kind).
	registry.Assign<Transform>(
	    entity, position, MobileStaticArchetype::XYZRotation(xAngleRadians, yAngleRadians, zAngleRadians), glm::vec3(1.0f));
	registry.Assign<Mobile>(entity);
	registry.Assign<DeadTree>(entity, type);
	registry.Assign<Life>(entity, life);
	const auto resourceId = resources::HashIdentifier(info.normal);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	// into the map cells. Setting the angles after it removes and inserts again (in the map): the same as inserting
	// with the angles
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
