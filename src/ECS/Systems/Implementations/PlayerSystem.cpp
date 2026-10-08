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

namespace
{
/// A name out of range falls to the neutral player
size_t IndexOf(openblack::PlayerNames name)
{
	const auto index = static_cast<size_t>(name);
	return index < PlayerSystem::k_Players ? index : static_cast<size_t>(openblack::PlayerNames::NEUTRAL);
}
} // namespace

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

void PlayerSystem::ClearPlayers()
{
	_players.clear();
}

Alignment& PlayerSystem::Alignment(PlayerNames name)
{
	return _alignment[IndexOf(name)];
}

PlayerMagic& PlayerSystem::MagicWithoutEntity(PlayerNames name)
{
	return _magicWithoutEntity[IndexOf(name)];
}

void PlayerSystem::ClearMagicWithoutEntity()
{
	_magicWithoutEntity.fill(PlayerMagic {});
}
