/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldToolTips.h"

#include <algorithm>

#include "ECS/Components/Abode.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/TownHomes.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::gui::world_tooltip
{

namespace
{
using namespace ecs::components;

constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

/// How much more the town believes in the local player than in the player it believes in most besides
float BeliefMargin(const TownImpression& impression)
{
	const auto& belief = impression.belief.belief;
	float others = 0.0f;
	bool any = false;
	for (size_t i = 0; i < belief.size(); ++i)
	{
		if (i == static_cast<size_t>(k_LocalPlayer))
		{
			continue;
		}
		others = any ? std::max(others, belief[i]) : belief[i];
		any = true;
	}
	return belief[static_cast<size_t>(k_LocalPlayer)] - others;
}

/// The places for people in a town's homes, adults and children
uint32_t PlacesOf(const Town& town)
{
	uint32_t places = 0;
	for (const auto abode : town.abodes)
	{
		if (const auto* info = ecs::world_objects::AbodeInfoOf(abode))
		{
			places += info->maxVillagersInAbode + info->maxChildrenInAbode;
		}
	}
	return places;
}
} // namespace

std::vector<Submission> Over(entt::entity object)
{
	std::vector<Submission> submissions;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return submissions;
	}
	if (registry.AllOf<Temple>(object))
	{
		submissions.push_back({.numbers = TempleHealth(ecs::world_objects::LifeOf(object))});
		return submissions;
	}
	if (const auto* entrance = registry.TryGet<const TempleEntrance>(object))
	{
		const auto* temple = registry.Valid(entrance->temple) ? registry.TryGet<const Temple>(entrance->temple) : nullptr;
		if (temple != nullptr && temple->owner == k_LocalPlayer)
		{
			submissions.push_back({.numbers = {.index = k_EnterTemple, .first = 0.0, .second = std::nullopt},
			                       .action = ToolTipAction::Apply,
			                       .wordsOnly = true});
		}
		return submissions;
	}
	if (const auto* abode = registry.TryGet<const Abode>(object))
	{
		const auto town = ecs::town_homes::TownOfAbode(registry, object);
		if (abode->type == AbodeNumber::TownCentre && town != entt::null && registry.Valid(town))
		{
			if (const auto* impression = registry.TryGet<const TownImpression>(town))
			{
				submissions.push_back({.numbers = Belief(BeliefMargin(*impression))});
			}
			if (const auto* townComponent = registry.TryGet<const Town>(town))
			{
				uint32_t people = 0;
				registry.Each<const Villager>(
				    [&people, town](entt::entity, const Villager& villager) { people += villager.town == town ? 1u : 0u; });
				submissions.push_back({.numbers = Population(people, PlacesOf(*townComponent))});
			}
		}
		else if (abode->type == AbodeNumber::StoragePit && Locator::resourceStoreSystem::has_value())
		{
			const auto& stores = Locator::resourceStoreSystem::value();
			submissions.push_back({.numbers = Stored(stores.GetResource(object, ResourceType::Food),
			                                         stores.GetResource(object, ResourceType::Wood))});
		}
		return submissions;
	}
	if (registry.AllOf<Pot>(object) && Locator::resourceStoreSystem::has_value())
	{
		// A pile, of a store or alone, says what its store holds or else what it holds itself
		const auto& stores = Locator::resourceStoreSystem::value();
		const auto type = stores.ResourceOf(object).type;
		if (type == ResourceType::Food || type == ResourceType::Wood)
		{
			submissions.push_back(
			    {.numbers = PileAmount(type == ResourceType::Food, stores.GetResource(object, type)), .force = true});
		}
	}
	return submissions;
}

} // namespace openblack::gui::world_tooltip
