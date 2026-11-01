/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandDraw.h"

#include <cstdint>

#include <algorithm>
#include <limits>

#include "Creature/CreatureHandRules.h"

using namespace openblack;

creature_hand_draw::Sends creature_hand_draw::Step(ecs::components::HandCreatureDraw& draw, const Frame& frame)
{
	Sends sends;
	// The draw counts the time of each frame, also when the hand holds no creature
	const auto add = static_cast<uint64_t>(frame.time.count() > 0 ? frame.time.count() : 0);
	draw.heldMs = static_cast<uint32_t>(
	    std::min<uint64_t>(static_cast<uint64_t>(draw.heldMs) + add, std::numeric_limits<uint32_t>::max()));

	// The camera of the hold: the draw starts the count when it sees the start of the hold. At the end of the hold, a
	// hold shorter than the time of a click is a click.
	if (frame.held.has_value())
	{
		draw.creature = *frame.held;
		if (!draw.holding)
		{
			draw.holding = true;
			draw.byCommand = false;
			draw.heldMs = 0;
		}
		// A command can take a hold of the button. The release of a command hold sends what it sends itself.
		draw.byCommand = draw.byCommand || frame.byCommand;
	}
	else if (draw.holding)
	{
		draw.holding = false;
		if (!draw.byCommand && static_cast<float>(draw.heldMs) < creature_hand::k_ClickMaxMs)
		{
			sends.click = draw.creature;
		}
	}

	// The pose of the hand on the creature: the hand takes it only when the world holds the creature and the creature
	// finished what it did. When the hand leaves the pose, the creature feels the strokes and the slaps.
	if (frame.held.has_value())
	{
		if (!draw.touching && frame.worldHolds && !frame.finishing)
		{
			draw.touching = true;
		}
	}
	else if (draw.touching)
	{
		draw.touching = false;
		if (!draw.byCommand)
		{
			sends.feedback = draw.creature;
		}
	}
	return sends;
}
