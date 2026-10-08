/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FeatureArchetype.h"

#include <BulletCollision/CollisionShapes/btConvexShape.h>
#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity FeatureArchetype::Create(const glm::vec3& position, FeatureInfo type, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().feature.at(static_cast<size_t>(type));

	const auto& transform = registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(scale));
	const auto [point, radius] = GetFixedObstacleBoundingCircle(info.meshId, transform);
	registry.Assign<Fixed>(entity, point, radius);
	registry.Assign<Feature>(entity, type);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));

	auto l3dMesh = Locator::resources::value().GetMeshes().Handle(resourceId);
	if (l3dMesh->HasPhysicsMesh())
	{
		auto& shape = l3dMesh->GetPhysicsMesh();
		// (openblack) a static body (mass 0) for the ray casts only: the original has no rigid-body world, and
		// L3DMesh's mass is a placeholder 1. Placed and turned as the Transform (AngleY above)
		const btVector3 bodyInertia(0, 0, 0);

		btTransform startTransform;
		startTransform.setIdentity();
		startTransform.setOrigin(btVector3(transform.position.x, transform.position.y, transform.position.z));
		const auto& r = transform.rotation;
		startTransform.setBasis(btMatrix3x3(r[0][0], r[1][0], r[2][0], r[0][1], r[1][1], r[2][1], r[0][2], r[1][2], r[2][2]));

		btRigidBody::btRigidBodyConstructionInfo rbInfo(0.0f, nullptr, &shape, bodyInertia);

		registry.Assign<RigidBody>(entity, rbInfo, startTransform);
	}
	// into the map cells it covers
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
