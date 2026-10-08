/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownArchetype.h"

#include "ECS/Components/Influence.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity TownArchetype::Create(int id, const glm::vec3& position, PlayerNames playerOwner, Tribe tribe)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	// const auto& info = Locator::infoConstants::value().town;

	auto& town = registry.Assign<Town>(entity, static_cast<uint32_t>(id));
	town.owner = playerOwner;
	// its place in the game's town list (new towns at the head): a counter that only goes up:
	// the land's own counter (RegistryContext, 0 again with every land), not a process-wide static
	town.creationStamp = ++registry.Context().townCreationCounter;
	// setting the owner adds it at the tail of the owner's list
	town.ownerListStamp = ecs::town_queries::NextOwnerListStamp();
	// the belief is set up while beliefInNeutralPlayer is still 0 (so the neutral belief is 0 until the first fold),
	// then beliefInNeutralPlayer comes from the town info; the 1.0 after it is TownBelief's default
	ecs::town_belief::Init(town);
	if (Locator::infoConstants::has_value())
	{
		town.belief.beliefInNeutralPlayer = Locator::infoConstants::value().town.beliefInNeutralPlayer;
	}
	registry.Assign<Tribe>(entity, tribe);
	registry.Assign<TownInfluence>(entity); // its influence (ECS/Influence); the owner is town.owner above
	// the magic types the town holds, its spell icons and its worship site (src/Worship)
	registry.Assign<TownMagic>(entity);
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& registryContext = registry.Context();
	registryContext.towns.insert({id, entity});

	return entity;
}
