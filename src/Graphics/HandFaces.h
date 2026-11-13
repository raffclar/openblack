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

/// The faces of the player's hand that show. The hand mesh is a left hand. The game makes a right hand when it mirrors
/// the mesh across its x axis. When it mirrors the mesh, it also turns the order of the corners of each triangle. Thus
/// the hand always shows its outer faces, mirrored or not.
namespace openblack::graphics::hand_faces
{

/// The corners of a triangle of the hand mesh as the game keeps them. The game changes the first two corners of each
/// triangle when it mirrors the hand.
[[nodiscard]] constexpr std::array<uint16_t, 3> Corners(const std::array<uint16_t, 3>& corners, bool mirroredHand) noexcept
{
	return mirroredHand ? std::array<uint16_t, 3> {corners[1], corners[0], corners[2]} : corners;
}

/// Whether a draw of the hand must cull the opposite faces to those of a usual model. The mirrored view of the sea turns
/// the faces round. A mirrored hand turns them round again, because its corners are in the opposite order.
[[nodiscard]] constexpr bool FacesTurned(bool mirroredView, bool mirroredHand) noexcept
{
	return mirroredView != mirroredHand;
}

} // namespace openblack::graphics::hand_faces
