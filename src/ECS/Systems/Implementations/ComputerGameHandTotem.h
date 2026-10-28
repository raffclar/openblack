/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Components/ComputerPlayer.h"
#include "ECS/ComputerHandTotemWorld.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems::computer_world
{

/// What the hand of the god does with the totems in the game's world
class GameHandTotem final: public computer_hand::TotemWorld
{
public:
	GameHandTotem(PlayerNames player, components::ComputerPlayer& state);

	void RaiseTotem(entt::entity totem, float share) override;

private:
	PlayerNames _player;
	[[maybe_unused]] components::ComputerPlayer& _state;
};

} // namespace openblack::ecs::systems::computer_world
