/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>

/// How the frame draws a tree that a forester felled. The game draws such a tree two times in each frame at the same
/// place. The first draw has the look of a burning thing when the tree burns (charred and with the glow of its fire),
/// else the usual look. The second draw always has the usual look. The second draw passes the depth test where it is at
/// the same depth as the first, so it covers the first draw. The first draw shows only where the second blends over it:
/// at the texels that blend by their alpha, and everywhere when the tree fades out in the distance. These rules use
/// only the frames: the game turns do not read them.
namespace openblack::ecs::felled_tree_draw
{

/// The look of one draw of a felled tree
enum class Look : uint8_t
{
	/// Charred and with the glow of its fire when it burns, else as Usual
	AsBurning,
	/// The land's light and haze alone, with no charring and no glow
	Usual,
};

/// The number of draws of a felled tree in each frame, thus the number of its instances
inline constexpr uint32_t k_Draws = 2;

/// The looks of the draws of a felled tree, in the order that the frame draws them. The second draw passes the depth test
/// at the depth of the first.
inline constexpr std::array<Look, k_Draws> k_Looks = {Look::AsBurning, Look::Usual};

} // namespace openblack::ecs::felled_tree_draw
