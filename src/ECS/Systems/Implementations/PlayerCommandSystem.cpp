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

#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
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
