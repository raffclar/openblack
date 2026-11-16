/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldFileGods.h"

#include <algorithm>
#include <tuple>

#include <spdlog/spdlog.h>

#include "ECS/GamePlayers.h"
#include "ECS/LeaverHand.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::world_file_gods
{

void GiveToRoom(std::span<const PlayerNames> players)
{
	if (!Locator::handSystem::has_value() || !Locator::playerSystem::has_value())
	{
		return;
	}
	auto& hands = Locator::handSystem::value();
	auto& gods = Locator::playerSystem::value();
	for (const auto player : game_players::Gods())
	{
		const God god {.player = player,
		               .roomPlayer = std::ranges::find(players, player) != players.end(),
		               .hands = hands.GetHandsOf(player).has_value(),
		               .computerMind = gods.IsComputerPlayer(player)};
		const auto change = ChangeOf(god);
		if (change.mindOff && Locator::computerPlayerSystem::has_value())
		{
			Locator::computerPlayerSystem::value().GiveToHuman(player);
		}
		if (change.makeHands)
		{
			std::ignore = gods.EnsurePlayer(player);
			std::ignore = hands.MakeHandsOf(player);
		}
		if (change.removeHands)
		{
			std::ignore = leaver_hand::ReleaseHandInGame(player);
			std::ignore = hands.RemoveHandsOf(player);
		}
		if (change != Change {})
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The god of player {} after the world file: {} of the room{}{}{}",
			                   static_cast<int>(player), god.roomPlayer ? "a player" : "no player",
			                   change.makeHands ? ", new hands" : "", change.mindOff ? ", no computer mind" : "",
			                   change.removeHands ? ", no hands" : "");
		}
	}
}

} // namespace openblack::ecs::world_file_gods
