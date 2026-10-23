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
#include <type_traits>
#include <utility>

#include "ECS/Components/Creature.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using namespace openblack::player_commands;

namespace
{

/// Do one command for the player that sent it
void Run(const Command& command)
{
	const auto& registry = openblack::Locator::entitiesRegistry::value();
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
	_queue.Submit(_sender, std::move(action));
}

void PlayerCommandSystem::SendOpenBlock()
{
	_queue.Send();
}

void PlayerCommandSystem::RunSent(uint32_t turn)
{
	auto commands = _queue.TakeSent();
	if (const auto received = _received.find(turn); received != _received.end())
	{
		commands.insert(commands.end(), received->second.begin(), received->second.end());
		_received.erase(received);
	}
	if (_listener)
	{
		_listener(turn, commands);
	}
	for (const auto& command : commands)
	{
		Run(command);
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
