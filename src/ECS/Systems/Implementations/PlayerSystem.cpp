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

#include <algorithm>

#include "Creature/PrimaryCreature.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerCreatures.h"
#include "ECS/Components/PlayerRelations.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/ProfileAlignment.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
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

size_t PlayerSystem::PlayerNumbersInGame() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	size_t count = 0;
	for (const auto& [name, entity] : _players)
	{
		if (registry.Valid(entity))
		{
			count = std::max(count, static_cast<size_t>(name) + 1);
		}
	}
	return player_slots::PlayerNumbersOf(count);
}

std::optional<entt::entity> PlayerSystem::FindPlayer(PlayerNames playerName) const
{
	const auto player = _players.find(playerName);
	return player != _players.end() ? std::optional(player->second) : std::nullopt;
}

entt::entity PlayerSystem::EnsurePlayer(PlayerNames playerName)
{
	// A player of a land that is gone is not a player of this land
	if (const auto player = FindPlayer(playerName); player.has_value() && Locator::entitiesRegistry::value().Valid(*player))
	{
		return *player;
	}
	const auto entity = openblack::ecs::archetypes::PlayerArchetype::Create(playerName);
	AddPlayer(entity);
	return entity;
}

openblack::player_kind::Facts PlayerSystem::FactsOf(PlayerNames name) const
{
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	// A computer mind that a land script or the start of a game switched on marks its player
	bool computerMind = false;
	if (const auto player = FindPlayer(name); player.has_value() && Locator::entitiesRegistry::has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		const auto* mind = registry.Valid(*player) ? registry.TryGet<const ComputerPlayer>(*player) : nullptr;
		computerMind = mind != nullptr && mind->marked;
	}
	const bool hands = Locator::handSystem::has_value() && Locator::handSystem::value().GetHandsOf(name).has_value();
	return {.player = name, .networkGame = networkGame, .computerMind = computerMind, .hands = hands};
}

bool PlayerSystem::IsComputerPlayer(PlayerNames name) const
{
	return player_kind::IsComputer(FactsOf(name));
}

bool PlayerSystem::IsHumanPlayer(PlayerNames name) const
{
	return player_kind::IsHuman(FactsOf(name));
}

void PlayerSystem::AddCreature(entt::entity creature)
{
	Acquire(creature, false);
}

void PlayerSystem::MakePrimaryCreature(entt::entity creature)
{
	Acquire(creature, true);
}

void PlayerSystem::Acquire(entt::entity creature, bool primary)
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
	const auto before = GetPrimaryCreature(body->owner);
	if (primary)
	{
		primary_creature::MakePrimary(creatures.acquired, creature);
	}
	else
	{
		primary_creature::Acquire(creatures.acquired, creature);
	}
	// A new primary creature changes the icon on the player's totems
	if (GetPrimaryCreature(body->owner) != before && Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().RefreshIcons(body->owner);
	}
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
		const auto* statistics = registry.TryGet<const PlayerStatistics>(entity);
		_kept.insert_or_assign(
		    player.name,
		    Kept {.alignment = alignment != nullptr ? std::optional(*alignment) : std::nullopt,
		          .damageFrom = player.damageFrom,
		          .windResistance = player.windResistance,
		          .miracles = player.miracles,
		          .virtualInfluence = virtualInfluence != nullptr ? std::optional(virtualInfluence->state) : std::nullopt,
		          .virtualInfluenceShown = virtualInfluence != nullptr ? virtualInfluence->shown : virtual_influence::Shown {},
		          .influenceHistory = statistics != nullptr ? statistics->influenceHistory : statistics::History {},
		          .populationHistory = statistics != nullptr ? statistics->populationHistory : statistics::History {}});
	});
}

void PlayerSystem::ForgetKept()
{
	_kept.clear();
	_alignmentKeptTurn = 0;
}

bool PlayerSystem::KeepAlignmentAt(uint32_t turn)
{
	if (!profile_alignment::DueToKeep(turn, _alignmentKeptTurn))
	{
		return false;
	}
	_alignmentKeptTurn = turn;
	return true;
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
	// Their enabled miracles and their power with each tribe don't outlast the land
	player->miracles = player_miracles::AfterLandCleared(kept.miracles);
	if (kept.alignment.has_value())
	{
		registry.AssignOrReplace<Alignment>(playerEntity, *kept.alignment);
	}
	// The hand goes on keeping what it kept, measured from where it last was in influence even on the land before; its
	// hum went with that land
	if (kept.virtualInfluence.has_value())
	{
		registry.AssignOrReplace<VirtualInfluence>(playerEntity, *kept.virtualInfluence, kept.virtualInfluenceShown);
	}
	// Their histories go on, though their statistics' counters start again
	if (auto* statistics = registry.TryGet<PlayerStatistics>(playerEntity))
	{
		statistics->influenceHistory = kept.influenceHistory;
		statistics->populationHistory = kept.populationHistory;
	}
}

void PlayerSystem::SetAlly(PlayerNames player, PlayerNames other, float strength)
{
	const auto playerIndex = static_cast<size_t>(player);
	const auto otherIndex = static_cast<size_t>(other);
	if (playerIndex >= k_PlayerRecordSize || otherIndex >= k_PlayerRecordSize)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto allies = [this, &registry](PlayerNames name) -> PlayerAllies& {
		const auto entity = EnsurePlayer(name);
		auto* found = registry.TryGet<PlayerAllies>(entity);
		return found != nullptr ? *found : registry.Assign<PlayerAllies>(entity);
	};
	// The alliance has the same strength for each of the two players
	allies(other).strength.at(playerIndex) = strength;
	allies(player).strength.at(otherIndex) = strength;
}

float PlayerSystem::AllyStrength(PlayerNames player, PlayerNames other) const
{
	const auto otherIndex = static_cast<size_t>(other);
	const auto entity = FindPlayer(player).value_or(entt::null);
	const auto& registry = Locator::entitiesRegistry::value();
	if (otherIndex >= k_PlayerRecordSize || !registry.Valid(entity))
	{
		return 0.0f;
	}
	const auto* allies = registry.TryGet<const PlayerAllies>(entity);
	return allies != nullptr ? allies->strength.at(otherIndex) : 0.0f;
}
