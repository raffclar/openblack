/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>

#include "PlayerSlots.h"

/// The players of the game that runs now, for the walks of each player. A land and a game of up to 7 gods have the 8
/// players of a land. A network game with more gods has the numbers of the later gods too.
namespace openblack::ecs::game_players
{

/// The count of the player numbers of this game. Without a player system, the player slots of a land.
[[nodiscard]] size_t Count();

/// Each player of this game, in the order of their numbers
[[nodiscard]] inline auto All()
{
	return player_slots::Players(Count());
}

/// Each god of this game, in the order of their numbers: each player but the neutral player
[[nodiscard]] inline auto Gods()
{
	return player_slots::Gods(Count());
}

/// The number of the land that runs now. Without a registry, no land of the story.
[[nodiscard]] int32_t LandNumber();

/// The colour of a player in the world of this game, 0xFFRRGGBB: the colour of the player that the land gives
[[nodiscard]] uint32_t Colour(PlayerNames player);

/// The colour of a player in the world of this game, 0xRRGGBB, from a table of the colours of the players of a land
[[nodiscard]] uint32_t Colour(const std::array<uint32_t, player_slots::k_LandSlots>& table, PlayerNames player);

} // namespace openblack::ecs::game_players
