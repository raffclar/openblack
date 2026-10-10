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
#include "ECS/Components/Alignment.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatures.h"
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

void PlayerSystem::KeepForNextLand()
{
	// A player not on this land keeps what they kept from the last one they were on
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Player>([this, &registry](entt::entity entity, const Player& player) {
		const auto* alignment = registry.TryGet<const Alignment>(entity);
		const auto* virtualInfluence = registry.TryGet<const VirtualInfluence>(entity);
		_kept.insert_or_assign(
		    player.name,
		    Kept {.alignment = alignment != nullptr ? std::optional(*alignment) : std::nullopt,
		          .damageFrom = player.damageFrom,
		          .windResistance = player.windResistance,
		          .virtualInfluence = virtualInfluence != nullptr ? std::optional(virtualInfluence->state) : std::nullopt});
	});
}

void PlayerSystem::TakeUpKept(entt::entity playerEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* player = registry.TryGet<Player>(playerEntity);
	if (player == nullptr)
	{
		return;
	}
	const auto found = _kept.find(player->name);
	if (found == _kept.end())
	{
		return;
	}
	const auto& kept = found->second;
	player->damageFrom = kept.damageFrom;
	player->windResistance = kept.windResistance;
	if (kept.alignment.has_value())
	{
		registry.AssignOrReplace<Alignment>(playerEntity, *kept.alignment);
	}
	// The hand goes on keeping what it kept, measured from where it last was in influence even on the land before; its
	// hum went with that land
	if (kept.virtualInfluence.has_value())
	{
		registry.AssignOrReplace<VirtualInfluence>(playerEntity, *kept.virtualInfluence);
	}
}
