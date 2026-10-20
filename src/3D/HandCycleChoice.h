/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "3D/HandAnimation.h"
#include "3D/HandNavigationPose.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/LeashSnap.h"

/// The cycle that the hand plays in a frame: its pose that moves the camera, the feel of a thing under it, the open
/// fingers over a miracle's icon, the pinch of the leash's rope, or the touch of a creature that it holds. Pure rules,
/// tested without the game.
namespace openblack::hand_cycle
{

/// What the hand does and what is under it in a frame
struct Facts
{
	hand_navigation_pose::Pose pose {hand_navigation_pose::Pose::Idle};
	/// The hand moves the camera
	bool cameraState {false};
	/// A thing with a model of its own, or a one-shot bubble, is under the hand
	bool overThing {false};
	/// A miracle's icon is under the hand
	bool overSpellIcon {false};
	/// Where the hand is to the rope of the tied leash
	leash_snap::Hand rope {leash_snap::Hand::Away};
	/// The touch of the creature that the hand holds, none while it holds no creature
	std::optional<creature_feedback::TouchCycle> creature;
};

/// The cycle, by these rules in turn. The last rule that applies wins.
/// 1. The pose that moves the camera gives the grip, the turn, the tilt or the zoom, and the idle pose the wiggle.
/// 2. Out of the camera state, a thing under the hand gives the feel of the thing.
/// 3. Out of the camera state, a miracle's icon gives the open fingers.
/// 4. Out of the camera state, the rope of the leash gives the pinch. A hand that holds the rope over a thing or an icon
///    keeps the cycle of that thing or icon.
/// 5. A creature in the hand gives its touch.
[[nodiscard]] inline HandAnimation::Cycle Choose(const Facts& facts)
{
	using Cycle = HandAnimation::Cycle;
	using Pose = hand_navigation_pose::Pose;
	auto cycle = Cycle::Wiggle;
	switch (facts.pose)
	{
	case Pose::Idle:
		break;
	case Pose::Grip:
		cycle = Cycle::Grip;
		break;
	case Pose::Rotate:
		cycle = Cycle::Rotate;
		break;
	case Pose::Pitch:
		cycle = Cycle::Pitch;
		break;
	case Pose::Zoom:
		cycle = Cycle::Zoom;
		break;
	}
	if (!facts.cameraState && facts.overThing)
	{
		cycle = Cycle::Stroke;
	}
	if (!facts.cameraState && facts.overSpellIcon)
	{
		cycle = Cycle::CanPickUp;
	}
	const bool feelsThing = facts.rope == leash_snap::Hand::Holding && (facts.overThing || facts.overSpellIcon);
	if (!facts.cameraState && facts.rope != leash_snap::Hand::Away && !feelsThing)
	{
		cycle = Cycle::HoldFingers;
	}
	if (facts.creature.has_value())
	{
		switch (*facts.creature)
		{
		case creature_feedback::TouchCycle::Wiggle:
			cycle = Cycle::Wiggle;
			break;
		case creature_feedback::TouchCycle::Tickle:
			cycle = Cycle::Tickle;
			break;
		case creature_feedback::TouchCycle::Slap:
			cycle = Cycle::Slap;
			break;
		}
	}
	return cycle;
}

} // namespace openblack::hand_cycle
