/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileStaticArchetype.h"

#include <cmath>

#include <glm/gtx/euler_angles.hpp>

#include "3D/ObjectMatrix.h"
#include "AbodeArchetype.h"
#include "BonfireArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity MobileStaticArchetype::Create(const glm::vec3& position, MobileStaticInfo type, float altitude,
                                           float xAngleRadians, float yAngleRadians, float zAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(type));

	glm::vec3 offset(0.0f, altitude, 0.0f);

	// position (x, ground altitude + altitude, z)
	registry.Assign<Transform>(entity, position + offset, XYZRotation(xAngleRadians, yAngleRadians, zAngleRadians),
	                           glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<MobileStatic>(entity, type);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	// into the map: at the head of every cell of its collide footprint
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}

glm::mat3 MobileStaticArchetype::XYZRotation(float xAngleRadians, float yAngleRadians, float zAngleRadians)
{
	return affine::RotationYXZ(yAngleRadians, xAngleRadians, zAngleRadians);
}

entt::entity MobileStaticArchetype::CreateFromInfo(const glm::vec3& position, MobileStaticInfo type, float altitude,
                                                   float yAngleRadians, float scale)
{
	switch (type)
	{
	case MobileStaticInfo::Bonfire: // temperature 100; the bonfire ignores it
		return BonfireArchetype::Create(position + glm::vec3(0.0f, altitude, 0.0f), yAngleRadians, scale);
	case MobileStaticInfo::SingingStoneBase: // nothing
		return entt::null;
	default: // a Rock or a MobileStatic, both drawn the same here
		return Create(position, type, altitude, 0.0f, yAngleRadians, 0.0f, scale);
	}
}

entt::entity MobileStaticArchetype::CreateWithXYZAngles(const glm::vec3& position, MobileStaticInfo type, float altitude,
                                                        float xAngleRadians, float yAngleRadians, float zAngleRadians,
                                                        float scale)
{
	switch (type)
	{
	case MobileStaticInfo::StreetLantern: // nothing (lanterns come from CREATE_STREET_LANTERN)
		return entt::null;
	case MobileStaticInfo::Bonfire:
	{
		// CreateFromInfo -> a bonfire (temperature 100, Y angle, scale), then the angles and the scale set
		const auto entity = CreateFromInfo(position, type, altitude, yAngleRadians, scale);
		auto& transform = Locator::entitiesRegistry::value().Get<Transform>(entity);
		transform.rotation = XYZRotation(xAngleRadians, yAngleRadians, zAngleRadians);
		transform.scale = glm::vec3(scale);
		// removed from and inserted into the map again when in it
		ecs::map_cells::OnAnglesOrScaleChanged(entity);
		return entity;
	}
	default:
		// info 6: a base-only object with the info's mesh; the rest a Rock or a MobileStatic. Setting the angles and the
		// scale afterwards gives both the three angles and the scale.
		return Create(position, type, altitude, xAngleRadians, yAngleRadians, zAngleRadians, scale);
	}
}
