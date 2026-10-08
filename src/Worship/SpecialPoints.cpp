/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpecialPoints.h"

#include <cmath>

#include <glm/vec2.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::optional<worship::SpecialPoint> worship::GetSpecialPoint(entt::entity entity, int index)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || index < 0)
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	if (transform == nullptr || mesh == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	const auto& metrics = meshes.Handle(mesh->id)->GetExtraMetrics();
	if (static_cast<size_t>(index) >= metrics.size())
	{
		return std::nullopt;
	}
	const auto& metric = metrics[static_cast<size_t>(index)];
	SpecialPoint point;
	point.position = transform->position + transform->rotation * (glm::vec3(metric[3]) * transform->scale);
	point.rotation = transform->rotation * glm::mat3(metric);
	point.yAngle = YAngleOf(point.rotation);
	return point;
}

size_t worship::ExtraMetricCount(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.Valid(entity) ? registry.TryGet<const Mesh>(entity) : nullptr;
	if (mesh == nullptr || !Locator::resources::has_value())
	{
		return 0;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(mesh->id) ? meshes.Handle(mesh->id)->GetExtraMetrics().size() : 0;
}

float worship::YAngleOf(const glm::mat3& rotation)
{
	// eulerAngleY(a) = [[cos a, 0, -sin a], [0, 1, 0], [sin a, 0, cos a]] (columns); the original's angle is -a
	return -std::atan2(-rotation[0][2], rotation[0][0]);
}

float worship::GroundAt(const glm::vec3& point)
{
	// (the placeholder UnloadedIsland throws, and has no materials)
	if (!Locator::terrainSystem::has_value() || Locator::terrainSystem::value().GetMaterialInfo().empty())
	{
		return 0.0f;
	}
	return Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z));
}
