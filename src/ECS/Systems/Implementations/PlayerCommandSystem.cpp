/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PlayerCommandSystem.h"

#include <iterator>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <spdlog/spdlog.h>

#include "Creature/LeashReport.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/LeftGame.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/CreatureFromFile.h"
#include "ECS/ProfileAlignment.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using namespace openblack::player_commands;

namespace
{
/// The help of this computer is for the player of this computer only
bool PlayerOfThisComputer(openblack::PlayerNames player)
{
	return !openblack::Locator::playerSystem::has_value() ||
	       openblack::Locator::playerSystem::value().GetLocalPlayer() == player;
}

/// The words of the leash report for a command that can change the leash of the player's creature, or none for
/// another command
std::optional<std::string_view> LeashCommandWords(const Action& action)
{
	return std::visit(
	    [](const auto& command) -> std::optional<std::string_view> {
		    using Kind = std::decay_t<decltype(command)>;
		    if constexpr (std::is_same_v<Kind, LeashKey>)
		    {
			    switch (command.key)
			    {
			    case openblack::creature_leash::LeashKey::Leash:
				    return "presses the leash key";
			    case openblack::creature_leash::LeashKey::PreviousLeash:
				    return "presses the previous leash key";
			    case openblack::creature_leash::LeashKey::NextLeash:
				    return "presses the next leash key";
			    }
			    return "presses a leash key";
		    }
		    else if constexpr (std::is_same_v<Kind, LeashTap>)
		    {
			    return "taps a creature";
		    }
		    else if constexpr (std::is_same_v<Kind, LeashOrder>)
		    {
			    return "gives a leash order";
		    }
		    else if constexpr (std::is_same_v<Kind, LeashPostTap>)
		    {
			    return "taps a leash post";
		    }
		    else if constexpr (std::is_same_v<Kind, LeashPick>)
		    {
			    return "picks a leash";
		    }
		    else if constexpr (std::is_same_v<Kind, HandShakeOff>)
		    {
			    return "shakes the hand";
		    }
		    else if constexpr (std::is_same_v<Kind, TempleLeashOff>)
		    {
			    return "goes into the temple";
		    }
		    else if constexpr (std::is_same_v<Kind, LeashLength>)
		    {
			    return "lets go of the rope";
		    }
		    else
		    {
			    return std::nullopt;
		    }
	    },
	    action);
}

/// Write the leash of the player's creature after a leash command to the log. Each computer writes the same line for
/// the same command, so that a network check can compare the leash on the server and on each client.
void LogLeashCommand(openblack::PlayerNames player, std::string_view words, uint32_t turn)
{
	const auto logger = spdlog::get("game");
	if (logger == nullptr || !openblack::Locator::leashSystem::has_value())
	{
		return;
	}
	openblack::creature_leash::CommandReport report {
	    .player = static_cast<uint32_t>(player),
	    .command = words,
	    .turn = turn,
	};
	if (const auto creature = openblack::Locator::leashSystem::value().PlayersCreature(player); creature.has_value())
	{
		report.creature = entt::to_integral(*creature);
		const auto* leashes =
		    openblack::Locator::entitiesRegistry::value().TryGet<const openblack::ecs::components::CreatureLeash>(*creature);
		if (leashes != nullptr && leashes->worn.has_value())
		{
			const auto& worn = *leashes->worn;
			report.worn = openblack::creature_leash::WornReport {
			    .type = worn.type,
			    .holder = static_cast<uint32_t>(worn.holder),
			    .tiedTo = worn.tiedTo.has_value() ? std::optional(entt::to_integral(*worn.tiedTo)) : std::nullopt,
			};
		}
	}
	SPDLOG_LOGGER_INFO(logger, "{}", openblack::creature_leash::ReportLine(report));
}

/// Do one command for the player that sent it, at a turn
void Run(const Command& command, uint32_t turn)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	std::visit(
	    [&](const auto& action) {
		    using Kind = std::decay_t<decltype(action)>;
		    if constexpr (std::is_same_v<Kind, LeashKey>)
		    {
			    if (openblack::Locator::leashSystem::has_value())
			    {
				    openblack::Locator::leashSystem::value().PressKey(command.player, action.key);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, LeashTap>)
		    {
			    if (openblack::Locator::leashSystem::has_value() && registry.Valid(action.creature))
			    {
				    openblack::Locator::leashSystem::value().TapCreature(command.player, action.creature);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, CreatureFeedback>)
		    {
			    // The creature can go from the world before the start of the turn
			    if (openblack::Locator::creatureMindSystem::has_value() && registry.Valid(action.creature))
			    {
				    openblack::Locator::creatureMindSystem::value().SendReleaseFeedback(action.creature, action.amount);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandTake> || std::is_same_v<Kind, HandLetGo> ||
		                       std::is_same_v<Kind, HandTap> || std::is_same_v<Kind, HandGive>)
		    {
			    // The tap or the take of a fireball catches it into the hand of miracles
			    if constexpr (std::is_same_v<Kind, HandTake> || std::is_same_v<Kind, HandTap>)
			    {
				    if (registry.Valid(action.object) &&
				        registry.AllOf<openblack::ecs::components::MagicFireBall>(action.object))
				    {
					    if (openblack::Locator::magicSystem::has_value())
					    {
						    openblack::Locator::magicSystem::value().DoCatchFireBall(command.player, action.object);
					    }
					    return;
				    }
			    }
			    // The hand checks that the thing is still there, and that its own state still asks for the command
			    if (!openblack::Locator::handGrabSystem::has_value())
			    {
				    return;
			    }
			    auto& hand = openblack::Locator::handGrabSystem::value();
			    if constexpr (std::is_same_v<Kind, HandTake>)
			    {
				    hand.DoTake(command.player, action.object);
			    }
			    else if constexpr (std::is_same_v<Kind, HandLetGo>)
			    {
				    hand.DoLetGo(command.player, action);
			    }
			    else if constexpr (std::is_same_v<Kind, HandTap>)
			    {
				    hand.DoTap(command.player, action.object);
			    }
			    else
			    {
				    hand.DoGive(command.player, action.creature, action.object, action.leftHand);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, LeashOrder>)
		    {
			    if (openblack::Locator::leashSystem::has_value())
			    {
				    openblack::Locator::leashSystem::value().DoOrder(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, CreatureBruise>)
		    {
			    if (openblack::Locator::creatureHandSystem::has_value() && registry.Valid(action.creature))
			    {
				    openblack::Locator::creatureHandSystem::value().DoBruise(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, SeedGesture>)
		    {
			    if (openblack::Locator::magicSystem::has_value())
			    {
				    openblack::Locator::magicSystem::value().DoSeedGesture(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandCast>)
		    {
			    if (openblack::Locator::magicSystem::has_value())
			    {
				    openblack::Locator::magicSystem::value().DoCast(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandCastUnlock>)
		    {
			    if (openblack::Locator::magicSystem::has_value())
			    {
				    openblack::Locator::magicSystem::value().DoCastUnlock(command.player, action.seed);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, GripDust>)
		    {
			    // The puff of dust is the spot visual of a grip of the land. Each computer and the game server make it at the
			    // same turn, so its random numbers come from the shared seed.
			    if (openblack::Locator::particleSystem::has_value())
			    {
				    openblack::Locator::particleSystem::value().StartSpotVisual(openblack::SpotVisualType::GripLandscape,
				                                                                action.point, std::nullopt, entt::null, 1.0f);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, ChatSpeech> || std::is_same_v<Kind, ChatTaunt>)
		    {
			    // A line of the chat changes nothing of the synced world
			    if (openblack::Locator::chatSystem::has_value())
			    {
				    openblack::Locator::chatSystem::value().Receive(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandClick>)
		    {
			    if (openblack::Locator::handGrabSystem::has_value())
			    {
				    openblack::Locator::handGrabSystem::value().DoClick(command.player, action,
				                                                        openblack::Locator::time::value().GetTurn());
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, CreatureReaction>)
		    {
			    if (openblack::Locator::creatureHandSystem::has_value())
			    {
				    openblack::Locator::creatureHandSystem::value().DoReaction(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, FightGround>)
		    {
			    if (openblack::Locator::creatureFightSystem::has_value())
			    {
				    openblack::Locator::creatureFightSystem::value().DoGroundPress(command.player, action.creature,
				                                                                   action.point, action.replace);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, FightMove>)
		    {
			    if (openblack::Locator::creatureFightSystem::has_value())
			    {
				    openblack::Locator::creatureFightSystem::value().DoMove(
				        command.player, action.target,
				        {.kind = static_cast<openblack::creature_fight::Move::Kind>(action.kind), .value = action.value},
				        action.replace);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, FightCharge>)
		    {
			    if (openblack::Locator::creatureFightSystem::has_value())
			    {
				    openblack::Locator::creatureFightSystem::value().DoCharge(command.player,
				                                                              static_cast<float>(action.heldMs));
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, TotemShare>)
		    {
			    if (openblack::Locator::villageTotemSystem::has_value() && registry.Valid(action.totem))
			    {
				    openblack::Locator::villageTotemSystem::value().DoShare(command.player, action.totem, action.share);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandApply>)
		    {
			    // The miracle in the hand is used on the thing, else the thing that the hand holds
			    if (openblack::Locator::magicSystem::has_value() &&
			        openblack::Locator::magicSystem::value().DoApplySeed(command.player, action.target))
			    {
				    return;
			    }
			    if (openblack::Locator::handGrabSystem::has_value() && registry.Valid(action.target))
			    {
				    openblack::Locator::handGrabSystem::value().DoApply(command.player, action.target);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandLock> || std::is_same_v<Kind, HandUnlock>)
		    {
			    // The hand holds a creature to stroke or slap it, and scoops from anything else. The hand checks that the
			    // thing is still there, and that it still asks for the lock.
			    // A town's totem: the world sees the hand take hold of it, and its end needs nothing more
			    if (registry.Valid(action.object) && registry.AllOf<openblack::ecs::components::VillageTotem>(action.object))
			    {
				    if constexpr (std::is_same_v<Kind, HandLock>)
				    {
					    if (openblack::Locator::villageTotemSystem::has_value())
					    {
						    openblack::Locator::villageTotemSystem::value().DoGrip(command.player, action.object);
					    }
				    }
				    return;
			    }
			    if (registry.Valid(action.object) && registry.AllOf<openblack::ecs::components::Creature>(action.object))
			    {
				    if (openblack::Locator::creatureHandSystem::has_value())
				    {
					    auto& creatures = openblack::Locator::creatureHandSystem::value();
					    if constexpr (std::is_same_v<Kind, HandLock>)
					    {
						    creatures.DoLock(command.player, action.object);
					    }
					    else
					    {
						    creatures.DoUnlock(command.player, action.object);
					    }
				    }
				    return;
			    }
			    if (!openblack::Locator::handGrabSystem::has_value())
			    {
				    return;
			    }
			    if constexpr (std::is_same_v<Kind, HandLock>)
			    {
				    openblack::Locator::handGrabSystem::value().DoLock(command.player, action.object);
			    }
			    else
			    {
				    openblack::Locator::handGrabSystem::value().DoUnlock(command.player, action.object);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandTwist>)
		    {
			    if (openblack::Locator::handGrabSystem::has_value())
			    {
				    openblack::Locator::handGrabSystem::value().DoTwist(command.player, action.object, action.moved);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandReport>)
		    {
			    if (openblack::Locator::handSystem::has_value())
			    {
				    openblack::Locator::handSystem::value().Report(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, ViewReport>)
		    {
			    if (openblack::Locator::handSystem::has_value())
			    {
				    openblack::Locator::handSystem::value().ReportView(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HelpReport>)
		    {
			    // Only the computer of the player counts what the player did
			    if (openblack::Locator::helpProfileSystem::has_value() && PlayerOfThisComputer(command.player))
			    {
				    openblack::Locator::helpProfileSystem::value().DoReport(action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HelpQuery>)
		    {
			    // Only the computer of the player says the help
			    if (openblack::Locator::helpMessageSystem::has_value() && PlayerOfThisComputer(command.player))
			    {
				    openblack::Locator::helpMessageSystem::value().DoHelpQuery(action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, PlayerJoined>)
		    {
			    // The god of the player gets hands of its own
			    if (openblack::Locator::handSystem::has_value())
			    {
				    std::ignore = openblack::Locator::handSystem::value().MakeHandsOf(command.player);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, PlayerLeft>)
		    {
			    // The god of the player is defeated, and its slot stays closed. A land can have no god for the slot.
			    std::optional<entt::entity> god;
			    registry.Each<const openblack::ecs::components::Player>(
			        [&god, &command](entt::entity entity, const openblack::ecs::components::Player& player) {
				        if (player.name == command.player)
				        {
					        god = entity;
				        }
			        });
			    if (god.has_value() && !registry.AllOf<openblack::ecs::components::LeftGame>(*god))
			    {
				    registry.Assign<openblack::ecs::components::LeftGame>(*god, turn);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandPick>)
		    {
			    if (openblack::Locator::handSystem::has_value())
			    {
				    openblack::Locator::handSystem::value().ReportPick(command.player, action.object);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, MiracleRequest> || std::is_same_v<Kind, MiracleRepeat>)
		    {
			    if (!openblack::Locator::spellIconSystem::has_value())
			    {
				    return;
			    }
			    // The turn checks again that an icon of the player may give the miracle, then asks for it. A repeat asks
			    // for the last miracle that went into the player's hand, as the player is at this turn.
			    auto& icons = openblack::Locator::spellIconSystem::value();
			    auto seed = openblack::SpellSeedType::None;
			    if constexpr (std::is_same_v<Kind, MiracleRequest>)
			    {
				    seed = action.seed;
			    }
			    else
			    {
				    seed = icons.LastSeedGained(command.player);
			    }
			    if (icons.ValidForRequest(command.player, seed))
			    {
				    icons.RequestSeed(command.player, seed);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, LeashPick>)
		    {
			    // The creature of the player changes to the leash that the picker chose, if it knows that leash and no
			    // nice or nasty spell is on it
			    if (openblack::Locator::leashSystem::has_value())
			    {
				    openblack::Locator::leashSystem::value().PickType(command.player, action.type);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, HandShakeOff>)
		    {
			    if (openblack::Locator::handGrabSystem::has_value())
			    {
				    openblack::Locator::handGrabSystem::value().DoShakeOff(command.player);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, CreatureFightCheat>)
		    {
			    if (openblack::Locator::creatureFightSystem::has_value())
			    {
				    openblack::Locator::creatureFightSystem::value().DoFightCheat(command.player);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, TempleLeashOff>)
		    {
			    // The player went into their temple: the turn checks again that the player holds the leash
			    if (openblack::Locator::leashSystem::has_value())
			    {
				    openblack::Locator::leashSystem::value().TakeOffAtTemple(command.player);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, LeashPostTap>)
		    {
			    if (openblack::Locator::leashSystem::has_value() && registry.Valid(action.post))
			    {
				    openblack::Locator::leashSystem::value().TapPost(command.player, action.post);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, GiveCallOff>)
		    {
			    if (openblack::Locator::handGrabSystem::has_value())
			    {
				    openblack::Locator::handGrabSystem::value().DoGiveCallOff(command.player, action.creature);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, CreatureTattoo>)
		    {
			    if (openblack::Locator::creatureSkinSystem::has_value() && registry.Valid(action.creature))
			    {
				    openblack::Locator::creatureSkinSystem::value().DoTattoo(command.player, action);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, LeashLength>)
		    {
			    if (openblack::Locator::leashSystem::has_value())
			    {
				    openblack::Locator::leashSystem::value().DoLeashLength(command.player, action.length);
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, QuickSave> || std::is_same_v<Kind, QuickLoad>)
		    {
			    // Only the player of this computer saves or loads the games of this computer
			    if (!openblack::Locator::saveGameSystem::has_value() || !PlayerOfThisComputer(command.player))
			    {
				    return;
			    }
			    auto& saves = openblack::Locator::saveGameSystem::value();
			    if constexpr (std::is_same_v<Kind, QuickSave>)
			    {
				    std::ignore = saves.DoQuickSave();
			    }
			    else
			    {
				    saves.DoQuickLoad();
			    }
		    }
		    else if constexpr (std::is_same_v<Kind, MusicMood>)
		    {
			    if (openblack::Locator::musicMoodSystem::has_value())
			    {
				    openblack::Locator::musicMoodSystem::value().DoMusicMood(action);
			    }
		    }
	    },
	    command.action);
	if (const auto words = LeashCommandWords(command.action); words.has_value())
	{
		LogLeashCommand(command.player, *words, turn);
	}
}

} // namespace

void PlayerCommandSystem::Submit(Action action)
{
	// A network game sends the command to the server at once (MI-2)
	if (_source != nullptr)
	{
		_source->Send(action);
		return;
	}
	_queue.Submit(_sender, std::move(action));
}

void PlayerCommandSystem::SendOpenBlock()
{
	_queue.Send();
}

void PlayerCommandSystem::SetSource(TurnCommandSourceInterface* source)
{
	_source = source;
	// The commands that waited for a single player turn do not go into the network game
	_queue.Clear();
}

void PlayerCommandSystem::RunSent(uint32_t turn)
{
	auto commands = _source != nullptr ? _source->TakeTurn(turn) : _queue.TakeSent();
	if (const auto received = _received.find(turn); received != _received.end())
	{
		commands.insert(commands.end(), received->second.begin(), received->second.end());
		_received.erase(received);
	}
	if (_listener)
	{
		_listener(turn, commands);
	}
	std::vector<PlayerNames> joined;
	for (const auto& command : commands)
	{
		Run(command, turn);
		if (std::holds_alternative<PlayerJoined>(command.action))
		{
			joined.push_back(command.player);
		}
	}
	// The humans of the network session joined at the start of the game: each gets its player, and the slots with no
	// player fill
	if (!joined.empty() && openblack::Locator::computerPlayerSystem::has_value())
	{
		openblack::Locator::computerPlayerSystem::value().HumansJoined(joined);
	}
	StartGodsAtSentAlignments(joined);
	BringCreatures(joined);
	// The creature of a skirmish comes after the commands of the turn, before the computer gods start
	if (std::exchange(_playersCreatureDue, false) && _source == nullptr)
	{
		BringPlayersCreature();
	}
}

void PlayerCommandSystem::BringPlayersCreature()
{
	if (!openblack::Locator::creatureCarryOverSystem::has_value())
	{
		return;
	}
	auto& carryOver = openblack::Locator::creatureCarryOverSystem::value();
	openblack::ecs::creature_from_file::BringKept(_sender, carryOver.KeptOrRead());
}

void PlayerCommandSystem::StartGodsAtSentAlignments(std::span<const PlayerNames> joined)
{
	if (_source == nullptr || joined.empty())
	{
		return;
	}
	// Each god starts at the alignment that its player sent before the start, and a god with no player at 0. Then the
	// players' statistics note the alignments they start the game at. The creatures come after this.
	auto& registry = openblack::Locator::entitiesRegistry::value();
	openblack::profile_alignment::StartGods(registry, [this](PlayerNames god) { return _source->StartAlignment(god); });
	openblack::player_statistics::GameStarts(registry);
}

void PlayerCommandSystem::BringCreatures(std::span<const PlayerNames> joined)
{
	if (_source == nullptr)
	{
		return;
	}
	// After the slots with no player filled, the god of each human brings the creature that its player sent before the
	// start, in the order of the joins
	for (const auto god : joined)
	{
		const auto file = _source->StartCreatureFile(god);
		if (file.empty())
		{
			continue;
		}
		const auto brought = openblack::ecs::creature_from_file::Bring(god, file);
		// Only the creature of this computer's own god came from this player's files
		if (brought.creature.has_value() && _source->OwnGod() == god &&
		    openblack::Locator::creatureCarryOverSystem::has_value())
		{
			openblack::Locator::creatureCarryOverSystem::value().SetBroughtCreature(*brought.creature);
		}
	}
}

void PlayerCommandSystem::Receive(uint32_t turn, std::vector<Command> commands)
{
	auto& waiting = _received[turn];
	waiting.insert(waiting.end(), std::make_move_iterator(commands.begin()), std::make_move_iterator(commands.end()));
}

void PlayerCommandSystem::Reset()
{
	_queue.Clear();
}
