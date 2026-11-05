/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GamePlayers.h"

#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

size_t openblack::ecs::game_players::Count()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().PlayerNumbersInGame()
	                                          : player_slots::k_LandSlots;
}

int32_t openblack::ecs::game_players::LandNumber()
{
	return Locator::entitiesRegistry::has_value() ? Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber
	                                              : 0;
}

uint32_t openblack::ecs::game_players::Colour(PlayerNames player)
{
	return player_colour::OnLand(player, LandNumber());
}

uint32_t openblack::ecs::game_players::Colour(const std::array<uint32_t, player_slots::k_LandSlots>& table, PlayerNames player)
{
	return player_colour::FromTableOnLand(table, player, LandNumber());
}
