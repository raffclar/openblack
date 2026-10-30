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

/// The colour of a god after the seventh, 0xFFRRGGBB: the chat colour of its god slot (deviation 164). None for a
/// player of a land, which keeps the colour of the game's tables.
[[nodiscard]] constexpr std::optional<uint32_t> LaterGodColour(PlayerNames player)
{
	if (player_slots::IsLandPlayer(player))
	{
		return std::nullopt;
	}
	const auto slot = player_slots::SlotOfPlayer(player);
	if (!slot.has_value() || *slot >= k_GodChatColours.size())
	{
		return std::nullopt;
	}
	return k_GodChatColours.at(*slot);
}

/// The colour of a player, 0xAARRGGBB, by player number. The table has a colour for each player slot of a land. A god
/// after the seventh has the chat colour of its god slot. A number past the gods takes the colour of its three low bits,
/// as the masked colour lookups of the game do.
[[nodiscard]] constexpr uint32_t Of(PlayerNames player)
{
	if (const auto later = LaterGodColour(player); later.has_value())
	{
		return *later;
	}
	const auto& colours = ecs::components::Player::k_Colours;
	static_assert((colours.size() & (colours.size() - 1)) == 0, "The mask of the colour table needs a power of two");
	return colours.at(static_cast<size_t>(player) & (colours.size() - 1));
}

/// The colour of a player, 0xRRGGBB, from a table of the colours of the players of a land: a god after the seventh has
/// the chat colour of its god slot, and any other player the colour of its three low bits
[[nodiscard]] constexpr uint32_t FromTable(const std::array<uint32_t, player_slots::k_LandSlots>& table, PlayerNames player)
{
	if (const auto later = LaterGodColour(player); later.has_value())
	{
		return *later & 0xFFFFFFu;
	}
	return table.at(static_cast<size_t>(player) & (table.size() - 1));
}

/// The colour of a player on a land, 0xFFRRGGBB. Some lands of the story give their second and third players the
/// colours of other players; the other lands give each player its own colour.
[[nodiscard]] constexpr uint32_t OnLand(PlayerNames player, int32_t landNumber)
{
	constexpr size_t k_Yellow = 4;
	constexpr size_t k_Blue = 5;
	constexpr size_t k_Orange = 6;
	if (const auto later = LaterGodColour(player); later.has_value())
	{
		return *later | 0xFF000000u;
	}
	auto index = static_cast<size_t>(player);
	if (index == 1 && (landNumber == 1 || landNumber == 4 || landNumber == 5))
	{
		index = k_Orange;
	}
	else if (index == 1 && landNumber == 2)
	{
		index = k_Yellow;
	}
	else if ((index == 2 && landNumber == 2) || (index == 1 && landNumber == 3))
	{
		index = k_Blue;
	}
	const auto& colours = ecs::components::Player::k_Colours;
	return colours.at(std::min(index, colours.size() - 1)) | 0xFF000000u;
}

} // namespace openblack::player_colour
