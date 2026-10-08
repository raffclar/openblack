/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BigForestArchetype.h"

#include <algorithm>

#include <glm/gtx/euler_angles.hpp>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity BigForestArchetype::Create(const glm::vec3& position, BigForestInfo type, [[maybe_unused]] uint32_t unknown,
                                        float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().bigForest.at(static_cast<size_t>(type));

	const auto& transform = registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(scale));
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);
	registry.Assign<Forest>(entity);
	// the wood it holds: its info's woodValue at its scale (taking wood rescales it to wood / woodValue)
	auto& forest = registry.Assign<BigForest>(entity);
	forest.woodValue = static_cast<float>(std::max<uint32_t>(info.woodValue, 1));
	forest.wood = forest.woodValue * scale;
	// a big forest owns a forest, which points back at the big forest
	forest.forestId = ecs::CreateForest(0, position);
	ecs::SetForestBigForest(forest.forestId, entity);
	registry.Assign<MorphWithTerrain>(entity);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	// on creation: InsertMapObject as a MultiMapFixed, the head of the fixed list of every cell of its shape
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
