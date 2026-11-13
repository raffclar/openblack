/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownCentreLinks.h"

#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"

using namespace openblack::ecs::town_centre_links;
using namespace openblack::ecs::components;

Links Links::Of(const Registry& registry)
{
	Links links;
	const auto& underlying = registry.Underlying();
	// The totems: the walk keeps the first totem of each centre. When the store of the totems leads, the walk tests the
	// centre before it looks for the transform.
	const auto* totems = underlying.storage<VillageTotem>();
	const auto* transforms = underlying.storage<Transform>();
	if (totems != nullptr && transforms != nullptr && !(transforms->size() < totems->size()))
	{
		for (const auto [totem, standing] : underlying.view<const VillageTotem>().each())
		{
			if (transforms->contains(totem))
			{
				links._totems.try_emplace(standing.townCentre, totem);
			}
		}
	}
	else
	{
		registry.Each<const VillageTotem, const Transform>(
		    [&links](entt::entity totem, const VillageTotem& standing, const Transform& /*plinth*/) {
			    links._totems.try_emplace(standing.townCentre, totem);
		    });
	}
	// The towns: the walk keeps the last town of each number
	const auto* towns = underlying.storage<Town>();
	const auto* impressions = underlying.storage<TownImpression>();
	if (towns != nullptr && impressions != nullptr && !(impressions->size() < towns->size()))
	{
		for (const auto [entity, town] : underlying.view<const Town>().each())
		{
			if (impressions->contains(entity))
			{
				links._towns.insert_or_assign(town.id, entity);
			}
		}
	}
	else
	{
		registry.Each<const Town, const TownImpression>(
		    [&links](entt::entity entity, const Town& town, const TownImpression& /*impression*/) {
			    links._towns.insert_or_assign(town.id, entity);
		    });
	}
	return links;
}

entt::entity Links::TotemOf(entt::entity centre) const
{
	const auto found = _totems.find(centre);
	return found != _totems.end() ? found->second : entt::entity {entt::null};
}

entt::entity Links::TownOf(uint32_t townId) const
{
	const auto found = _towns.find(townId);
	return found != _towns.end() ? found->second : entt::entity {entt::null};
}
