/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// On the player's hand while the button holds it to a creature: how far the hold went (see creature_hand_pose). The
/// press starts it. Only this computer keeps it. The world does not read it.
struct HandCreaturePose
{
	/// The world holds the creature and the camera of the hold turned to it
	bool cameraTaken {false};
	/// The hand has its pose on the creature, and it weighs strokes and slaps
	bool posed {false};
	/// The button let go before the world held the creature. The hand lets go when the camera of the hold is taken.
	bool releaseWaits {false};
};

} // namespace openblack::ecs::components
