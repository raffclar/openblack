/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// The one test of whether the body of a creature does an action. The game keeps one state for the body. The state is
/// "stand" or one of the actions: a move, a static pose, a dance, a point, an action on a thing, a kiss, a catch, a
/// fight move, a faint, a turn on the spot. openblack keeps these in three parts, so the test reads all three.
namespace openblack::creature_body
{

/// What the body of a creature does now, from the three parts that play it
struct Activity
{
	/// The body plays an action of its own: a static pose, a dance, a kiss, a fight move, a faint or the stand up, a
	/// reaction, or the confused look of a walk that waits for its route
	bool bodyPlays {false};
	/// The body does an action on a thing: it plays the animations, or it turned on the spot for the action. A catch
	/// is such an action from its start.
	bool objectActs {false};
	/// It waits for a route, walks or turns on the spot
	bool moving {false};
};

/// Whether the body does an action. The body is free only when it stands. Each other state of the body (a move
/// included) makes the creature busy.
[[nodiscard]] constexpr bool PerformsBodyAction(const Activity& activity)
{
	return activity.bodyPlays || activity.objectActs || activity.moving;
}

/// Whether the body does an action that is not only its move
[[nodiscard]] constexpr bool ActsBesidesMoving(const Activity& activity)
{
	return activity.bodyPlays || activity.objectActs;
}

} // namespace openblack::creature_body
