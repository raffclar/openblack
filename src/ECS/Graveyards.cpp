/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Graveyards.h"

#include <vector>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Graveyard.h"
#include "ECS/Components/Town.h"
#include "ECS/GraveyardRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::graveyards
{
namespace
{
using ecs::components::Abode;
using ecs::components::Graveyard;
using ecs::components::Town;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The town a building belongs to, none when its town is gone
entt::entity TownOfBuilding(entt::entity building)
{
	const auto& registry = Entities();
	const auto* abode = registry.TryGet<const Abode>(building);
	if (abode == nullptr)
	{
		return entt::null;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(abode->townId);
	return found != towns.end() && registry.Valid(found->second) ? found->second : entt::null;
}

/// Whether the building working as a town's graveyard works. It is usually a graveyard, but may be another civic
/// building that took over from one
bool Works(entt::entity building)
{
	return building != entt::null && Entities().Valid(building) && ecs::villager_home::IsFunctional(building);
}

/// Whether the building takes over as its town's graveyard when the graveyard goes
bool TakesOver(entt::entity building)
{
	const auto* info = ecs::world_objects::AbodeInfoOf(building);
	return info != nullptr && graveyard::TakesOverAsGraveyard(info->abodeType);
}

/// The graveyard counts one more burial, if it has a town and works, until it is full; its graves follow
void CountBurial(entt::entity graveyard)
{
	auto& registry = Entities();
	// Only a graveyard counts burials: another building standing in as the town's graveyard counts none
	auto* yard = registry.TryGet<Graveyard>(graveyard);
	if (yard == nullptr || TownOfBuilding(graveyard) == entt::null || !ecs::villager_home::IsFunctional(graveyard))
	{
		return;
	}
	const auto burials = graveyard::BurialsAfterDeath(yard->burials);
	if (burials != yard->burials)
	{
		yard->burials = burials;
		registry.SetDirty();
	}
}
} // namespace

void Made(entt::entity graveyard)
{
	auto& registry = Entities();
	if (!registry.Valid(graveyard))
	{
		return;
	}
	registry.AssignOrReplace<Graveyard>(graveyard);
	if (ecs::villager_home::IsFunctional(graveyard))
	{
		StartsWorking(graveyard);
	}
}

void StartsWorking(entt::entity graveyard)
{
	auto& registry = Entities();
	if (!registry.AllOf<Graveyard>(graveyard))
	{
		return;
	}
	if (const auto town = TownOfBuilding(graveyard); town != entt::null)
	{
		auto& own = registry.Get<Town>(town).graveyard;
		if (own == entt::null || !registry.Valid(own))
		{
			own = graveyard;
		}
	}
	CountBurial(graveyard);
}

void TownPersonDied(entt::entity town)
{
	if (const auto graveyard = GraveyardOf(town); graveyard != entt::null)
	{
		CountBurial(graveyard);
	}
}

void Lost(entt::entity graveyard)
{
	auto& registry = Entities();
	const auto town = TownOfBuilding(graveyard);
	if (town == entt::null)
	{
		return;
	}
	auto& data = registry.Get<Town>(town);
	if (!registry.AllOf<Graveyard>(graveyard))
	{
		// Another building standing in as the town's graveyard goes: the town is left with none
		if (data.graveyard == graveyard)
		{
			data.graveyard = entt::null;
		}
		return;
	}
	std::vector<entt::entity> others;
	std::vector<graveyard::OtherBuilding> facts;
	for (const auto building : data.abodes)
	{
		if (building != graveyard && registry.Valid(building))
		{
			others.push_back(building);
			facts.push_back({.working = Works(building), .takesOver = TakesOver(building)});
		}
	}
	const auto takesOver = graveyard::GraveyardAfterLoss(data.graveyard == graveyard, facts);
	data.graveyard = takesOver.has_value() ? others.at(*takesOver) : entt::null;
}

bool HasWorkingGraveyard(entt::entity town)
{
	return Works(GraveyardOf(town));
}

entt::entity GraveyardOf(entt::entity town)
{
	const auto& registry = Entities();
	if (!registry.Valid(town))
	{
		return entt::null;
	}
	const auto* data = registry.TryGet<const Town>(town);
	return data != nullptr && registry.Valid(data->graveyard) ? data->graveyard : entt::null;
}

uint32_t GravesShown(entt::entity graveyard)
{
	const auto* yard = Entities().TryGet<const Graveyard>(graveyard);
	return yard != nullptr ? graveyard::GravesShown(yard->burials) : 0;
}

} // namespace openblack::graveyards
