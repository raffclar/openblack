/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// On the player's hand: how the draw of the hand sees the hold of a creature (see creature_hand_draw). The draw sees the
/// press and the release in their frame, after the turns and the send of the commands. Only this computer keeps it.
/// The world does not read it.
struct HandCreatureDraw
{
	/// The creature that the draw last saw the hand hold
	entt::entity creature {entt::null};
	/// The draw saw the start of the hold and did not see its end yet
	bool holding {false};
	/// A command, not the button, holds the hand to the creature now or did so at a frame of this hold
	bool byCommand {false};
	/// The whole milliseconds that the draw counted since it saw the start of the hold
	uint32_t heldMs {0};
	/// The hand has its pose on the creature: the world holds the creature, and the creature finished what it did
	bool touching {false};
};

} // namespace openblack::ecs::components
