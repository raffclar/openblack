/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>

#include <entt/entity/entity.hpp>

#include "ECS/Components/HandCreatureDraw.h"

/// The draw of the hand and its hold of a creature, in a game alone. The input of a frame sees the press and the release
/// of the button. The draw of the hand sees them in the same frame, after the turns and after the send of the commands.
/// In the draw the hand sends the leash tap of a click and then the feedback of the strokes and slaps. Thus the next
/// frame sends them, after its turns, one frame after the release.
namespace openblack::creature_hand_draw
{

/// What the draw of the hand sees in one frame
struct Frame
{
	/// The creature that the input holds the hand to in this frame, none when the hand does not hold a creature
	std::optional<entt::entity> held;
	/// The time that the draw counts in this frame, in whole milliseconds
	std::chrono::milliseconds time {0};
	/// A command, not the button, holds the hand to the creature
	bool byCommand {false};
	/// The world holds the creature: a turn did the lock of the press
	bool worldHolds {false};
	/// The creature did not finish what it did before the hold, and does not face the player yet
	bool finishing {false};
};

/// What the draw of the hand sends in one frame
struct Sends
{
	/// The draw saw the end of a hold of the button within the time of a click: the leash tap of this creature. Only the
	/// creature of the player of this computer takes it.
	std::optional<entt::entity> click;
	/// The hand left its pose on this creature: the creature feels the strokes and the slaps, if it is not still
	std::optional<entt::entity> feedback;
};

/// Do the draw of the hand for one frame. The draw counts the time of the frame first. Then it sees the start or the
/// end of the hold, and then it puts the hand into its pose on the creature or takes the hand out of it.
[[nodiscard]] Sends Step(ecs::components::HandCreatureDraw& draw, const Frame& frame);

} // namespace openblack::creature_hand_draw
