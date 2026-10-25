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
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "ECS/Components/Creature.h"
#include "ECS/Components/LeftGame.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
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
				    hand.DoGive(command.player, action.creature, action.object);
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
		    else if constexpr (std::is_same_v<Kind, LeashPostTap>)
		    {
			    if (openblack::Locator::leashSystem::has_value() && registry.Valid(action.post))
			    {
				    openblack::Locator::leashSystem::value().TapPost(action.post);
			    }
		    }
	    },
	    command.action);
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
