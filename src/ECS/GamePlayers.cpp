/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GamePlayers.h"

#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

size_t openblack::ecs::game_players::Count()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().PlayerNumbersInGame()
	                                          : player_slots::k_LandSlots;
}
