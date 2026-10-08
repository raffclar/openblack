/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerInteract.h"

#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Fields.h"
#include "ECS/FishFarms.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerFarmer.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs::villager
{
namespace
{
using components::LivingAction;
using components::Town;
using components::Villager;

/// The town's owner; none without a town (the callers then give 0)
std::optional<PlayerNames> PlayerOfTown(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return std::nullopt;
	}
	return registry.Get<const Town>(town).owner;
}

/// The object the villager inspects
entt::entity TargetOf(entt::entity villager)
{
	const auto* v = Locator::entitiesRegistry::value().TryGet<const Villager>(villager);
	return v != nullptr ? v->targetThing : entt::null;
}
} // namespace

uint32_t CheckInteractWithField(LivingAction& action)
{
	const auto villager = Locator::entitiesRegistry::value().ToEntity(action);
	// the target as a field. (openblack, guard) the original uses it without a null test: not a field -> 0
	const auto field = TargetOf(villager);
	if (!fields::IsField(field))
	{
		return 0;
	}
	// the field's player (its town's) == the villager's
	if (PlayerOfTown(fields::TownOf(field)) != GetPlayerOf(villager))
	{
		return 0;
	}
	// SetFarmerGotoField(field, 1) == 1 (the int is never read)
	if (SetFarmerGotoField(villager, field) != 1)
	{
		return 0;
	}
	// with a player, the player's creature may come to share the town's need for food (0.5, at the villager)
	creature_mimic::EmpathiseWithTownDesire(GetPlayerOf(villager), TownDesireInfo::ForFood, 0.5f, villager);
	return 1;
}

uint32_t CheckInteractWithFishFarm(LivingAction& action)
{
	const auto villager = Locator::entitiesRegistry::value().ToEntity(action);
	// the target as a fish farm. (openblack, guard) no null test in the original
	const auto farm = TargetOf(villager);
	if (!fish_farms::IsFishFarm(farm))
	{
		return 0;
	}
	// the farm's player (its town's) == the villager's
	if (PlayerOfTown(fish_farms::TownOf(farm)) != GetPlayerOf(villager))
	{
		return 0;
	}
	// VillagerBecomesFisherman(farm) == 1
	if (VillagerBecomesFisherman(villager, farm) != 1)
	{
		return 0;
	}
	// with a player, the player's creature may come to share the town's need for food (0.5, at the villager)
	creature_mimic::EmpathiseWithTownDesire(GetPlayerOf(villager), TownDesireInfo::ForFood, 0.5f, villager);
	return 1;
}

} // namespace openblack::ecs::villager
