/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerBuildingSites.h"

#include <entt/entity/entity.hpp>

#include "ECS/Abodes.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Town/BuildingSites.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

namespace bs = openblack::ecs::building_sites;

bool VillagerBuildingSites::IsBuildingHappening(entt::entity town) const
{
	return bs::IsBuildingHappening(town);
}

entt::entity VillagerBuildingSites::GetBestBuildingSite(entt::entity town, const map_coords::MapCoords& pos,
                                                        bool includeFull) const
{
	return bs::GetBestBuildingSite(town, pos, includeFull);
}

entt::entity VillagerBuildingSites::GetBestRepairBuildingSite(entt::entity town) const
{
	return bs::GetBestRepairBuildingSite(town);
}

bool VillagerBuildingSites::IsBuildingSiteValid(entt::entity town, entt::entity site) const
{
	return site != entt::null && bs::IsBuildingSiteValid(town, site);
}

entt::entity VillagerBuildingSites::GetBuildingSiteInList(entt::entity town, entt::entity building) const
{
	return bs::GetBuildingSiteInList(town, building);
}

entt::entity VillagerBuildingSites::AddBuildingSite(entt::entity town, entt::entity building)
{
	return bs::AddBuildingSite(town, building);
}

bool VillagerBuildingSites::RequestBestPlanned(entt::entity town)
{
	return bs::RequestBestPlanned(town);
}

bool VillagerBuildingSites::RequestANewAbode(entt::entity town)
{
	// The builders always ask for living quarters; the type is not used by the request
	return bs::RequestANewAbode(town, AbodeType::LivingQuarters);
}

void VillagerBuildingSites::AddWoodUsedForBuilding(entt::entity town, uint32_t wood)
{
	bs::AddWoodUsedForBuilding(town, wood);
}

entt::entity VillagerBuildingSites::GetBuilding(entt::entity site) const
{
	return bs::GetBuilding(site);
}

bool VillagerBuildingSites::NeedsBuilders(entt::entity site) const
{
	return bs::NeedsBuilders(site);
}

bool VillagerBuildingSites::IsBuilder(entt::entity site, entt::entity villager) const
{
	return bs::IsBuilder(site, villager);
}

int32_t VillagerBuildingSites::GetBuilderCount(entt::entity site) const
{
	return bs::GetBuilderCount(site);
}

float VillagerBuildingSites::GetClearAreaRadius(entt::entity site) const
{
	return bs::GetClearAreaRadius(site);
}

float VillagerBuildingSites::GetWoodValue(entt::entity site) const
{
	return bs::GetWoodValue(site);
}

bool VillagerBuildingSites::ShouldIGetWood(entt::entity site, entt::entity villager,
                                           const std::function<map_coords::MapCoords()>& resourceDropoffPos) const
{
	return bs::ShouldIGetWood(site, villager, resourceDropoffPos);
}

uint32_t VillagerBuildingSites::GetResource(entt::entity site, ResourceType type) const
{
	return bs::GetResource(site, type);
}

uint32_t VillagerBuildingSites::AddResource(entt::entity site, ResourceType type, uint32_t amount,
                                            const map_coords::MapCoords* pos)
{
	return bs::AddResource(site, type, amount, pos);
}

uint32_t VillagerBuildingSites::RemoveResource(entt::entity site, ResourceType type, uint32_t amount)
{
	return bs::RemoveResource(site, type, amount);
}

void VillagerBuildingSites::BuildBy(entt::entity site, float amount)
{
	bs::BuildBy(site, amount);
}

bool VillagerBuildingSites::IsAvailable(entt::entity site) const
{
	return bs::IsAvailable(site);
}

map_coords::MapCoords VillagerBuildingSites::GetRandomBuildPos(entt::entity site, entt::entity villager, int32_t& index) const
{
	return bs::GetRandomBuildPos(site, villager, index);
}

map_coords::MapCoords VillagerBuildingSites::GetNextPosFromIndex(entt::entity site, int32_t& index) const
{
	return bs::GetNextPosFromIndex(site, index);
}

std::optional<map_coords::MapCoords> VillagerBuildingSites::GetBuildPos(entt::entity site, int32_t index) const
{
	return bs::GetBuildPos(site, index);
}

void VillagerBuildingSites::AddBuilder(entt::entity site, entt::entity villager)
{
	bs::AddBuilder(site, villager);
}

void VillagerBuildingSites::RemoveBuilder(entt::entity site, entt::entity villager)
{
	bs::RemoveBuilder(site, villager);
}

bool VillagerBuildingSites::IsBuilt(entt::entity building) const
{
	return abodes::IsBuilt(building);
}

bool VillagerBuildingSites::IsRepaired(entt::entity building) const
{
	return abodes::IsRepaired(building);
}

bool VillagerBuildingSites::IsTouching(entt::entity villager, entt::entity building, float margin) const
{
	return object::IsTouching(villager, building, margin);
}

float VillagerBuildingSites::LandAlignmentAt(const glm::vec3& position) const
{
	return effects::alignment::LandAlignmentAt(position);
}
