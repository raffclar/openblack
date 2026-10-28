/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerArchetype.h"

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity PlayerArchetype::Create(PlayerNames name)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Player>(entity, name);
	registry.Assign<PlayerStatistics>(entity);
	// Each player has an alignment from the start, which is neutral until the player does good or evil. Thus a player has
	// the same parts on a land that the game loaded and on a land that the story changed to.
	registry.Assign<Alignment>(entity);
	// Each player has no allies at the start, a computer mind that thinks of each player at the start value, and no
	// influence power yet
	registry.Assign<PlayerAllies>(entity);
	registry.Assign<ComputerAttitudes>(entity);
	registry.Assign<PlayerInfluencePower>(entity);
	// A player made again on a later land is as they were on the last
	if (Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().TakeUpKept(entity);
	}
	// A player made after the land made its towns may cast what its towns offer
	if (Locator::spellIconSystem::has_value())
	{
		Locator::spellIconSystem::value().PlayerMade(name);
	}
	return entity;
}
