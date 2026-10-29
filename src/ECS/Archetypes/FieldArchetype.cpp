/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FieldArchetype.h"

#include "AbodeArchetype.h"
#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Swayable.h"
#include "ECS/Components/Town.h"
#include "ECS/Registry.h"
#include "ECS/Systems/VegetationInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using openblack::ecs::systems::VegetationInterface;

namespace
{
/// The crop follows one of the sways of the trees. The sway comes from the place of the field in memory, which is
/// almost random.
void GiveSway(ecs::Registry& registry, entt::entity entity)
{
	registry.Assign<Swayable>(
	    entity, static_cast<uint8_t>(Locator::rng::value().NextValue<uint32_t>(0, VegetationInterface::k_SwayCount - 1)));
}
} // namespace

entt::entity FieldArchetype::Create(int townId, const glm::vec3& position, FieldTypeInfo type, float yAngleRadians)
{
	auto& registry = Locator::entitiesRegistry::value();

	auto townTribe = registry.Get<Tribe>(registry.Context().towns[townId]);
	auto abodeInfo = GAbodeInfo::Find(townTribe, AbodeNumber::Field);

	auto entity = AbodeArchetype::Create(townId, position, abodeInfo, yAngleRadians, 1.0f, 0, 0);
	// A new field is bare: the town's farmers sow it
	registry.Assign<Field>(entity, townId, type, field_crop::Crop {},
	                       Locator::gameRandom::value().GameRand(field_crop::k_TurnsPerGrowth), field_crop::Settle {});
	GiveSway(registry, entity);

	return entity;
}

entt::entity FieldArchetype::CreateLoaded(int townId, const glm::vec3& position, AbodeInfo row, FieldTypeInfo type,
                                          float yAngleRadians, float scale, float built, const field_crop::Crop& crop,
                                          uint32_t growthTurn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = AbodeArchetype::Create(static_cast<uint32_t>(townId), position, row, yAngleRadians, scale, 0, 0, built);
	if (entity == entt::null)
	{
		return entity;
	}
	registry.Assign<Field>(entity, townId, type, crop, growthTurn, field_crop::Settle {});
	GiveSway(registry, entity);
	return entity;
}
