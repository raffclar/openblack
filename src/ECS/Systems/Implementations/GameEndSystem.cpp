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
#include <string>

#include <spdlog/spdlog.h>

#include "3D/TempleExteriorMorph.h"
#include "Common/X87.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/LeftGame.h"
#include "ECS/Components/MiracleImpression.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GamePlayers.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Online/SkirmishPing.h"
#include "PlayerSlots.h"

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
			if (player_slots::IsPlayerNumber(player.name))
			{
				_players.at(static_cast<size_t>(player.name)) = entity;
			}
		});
	}

	[[nodiscard]] std::vector<PlayerNames> ActiveGods() const override
	{
		std::vector<PlayerNames> gods;
		for (const auto player : ecs::game_players::Gods())
		{
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
		for (const auto each : ecs::game_players::All())
		{
			total = x87::Add(influence.InfluencePower(each), total);
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
	std::array<entt::entity, player_slots::k_PlayerNumbers> _players {};
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
	    .timeLimitTurn = networkGame ? std::optional(game_end::TimeLimitTurn(GetTimeLimit())) : std::nullopt,
	    .conditionCount = context.gameEnd.conditionCount,
	    .templesNeeded = !(networkGame && Locator::landSystem::has_value() && Locator::landSystem::value().OnTestbed()) &&
	                     !(Locator::simulation::has_value() && Locator::simulation::value().GetSettings().godsNeedNoTemple),
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
		// The rank of each god when the game is decided, so that the tools can compare the end on each computer
		if (game_end::DecidingEvents(_events) != 0)
		{
			for (const auto god : world.ActiveGods())
			{
				const auto& standing = world.Standing(god);
				SPDLOG_LOGGER_INFO(logger, "Turn {}: the god of player {} ends the game {} with a share of {:.9g} at place {}",
				                   turn, static_cast<int>(god), standing.out ? "out" : "in", standing.share,
				                   standing.place.has_value() ? std::to_string(*standing.place) : std::string("none"));
			}
		}
	}
}

uint32_t GameEndSystem::GetTimeLimit() const
{
	if (_timeLimitMinutes.has_value())
	{
		return *_timeLimitMinutes;
	}
	// The game server and a tool's network game without a server take the time limit of the test switch
	if (Locator::simulation::has_value())
	{
		if (const auto& limit = Locator::simulation::value().GetSettings().timeLimit; limit.has_value())
		{
			return *limit;
		}
	}
	return game_end::k_DefaultTimeLimitMinutes;
}

void GameEndSystem::GiveConditions()
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& context = registry.Context();
	if (!Locator::time::value().IsNetworkGame() || context.gameEnd.conditionCount != 0 || context.gameEnd.decided)
	{
		return;
	}
	// The conditions of the game server, else those of the test switch of a tool's run
	std::vector<game_end::ConditionPick> picks;
	if (_conditions.has_value())
	{
		picks = *_conditions;
	}
	else if (Locator::simulation::has_value())
	{
		picks = Locator::simulation::value().GetSettings().conditions;
	}
	if (picks.empty())
	{
		return;
	}
	GameEndWorld world;
	const auto turn = Locator::time::value().GetTurn();
	for (const auto god : world.ActiveGods())
	{
		game_end::GiveConditions(world.Standing(god), picks);
		if (const auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_INFO(logger, "Turn {}: the god of player {} gets {} winning conditions", turn, static_cast<int>(god),
			                   picks.size());
		}
	}
	context.gameEnd.conditionCount = static_cast<uint32_t>(picks.size());
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

namespace
{
/// The standing of a player that has winning conditions, in a network game. None otherwise.
GameStanding* StandingWithConditions(Registry& registry, PlayerNames player)
{
	if (!Locator::time::has_value() || !Locator::time::value().IsNetworkGame())
	{
		return nullptr;
	}
	GameStanding* found = nullptr;
	registry.Each<const components::Player>([&registry, &found, player](entt::entity entity, const components::Player& each) {
		if (found == nullptr && each.name == player)
		{
			found = registry.TryGet<GameStanding>(entity);
		}
	});
	return found;
}

/// Which winning conditions of a god are completed
std::array<bool, GameStanding::k_ConditionCount> CompletedConditions(const GameStanding& standing)
{
	std::array<bool, GameStanding::k_ConditionCount> completed {};
	std::ranges::transform(standing.conditions, completed.begin(),
	                       [](const auto& condition) { return condition.has_value() && condition->completed; });
	return completed;
}

/// The log tells each winning condition that the god completed since an earlier look, so that the tools can compare
/// the turns of the server and of each client
void LogNewlyCompleted(PlayerNames player, const std::array<bool, GameStanding::k_ConditionCount>& before,
                       const GameStanding& standing)
{
	const auto logger = spdlog::get("game");
	if (!logger)
	{
		return;
	}
	const auto after = CompletedConditions(standing);
	for (size_t index = 0; index < after.size(); ++index)
	{
		if (after.at(index) && !before.at(index))
		{
			SPDLOG_LOGGER_INFO(logger, "Turn {}: the god of player {} completed winning condition {} at {}",
			                   Locator::time::value().GetTurn(), static_cast<int>(player), index,
			                   standing.conditions.at(index)->current);
		}
	}
}

/// The amounts of the piles of the storage pits of the player's towns: the food pile, or the wood piles
std::vector<uint32_t> StorePiles(Registry& registry, const std::vector<entt::entity>& towns, bool wood)
{
	std::vector<uint32_t> amounts;
	const auto add = [&registry, &amounts](entt::entity pile) {
		if (registry.Valid(pile))
		{
			if (const auto* pot = registry.TryGet<const components::Pot>(pile))
			{
				amounts.push_back(pot->amount);
			}
		}
	};
	for (const auto town : towns)
	{
		const auto pit = registry.Valid(town) ? construction::StoragePitOf(town) : std::nullopt;
		const auto* store = pit.has_value() ? registry.TryGet<const components::StoragePit>(*pit) : nullptr;
		if (store == nullptr)
		{
			continue;
		}
		if (wood)
		{
			std::ranges::for_each(store->woodPiles, add);
		}
		else
		{
			add(store->foodPile);
		}
	}
	return amounts;
}
} // namespace

void GameEndSystem::AddToCondition(std::optional<PlayerNames> player, game_end::Condition condition, int32_t amount)
{
	if (!player.has_value())
	{
		return;
	}
	if (auto* standing = StandingWithConditions(Locator::entitiesRegistry::value(), *player))
	{
		const auto before = CompletedConditions(*standing);
		game_end::AddToCondition(*standing, condition, amount);
		LogNewlyCompleted(*player, before, *standing);
	}
}

void GameEndSystem::BuildingWorks(entt::entity building)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.Valid(building) ? registry.TryGet<const components::Abode>(building) : nullptr;
	const auto& towns = registry.Context().towns;
	const auto found = abode != nullptr ? towns.find(abode->townId) : towns.end();
	if (found == towns.end() || !registry.Valid(found->second))
	{
		return;
	}
	auto& town = registry.Get<components::Town>(found->second);
	if (town.countedAsNew)
	{
		return;
	}
	const auto typeOf = [](entt::entity each) {
		const auto* info = world_objects::AbodeInfoOf(each);
		return info != nullptr ? info->abodeType : AbodeType::General;
	};
	std::vector<game_end::TownBuilding> others;
	for (const auto other : town.abodes)
	{
		if (other == building || !registry.Valid(other) || !registry.AllOf<components::Abode>(other))
		{
			continue;
		}
		const auto* progress = registry.TryGet<const components::BuildProgress>(other);
		others.push_back({.type = typeOf(other), .built = progress == nullptr || progress->built >= 1.0f});
	}
	if (!game_end::CompletesNewTown(others, typeOf(building)))
	{
		return;
	}
	town.countedAsNew = true;
	AddToCondition(world_objects::PlayerOf(building), game_end::Condition::NewTownsBuilt, 1);
}

void GameEndSystem::MeasureConditions(PlayerNames player)
{
	using game_end::Condition;
	auto& registry = Locator::entitiesRegistry::value();
	auto* standing = StandingWithConditions(registry, player);
	if (standing == nullptr ||
	    std::ranges::none_of(std::array {Condition::WoodInStores, Condition::FoodInStores, Condition::PrayerPowerGenerated,
	                                     Condition::BeliefInWorld},
	                         [standing](Condition condition) { return game_end::HasCondition(*standing, condition); }))
	{
		return;
	}
	const auto before = CompletedConditions(*standing);
	const auto towns = Locator::townSystem::value().TownsInGainedOrder(player);
	game_end::SetCondition(*standing, Condition::WoodInStores, game_end::AmountInStores(StorePiles(registry, towns, true)));
	game_end::SetCondition(*standing, Condition::FoodInStores, game_end::AmountInStores(StorePiles(registry, towns, false)));
	// The prayer power that the worship sites of the player's temple have available. A player with no temple keeps the
	// value that it had.
	if (Locator::worshipSiteSystem::has_value())
	{
		const auto& worship = Locator::worshipSiteSystem::value();
		const auto temple = worship.TempleOf(player);
		if (const auto* places = registry.Valid(temple) ? registry.TryGet<const components::CitadelWorship>(temple) : nullptr)
		{
			std::vector<float> sites;
			for (const auto site : places->sites)
			{
				if (site != entt::null && registry.Valid(site))
				{
					sites.push_back(worship.ChantsAvailable(site));
				}
			}
			game_end::SetCondition(*standing, Condition::PrayerPowerGenerated, game_end::PrayerPower(sites));
		}
	}
	// The belief of each town of the player in the player
	std::vector<float> beliefs;
	for (const auto town : towns)
	{
		const auto* impression = registry.Valid(town) ? registry.TryGet<const components::TownImpression>(town) : nullptr;
		beliefs.push_back(impression != nullptr ? impression->belief.belief.at(static_cast<size_t>(player)) : 0.0f);
	}
	game_end::SetCondition(*standing, Condition::BeliefInWorld, game_end::BeliefInWorld(beliefs));
	LogNewlyCompleted(player, before, *standing);
}

} // namespace openblack::ecs::systems
