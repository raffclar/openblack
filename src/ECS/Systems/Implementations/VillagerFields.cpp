/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerFields.h"

#include "ECS/Fields.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;
namespace map_coords = openblack::map_coords;

std::vector<entt::entity> VillagerFields::TownFields(entt::entity town) const
{
	return fields::TownFields(town);
}

float VillagerFields::GetDesireToBeFarmed(entt::entity field) const
{
	return fields::GetDesireToBeFarmed(field);
}

int VillagerFields::GetFieldActivity(entt::entity field) const
{
	return fields::GetFieldActivity(field);
}

map_coords::MapCoords VillagerFields::GetArrivePos(entt::entity field) const
{
	return fields::GetArrivePos(field);
}

map_coords::MapCoords VillagerFields::RandomFarmPoint(entt::entity field) const
{
	return fields::RandomFarmPoint(field);
}

bool VillagerFields::RipeFarmPoint(entt::entity field, map_coords::MapCoords& out) const
{
	return fields::RipeFarmPoint(field, out);
}

bool VillagerFields::PlantCrop(entt::entity field)
{
	return fields::PlantCrop(field);
}

bool VillagerFields::IsStillSowing(entt::entity field) const
{
	return fields::IsStillSowing(field);
}

int32_t VillagerFields::RemoveFood(entt::entity field, float amount)
{
	return fields::RemoveFood(field, amount);
}

void VillagerFields::AddFarmer(entt::entity field, entt::entity villager)
{
	fields::AddFarmer(field, villager);
}

void VillagerFields::RemoveFarmer(entt::entity field, entt::entity villager)
{
	fields::RemoveFarmer(field, villager);
}

bool VillagerFields::IsField(entt::entity thing) const
{
	return fields::IsField(thing);
}
