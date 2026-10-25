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

#include "ECS/Components/Player.h"
#include "Enums.h"

/// The colour that the game shows for a player on a land
namespace openblack::player_colour
{

/// The colour of a player on a land, 0xFFRRGGBB. Some lands of the story give their second and third players the
/// colours of other players; the other lands give each player its own colour.
[[nodiscard]] constexpr uint32_t OnLand(PlayerNames player, int32_t landNumber)
{
	constexpr size_t k_Yellow = 4;
	constexpr size_t k_Blue = 5;
	constexpr size_t k_Orange = 6;
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
