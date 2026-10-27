/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameEndSystem.h"

#include <algorithm>
#include <array>

#include <spdlog/spdlog.h>

#include "3D/TempleExteriorMorph.h"
#include "Common/X87.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/LeftGame.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Online/SkirmishPing.h"

namespace openblack::ecs::systems
{
using components::GameStanding;

namespace
{
/// The world of the game for the end of the game
class GameEndWorld final: public game_end::World
{
public:
	GameEndWorld()
	    : _registry(Locator::entitiesRegistry::value())
	{
		_players.fill(entt::null);
		_registry.Each<const components::Player>([this](entt::entity entity, const components::Player& player) {
			if (player.name < PlayerNames::_COUNT)
			{
				_players.at(static_cast<size_t>(player.name)) = entity;
			}
		});
	}

	[[nodiscard]] std::vector<PlayerNames> ActiveGods() const override
	{
		std::vector<PlayerNames> gods;
		for (uint8_t number = 0; number < static_cast<uint8_t>(PlayerNames::NEUTRAL); ++number)
		{
			const auto player = static_cast<PlayerNames>(number);
			if (IsHuman(player) || IsComputer(player))
			{
				gods.push_back(player);
			}
		}
		return gods;
	}

	[[nodiscard]] bool IsHuman(PlayerNames player) const override
	{
		// A human moves the hands of the god. A computer mind that plays the god makes it a computer god.
		return Entity(player) != entt::null && Locator::handSystem::has_value() &&
		       Locator::handSystem::value().GetHandsOf(player).has_value() && !IsComputer(player);
	}

	[[nodiscard]] bool HasTemple(PlayerNames player) const override
	{
		bool temple = false;
		_registry.Each<const components::Temple>(
		    [&temple, player](const components::Temple& each) { temple = temple || each.owner == player; });
		return temple;
	}

	[[nodiscard]] GameStanding& Standing(PlayerNames player) override
	{
		const auto entity = Entity(player);
		auto* standing = _registry.TryGet<GameStanding>(entity);
		return standing != nullptr ? *standing : _registry.Assign<GameStanding>(entity);
	}

	[[nodiscard]] float InfluenceShare(PlayerNames player) const override
	{
		// The game sums the influence of every player and the neutral player, in their order
		const auto& influence = Locator::influenceSystem::value();
		float total = 0.0f;
		for (uint8_t number = 0; number < static_cast<uint8_t>(PlayerNames::_COUNT); ++number)
		{
			total = x87::Add(influence.InfluencePower(static_cast<PlayerNames>(number)), total);
		}
		constexpr int32_t k_FirstLand = 1;
		if (_registry.Context().mapScriptGlobals.landNumber == k_FirstLand || total == 0.0f)
		{
			return TempleExteriorMorph::k_ShareOfNoInfluence;
		}
		return x87::Div(influence.InfluencePower(player), total);
	}

	[[nodiscard]] float InfluencePower(PlayerNames player) const override
	{
		return Locator::influenceSystem::value().InfluencePower(player);
	}

	void RemoveCreature(PlayerNames player) override
	{
		if (const auto creature = Locator::playerSystem::value().GetPrimaryCreature(player); creature.has_value())
		{
			world_objects::Remove(*creature);
		}
	}

	void CountGodDefeated(PlayerNames player) override
	{
		if (auto* statistics = _registry.TryGet<components::PlayerStatistics>(Entity(player)))
		{
			++statistics->godsDefeated;
		}
	}

	void StopMind(PlayerNames player) override
	{
		if (IsComputer(player) && Locator::computerPlayerSystem::has_value())
		{
			Locator::computerPlayerSystem::value().SwitchOn(player, false);
		}
	}

private:
	/// The entity of a player, none for a player that the land has not made
	[[nodiscard]] entt::entity Entity(PlayerNames player) const { return _players.at(static_cast<size_t>(player)); }

	[[nodiscard]] bool IsComputer(PlayerNames player) const
	{
		const auto entity = Entity(player);
		if (entity == entt::null)
		{
			return false;
		}
		const auto* mind = _registry.TryGet<const components::ComputerPlayer>(entity);
		return mind != nullptr && mind->marked;
	}

	Registry& _registry;
	std::array<entt::entity, static_cast<size_t>(PlayerNames::_COUNT)> _players {};
};
} // namespace

void GameEndSystem::ProcessTurn(uint32_t turn)
{
	_events.clear();
	auto& registry = Locator::entitiesRegistry::value();
	auto& context = registry.Context();
	const bool networkGame = Locator::time::value().IsNetworkGame();
	const game_end::Facts facts {
	    .networkGame = networkGame,
	    .skirmish = context.skirmish && !networkGame,
	    .turn = turn,
	    .timeLimitTurn = std::nullopt,
	    .conditionCount = 0,
	};
	if ((!facts.networkGame && !facts.skirmish) || context.gameEnd.decided)
	{
		return;
	}
	GameEndWorld world;
	// The god of a human who left the network game is defeated
	registry.Each<const components::Player, const components::LeftGame>(
	    [&world](const components::Player& player, const components::LeftGame&) {
		    auto& standing = world.Standing(player.name);
		    if (!standing.out)
		    {
			    standing.out = true;
			    standing.reason = components::OutReason::Left;
		    }
	    });
	_events = game_end::ProcessTurn(world, facts, context.gameEnd);
	_eventsToShow.insert(_eventsToShow.end(), _events.begin(), _events.end());
	// The lobby hears of a decided skirmish. The message is not part of the world.
	if (facts.skirmish && Locator::lobbyClient::has_value())
	{
		online::SendSkirmishPings(Locator::lobbyClient::value(), game_end::DecidingEvents(_events));
	}
	if (const auto logger = spdlog::get("game"))
	{
		for (const auto& event : _events)
		{
			SPDLOG_LOGGER_INFO(logger, "Turn {}: the god of player {} {}", turn, static_cast<int>(event.god),
			                   event.kind == game_end::Event::Kind::Won      ? "won the game"
			                   : event.kind == game_end::Event::Kind::GodOut ? "is out of the game"
			                                                                 : "is out of the game, and the game is decided");
		}
	}
}

bool GameEndSystem::IsDecided() const
{
	return Locator::entitiesRegistry::value().Context().gameEnd.decided;
}

std::optional<PlayerNames> GameEndSystem::LastGodIn() const
{
	// The view reads the standings and adds none: a god with no standing yet is still in
	const GameEndWorld world;
	auto& registry = Locator::entitiesRegistry::value();
	const auto gods = world.ActiveGods();
	std::vector<PlayerNames> in;
	registry.Each<const components::Player>([&gods, &registry, &in](entt::entity entity, const components::Player& player) {
		const auto* standing = registry.TryGet<const GameStanding>(entity);
		if (game_end::CanLose(player.name) && std::ranges::find(gods, player.name) != gods.end() &&
		    (standing == nullptr || !standing->out))
		{
			in.push_back(player.name);
		}
	});
	return in.size() == 1 ? std::optional(in.front()) : std::nullopt;
}

std::vector<GameEndSystemInterface::GodStanding> GameEndSystem::Standings() const
{
	// The view reads the standings and adds none
	const GameEndWorld world;
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<GodStanding> standings;
	for (const auto god : world.ActiveGods())
	{
		standings.push_back({.god = god, .standing = {}});
	}
	registry.Each<const components::Player>([&registry, &standings](entt::entity entity, const components::Player& player) {
		const auto found = std::ranges::find(standings, player.name, &GodStanding::god);
		if (const auto* standing = registry.TryGet<const GameStanding>(entity); standing != nullptr && found != standings.end())
		{
			found->standing = *standing;
		}
	});
	return standings;
}

} // namespace openblack::ecs::systems
