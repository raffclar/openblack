/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileObjectArchetype.h"

#include <stdexcept>

#include "AbodeArchetype.h"
#include "Common/X87Angle.h"
#include "ECS/Components/CreedObject.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/NoLandShadow.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileObjectClassRules.h"
#include "ECS/MobileObjectRecord.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Sound/SoundId.h"
#include "Utils.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity MobileObjectArchetype::Create(const glm::vec3& position, MobileObjectInfo type, float yAngleRadians, float scale)
{
	return Create(position, MobileObject {.type = type}, yAngleRadians, scale);
}

entt::entity MobileObjectArchetype::Create(const glm::vec3& position, const MobileObject& kind, float yAngleRadians,
                                           float scale)
{
	const auto* record = ecs::mobile_object_record::Of(Locator::infoConstants::value(), kind);
	if (record == nullptr)
	{
		throw std::out_of_range("The loose object has no record in the info tables");
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// It takes its place in the order of the things made
	TakeThingMadePlace();

	const auto& info = *record;

	registry.Assign<Transform>(entity, position, x87::GameTurnMatrix(0.0f, yAngleRadians, 0.0f), glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<MobileObject>(entity, kind);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));

	// The thing keeps the angles it got
	ecs::placed_turn::KeepAngles(Locator::entitiesRegistry::value(), entity, glm::vec3(0.0f, yAngleRadians, 0.0f));

	switch (ecs::mobile_object_record::ClassOf(Locator::infoConstants::value(), kind))
	{
	case ecs::mobile_object_record::Class::Creed:
		// A creed shows only its glow, so it casts no shadow into the land. It hums from the start, and the hum loops.
		registry.Assign<CreedObject>(entity);
		registry.Assign<NoLandShadow>(entity);
		registry.Assign<SoundTag>(entity, static_cast<entt::id_type>(audio::SoundId::G_Creed_01), glm::vec3(0.0f), true);
		break;
	case ecs::mobile_object_record::Class::Crop:
		// The size asked for is the crop's life. A crop of no life goes at once. Otherwise its size comes from its life.
		if (ecs::mobile_object_class::CropGoes(scale))
		{
			registry.Destroy(entity);
			return entt::null;
		}
		registry.Assign<ObjectLife>(entity, ObjectLife {.life = scale});
		registry.Get<Transform>(entity).scale = glm::vec3(ecs::mobile_object_class::CropScale(scale, info.finalScale));
		break;
	default:
		break;
	}
	return entity;
}
