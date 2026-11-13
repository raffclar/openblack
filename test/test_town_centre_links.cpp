/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"
#include "ECS/TownCentreLinks.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::town_centre_links::Links;

namespace
{
/// The totem that a walk for one centre finds: the first totem on the centre in the walk of the totems with a transform
entt::entity TotemByWalk(const ecs::Registry& registry, entt::entity centre)
{
	entt::entity found = entt::null;
	registry.Each<const VillageTotem, const Transform>(
	    [&found, centre](entt::entity totem, const VillageTotem& standing, const Transform& /*plinth*/) {
		    if (found == entt::null && standing.townCentre == centre)
		    {
			    found = totem;
		    }
	    });
	return found;
}

/// The town that a walk for one town number finds: the last town of the number in the walk of the towns with an
/// impression
entt::entity TownByWalk(const ecs::Registry& registry, uint32_t townId)
{
	entt::entity found = entt::null;
	registry.Each<const Town, const TownImpression>(
	    [&found, townId](entt::entity entity, const Town& town, const TownImpression& /*impression*/) {
		    if (town.id == townId)
		    {
			    found = entity;
		    }
	    });
	return found;
}

/// Fills the registry with centres, totems and towns. Some centres have two totems, some totems have no transform, and
/// some towns share a number or have no impression. `extraTransforms` and `extraImpressions` make those stores larger
/// than the stores of the totems and the towns.
std::vector<entt::entity> Fill(ecs::Registry& registry, int extraTransforms, int extraImpressions)
{
	std::vector<entt::entity> centres;
	for (int i = 0; i < 6; ++i)
	{
		centres.push_back(registry.Create());
	}
	for (int i = 0; i < 12; ++i)
	{
		const auto totem = registry.Create();
		registry.Assign<VillageTotem>(totem, VillageTotem {.townCentre = centres[static_cast<size_t>(i % 5)]});
		if (i % 4 != 3)
		{
			registry.Assign<Transform>(totem, Transform {});
		}
	}
	for (int i = 0; i < extraTransforms; ++i)
	{
		registry.Assign<Transform>(registry.Create(), Transform {});
	}
	for (uint32_t i = 0; i < 10; ++i)
	{
		const auto town = registry.Create();
		registry.Assign<Town>(town, Town {.id = i % 4});
		if (i % 3 != 2)
		{
			registry.Assign<TownImpression>(town);
		}
	}
	for (int i = 0; i < extraImpressions; ++i)
	{
		registry.Assign<TownImpression>(registry.Create());
	}
	return centres;
}
} // namespace

TEST(TownCentreLinks, FindTheSameTotemsAndTownsAsAWalkForEachCentre)
{
	// Each store of the two can be the smaller one
	for (const int extraTransforms : {0, 20})
	{
		for (const int extraImpressions : {0, 20})
		{
			ecs::Registry registry;
			const auto centres = Fill(registry, extraTransforms, extraImpressions);
			const auto links = Links::Of(registry);
			for (const auto centre : centres)
			{
				EXPECT_EQ(links.TotemOf(centre), TotemByWalk(registry, centre));
			}
			for (uint32_t townId = 0; townId < 5; ++townId)
			{
				EXPECT_EQ(links.TownOf(townId), TownByWalk(registry, townId));
			}
		}
	}
}

TEST(TownCentreLinks, AnEmptyRegistryHasNoLinks)
{
	ecs::Registry registry;
	const auto links = Links::Of(registry);
	EXPECT_EQ(links.TotemOf(registry.Create()), entt::entity {entt::null});
	EXPECT_EQ(links.TownOf(0), entt::entity {entt::null});
}
