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

#include <algorithm>
#include <array>
#include <optional>

#include "ECS/Components/Player.h"
#include "Enums.h"
#include "PlayerSlots.h"

/// The colour that the game shows for a player on a land
namespace openblack::player_colour
{

/// The chat colour of each god of a network game, 0xAARRGGBB, by god slot: the room's colours of deviation 116. The
/// first four are the team colours of the game room: red, green, blue and yellow. The others are far from each other,
/// from the grey of the observers and from the player's own lines, and bright enough to read on the dark chat box. A
/// god earlier in the list is further from the colours before it.
inline constexpr std::array<uint32_t, player_slots::k_MaxPlayers> k_GodChatColours = {
    0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFF00, 0xFFFF33BB, 0xFF3377EE, 0xFF11FFBB, 0xFF559900,
    0xFFDD5566, 0xFFEEFF99, 0xFFEEAAFF, 0xFF00BBFF, 0xFFDD11FF, 0xFF119977, 0xFF66FFFF, 0xFFAA55DD,
    0xFFAAFF55, 0xFF778844, 0xFFFF7700, 0xFFEEAA88, 0xFFFFCC33, 0xFF00FF77, 0xFFBB7799, 0xFFAAEEBB,
    0xFF2299AA, 0xFFFF0077, 0xFF7788CC, 0xFFFF77BB, 0xFF00AA55, 0xFFBBCC33, 0xFFFF0044, 0xFF00BB00,
};

/// The land colour of each god after the seventh, 0xFFRRGGBB, by god slot from the eighth (deviation 164). The land
/// shows these colours, and the chat shows the colours of `k_GodChatColours`. The colours are far from each other, from
/// the 7 land colours of the first gods and from the black and white of the neutral player. Each colour is also far
/// from the chat colour of its god, and bright enough to read on a dark place. A god earlier in the list is further from
/// the colours before it.
inline constexpr std::array<uint32_t, player_slots::k_MaxPlayers - player_slots::k_LandGods> k_LaterGodLandColours = {
    0xFF668844, 0xFFBB6699, 0xFF3388BB, 0xFFFF0099, 0xFF99FFAA, 0xFFFFEE99, 0xFF009911, 0xFFAA7755, 0xFFCCBBFF,
    0xFFAA9900, 0xFF009988, 0xFFDD88FF, 0xFFFF8888, 0xFF889999, 0xFF99DD33, 0xFFFF6600, 0xFF00AA66, 0xFFAADDBB,
    0xFFDDAABB, 0xFFDD3366, 0xFF99DDFF, 0xFF0099FF, 0xFF8855FF, 0xFF8866BB, 0xFFFFCCAA,
};

/// The player whose colour a player shows. The god of the observers shows the colour of player seven, the orange of the
/// observers, also when it is a god after the seventh (deviation 218). Every other player shows its own colour.
[[nodiscard]] constexpr PlayerNames ShownAs(PlayerNames player, std::optional<PlayerNames> observersGod) noexcept
{
	return observersGod == player ? PlayerNames::PLAYER_SEVEN : player;
}

/// The land colour of a god after the seventh, 0xFFRRGGBB, from `k_LaterGodLandColours` by its god slot (deviation
/// 164). None for a player of a land, which keeps the colour of the game's tables.
[[nodiscard]] constexpr std::optional<uint32_t> LaterGodColour(PlayerNames player)
{
	if (player_slots::IsLandPlayer(player))
	{
		return std::nullopt;
	}
	const auto slot = player_slots::SlotOfPlayer(player);
	if (!slot.has_value() || *slot < player_slots::k_LandGods ||
	    *slot - player_slots::k_LandGods >= k_LaterGodLandColours.size())
	{
		return std::nullopt;
	}
	return k_LaterGodLandColours.at(*slot - player_slots::k_LandGods);
}

/// The colour of a player, 0xAARRGGBB, by player number. The table has a colour for each player slot of a land. A god
/// after the seventh has the land colour of its god slot. A number past the gods takes the colour of its three low bits,
/// as the masked colour lookups of the game do.
[[nodiscard]] constexpr uint32_t Of(PlayerNames player)
{
	if (const auto later = LaterGodColour(player); later.has_value())
	{
		return *later;
	}
	using ecs::components::Player;
	static_assert((Player::k_Colours.size() & (Player::k_Colours.size() - 1)) == 0,
	              "The mask of the colour table needs a power of two");
	return Player::k_Colours.at(static_cast<size_t>(player) & (Player::k_Colours.size() - 1));
}

/// The colour of a player, 0xRRGGBB, from a table of the colours of the players of a land: a god after the seventh has
/// the land colour of its god slot, and any other player the colour of its three low bits
[[nodiscard]] constexpr uint32_t FromTable(const std::array<uint32_t, player_slots::k_LandSlots>& table, PlayerNames player)
{
	if (const auto later = LaterGodColour(player); later.has_value())
	{
		return *later & 0xFFFFFFu;
	}
	return table.at(static_cast<size_t>(player) & (table.size() - 1));
}

/// The player whose colour a player shows on a land. Some lands of the story give their second and third players the
/// colours of other players. The other lands give each player its own colour. All the colours of a player in the
/// world come from this player: the symbols, the columns of light, the influence circles and the effects.
[[nodiscard]] constexpr PlayerNames ColourPlayerOnLand(PlayerNames player, int32_t landNumber) noexcept
{
	if (player == PlayerNames::PLAYER_TWO && (landNumber == 1 || landNumber == 4 || landNumber == 5))
	{
		return PlayerNames::PLAYER_SEVEN;
	}
	if (player == PlayerNames::PLAYER_TWO && landNumber == 2)
	{
		return PlayerNames::PLAYER_FIVE;
	}
	if ((player == PlayerNames::PLAYER_THREE && landNumber == 2) || (player == PlayerNames::PLAYER_TWO && landNumber == 3))
	{
		return PlayerNames::PLAYER_SIX;
	}
	return player;
}

/// The colour of a player on a land, 0xFFRRGGBB: the colour of the player that `ColourPlayerOnLand` gives
[[nodiscard]] constexpr uint32_t OnLand(PlayerNames player, int32_t landNumber)
{
	if (const auto later = LaterGodColour(player); later.has_value())
	{
		return *later | 0xFF000000u;
	}
	using ecs::components::Player;
	const auto index = static_cast<size_t>(ColourPlayerOnLand(player, landNumber));
	return Player::k_Colours.at(std::min(index, Player::k_Colours.size() - 1)) | 0xFF000000u;
}

/// The colour of a player on a land, 0xRRGGBB, from a table of the colours of the players of a land. The player is the
/// player that `ColourPlayerOnLand` gives.
[[nodiscard]] constexpr uint32_t FromTableOnLand(const std::array<uint32_t, player_slots::k_LandSlots>& table,
                                                 PlayerNames player, int32_t landNumber)
{
	return FromTable(table, ColourPlayerOnLand(player, landNumber));
}

} // namespace openblack::player_colour
