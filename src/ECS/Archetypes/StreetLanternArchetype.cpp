/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StreetLanternArchetype.h"

#include <vector>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/StreetLanternRules.h"
#include "ECS/Systems/VillageLightSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundId.h"
#include "Utils.h"
#include "VillageLightArchetype.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

entt::entity StreetLanternArchetype::Create(const glm::vec3& position, MobileStaticInfo info)
{
	auto& registry = Locator::entitiesRegistry::value();
	// No lantern is made too near a thing of the statics' kind in its map cell
	if (Locator::entitiesMap::has_value())
	{
		std::vector<glm::vec3> statics;
		for (const auto other : Locator::entitiesMap::value().GetFixedInGridCell(position))
		{
			const auto* row = registry.Valid(other) ? world_objects::EffectInfoOf(other) : nullptr;
			const auto* transform = registry.TryGet<const Transform>(other);
			if (row != nullptr && row->type == ObjectType::MobileStatic && transform != nullptr)
			{
				statics.push_back(transform->position);
			}
		}
		if (street_lanterns::Refused(position, statics))
		{
			return entt::null;
		}
	}
	const auto entity = registry.Create();
	// It takes its place in the order of the things made
	TakeThingMadePlace();
	// It stands as it was made, unturned
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	const bool country = info != MobileStaticInfo::StreetLantern;
	const auto resourceId = resources::HashIdentifier(country ? MeshId::BuildingCampfire : MeshId::ObjectTownLight);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	registry.Assign<StreetLantern>(entity, StreetLantern {.country = country, .info = info});
	// It crackles from its top while it is dark
	float height = 0.0f;
	if (const auto& meshes = Locator::resources::value().GetMeshes(); meshes.Contains(resourceId))
	{
		height = meshes.Handle(resourceId)->GetBoundingBox().Size().y;
	}
	// A computer that does not draw the world has no dark, and its lanterns do not crackle
	const bool dark = Locator::villageLightSystem::has_value() && Locator::villageLightSystem::value().IsDark();
	registry.Assign<SoundTag>(entity, static_cast<entt::id_type>(audio::SoundId::G_Lantern_01), glm::vec3(0.0f, height, 0.0f),
	                          dark);
	// Its light lights the village around it at night
	VillageLightArchetype::Create(position, country ? VillageLight::Kind::Country : VillageLight::Kind::Town, entity);
	return entity;
}
