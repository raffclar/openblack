/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerFishFarms.h"

#include <cstddef>

#include "ECS/Components/PlayerMagic.h"
#include "ECS/FishFarms.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Weather/Calendar.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

namespace
{
/// The catch always uses the Norse tribe's power, whatever the villager's tribe
constexpr size_t k_FishingTribe = 7;
/// The tribal power of a villager without a player
constexpr float k_NoPlayerTribalPower = 1.0f;
} // namespace

std::vector<entt::entity> VillagerFishFarms::TownFishFarms(entt::entity town) const
{
	return fish_farms::TownFishFarms(town);
}

int32_t VillagerFishFarms::Score(entt::entity farm) const
{
	return fish_farms::Score(farm);
}

map_coords::MapCoords VillagerFishFarms::GetArrivePos(entt::entity farm) const
{
	return fish_farms::GetArrivePos(farm);
}

map_coords::MapCoords VillagerFishFarms::FishingSpot(entt::entity farm) const
{
	return fish_farms::FishingSpot(farm);
}

uint32_t VillagerFishFarms::FishermanCount(entt::entity farm) const
{
	return fish_farms::FishermanCount(farm);
}

bool VillagerFishFarms::HasFisherman(entt::entity farm, entt::entity villager) const
{
	return fish_farms::HasFisherman(farm, villager);
}

void VillagerFishFarms::AddFisherman(entt::entity farm, entt::entity villager)
{
	fish_farms::AddFisherman(farm, villager);
}

void VillagerFishFarms::RemoveFisherman(entt::entity farm, entt::entity villager)
{
	fish_farms::RemoveFisherman(farm, villager);
}

bool VillagerFishFarms::IsAvailable(entt::entity farm) const
{
	// A farm is available while it is a fish farm not being deleted
	return fish_farms::IsFishFarm(farm);
}

uint32_t VillagerFishFarms::Season() const
{
	return weather::calendar::GetSeason(villager::CurrentTurn());
}

float VillagerFishFarms::TribalPower(entt::entity villager) const
{
	const auto player = villager::GetPlayerOf(villager);
	return player.has_value() ? magic::players::MagicOf(*player).tribalPower.at(k_FishingTribe) : k_NoPlayerTribalPower;
}
