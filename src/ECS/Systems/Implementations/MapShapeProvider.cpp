/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MapShapeProvider.h"

#include <glm/vec2.hpp>

#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// Added to the scaled half diagonal of the mesh to give the reach of the shape's cells
constexpr float k_DescriptorReachAdd = 1.0f;
} // namespace

bool MapShapeProvider::MeshShape(entt::entity object, map_collide::Shape& shape, float& reach) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(object);
	const auto* mesh = registry.TryGet<const Mesh>(object);
	const float scale = object::GetScale(object);
	if (mesh == nullptr || !map_collide::FromMesh(mesh->id, glm::vec2(transform.position.x, transform.position.z),
	                                              map_cells::detail::YAngleOf(transform.rotation), scale, shape))
	{
		return false;
	}
	reach = scale * object::MeshHalfDiagonal(mesh->id) + k_DescriptorReachAdd;
	return true;
}
