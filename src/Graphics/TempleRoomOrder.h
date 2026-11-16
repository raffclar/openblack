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

namespace openblack::graphics::temple_room_order
{

/// The steps that draw the meshes of the temple's rooms, in the order of k_Order.
///
/// The game draws the room that the player is in first. Then it draws the other room: the room that the camera goes
/// into, or the main room, whole or only its doors. Thus the clear windows of a room hide what is behind them. The
/// windows write depth, and the doors of the main room behind a window fail the depth test.
enum class Step : uint8_t
{
	CurrentRoomSolid,
	CurrentRoomBlended,
	OtherRoomsSolid,
	OtherRoomsBlended,
};

/// The order of the steps in a frame
inline constexpr std::array<Step, 4> k_Order {Step::CurrentRoomSolid, Step::CurrentRoomBlended, Step::OtherRoomsSolid,
                                              Step::OtherRoomsBlended};

/// The step that draws a mesh. A mesh of the room that the player is not in comes after all the meshes of the room that
/// the player is in. A mesh of only blended primitives comes after the solid meshes of its group. Outside the temple,
/// no mesh is of another room.
[[nodiscard]] constexpr Step StepOf(bool otherRoom, bool translucent) noexcept
{
	if (otherRoom)
	{
		return translucent ? Step::OtherRoomsBlended : Step::OtherRoomsSolid;
	}
	return translucent ? Step::CurrentRoomBlended : Step::CurrentRoomSolid;
}

} // namespace openblack::graphics::temple_room_order
