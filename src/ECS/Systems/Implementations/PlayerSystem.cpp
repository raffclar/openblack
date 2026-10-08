/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PlayerSystem.h"

#include "ECS/Components/Player.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

void PlayerSystem::RegisterPlayers()
{
	// The players of a land that has gone are forgotten; those already known on this land stay as they were
	const auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(_players, [&registry](const auto& entry) { return !registry.Valid(entry.second); });
	registry.Each<const Player>(
	    [this](const entt::entity entity, const Player& player) { _players.emplace(player.name, entity); });
}

void PlayerSystem::AddPlayer(entt::entity playerEntity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& player = registry.Get<components::Player>(playerEntity);
	// A player made for a new land takes the place of the one of that name on the last land
	_players.insert_or_assign(player.name, playerEntity);
}

entt::entity PlayerSystem::GetPlayer(PlayerNames playerName) const
{
	return _players.at(playerName);
}
