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

#include "Creature/PrimaryCreature.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

void PlayerSystem::RegisterPlayers()
{
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Player>(
	    [this](const entt::entity entity, const Player& player) { _players.emplace(player.name, entity); });
}

void PlayerSystem::AddPlayer(entt::entity playerEntity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& player = registry.Get<components::Player>(playerEntity);
	_players.emplace(player.name, playerEntity);
}

entt::entity PlayerSystem::GetPlayer(PlayerNames playerName) const
{
	return _players.at(playerName);
}

void PlayerSystem::AddCreature(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto player = body != nullptr ? _players.find(body->owner) : _players.end();
	if (player == _players.end() || !registry.Valid(player->second))
	{
		return;
	}
	auto* list = registry.TryGet<PlayerCreatures>(player->second);
	auto& creatures = list != nullptr ? *list : registry.Assign<PlayerCreatures>(player->second);
	primary_creature::Acquire(creatures.acquired, creature);
}

std::optional<entt::entity> PlayerSystem::GetPrimaryCreature(PlayerNames name) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto player = _players.find(name);
	if (player == _players.end() || !registry.Valid(player->second))
	{
		return std::nullopt;
	}
	const auto* list = registry.TryGet<const PlayerCreatures>(player->second);
	if (list == nullptr)
	{
		return std::nullopt;
	}
	return primary_creature::Primary(list->acquired, [&registry, name](entt::entity creature) {
		const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
		return body != nullptr && body->owner == name;
	});
}
