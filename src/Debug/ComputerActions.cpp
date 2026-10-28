/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerActions.h"

#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Player.h"
#include "ECS/ComputerMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;

bool debug::computer_actions::MakeReady(computer_mind::Mind& mind, std::string_view name)
{
	if (mind.nodes.empty())
	{
		mind = computer_mind::MakeMind();
	}
	const auto found = computer_mind::FindNodes(mind, name);
	if (found.empty())
	{
		return false;
	}
	auto& node = mind.nodes[found.front()];
	if (computer_mind::InfoOf(node.kind).perform && node.childCount == 0)
	{
		node.bestLeaf = found.front();
	}
	return true;
}

bool debug::computer_actions::Force(PlayerNames player, std::string_view name, entt::entity first, entt::entity second)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::computerPlayerSystem::has_value())
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	bool ready = false;
	registry.Each<const ecs::components::Player, ecs::components::ComputerPlayer>(
	    [player, name, &ready](const ecs::components::Player& owner, ecs::components::ComputerPlayer& computer) {
		    if (owner.name == player)
		    {
			    ready = MakeReady(computer.mind, name);
		    }
	    });
	if (ready)
	{
		Locator::computerPlayerSystem::value().QueueAction(player, name, first, second);
	}
	return ready;
}

bool debug::computer_actions::SetUntaughtSends(computer_mind::Mind& mind, size_t kind, uint32_t count)
{
	if (kind >= mind.untaughtSends.size())
	{
		return false;
	}
	mind.untaughtSends.at(kind) = count;
	return true;
}

bool debug::computer_actions::SetUntaughtSends(PlayerNames player, size_t kind, uint32_t count)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	bool set = false;
	Locator::entitiesRegistry::value().Each<const ecs::components::Player, ecs::components::ComputerPlayer>(
	    [player, kind, count, &set](const ecs::components::Player& owner, ecs::components::ComputerPlayer& computer) {
		    if (owner.name == player)
		    {
			    set = SetUntaughtSends(computer.mind, kind, count);
		    }
	    });
	return set;
}
