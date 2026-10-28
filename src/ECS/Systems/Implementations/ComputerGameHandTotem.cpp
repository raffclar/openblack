/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerGameHandTotem.h"

#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems::computer_world;

GameHandTotem::GameHandTotem(PlayerNames player, ComputerPlayer& state)
    : _player(player)
    , _state(state)
{
}

void GameHandTotem::RaiseTotem(entt::entity totem, float share)
{
	if (Locator::villageTotemSystem::has_value())
	{
		Locator::villageTotemSystem::value().RaiseByGod(_player, totem, share);
	}
}
