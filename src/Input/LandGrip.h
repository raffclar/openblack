/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::input::land_grip
{

/// Whether the hand grips the land in this frame. The grip starts with a drag of the land and continues until the Move
/// button is let go. Both buttons together stop the drag but not the grip, so the Move button counts as held while both
/// buttons are held.
[[nodiscard]] constexpr bool GripsLand(bool dragging, bool grippedBefore, bool moveHeld) noexcept
{
	return dragging || (grippedBefore && moveHeld);
}

} // namespace openblack::input::land_grip
