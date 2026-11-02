/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHandPose.h"

using namespace openblack;

creature_hand_pose::Changes creature_hand_pose::Step(ecs::components::HandCreaturePose& pose, const Frame& frame)
{
	Changes changes;
	// The first draw that sees the hold of the world turns the camera. The draw does this one time in a hold, also when
	// the camera cannot turn.
	if (!pose.cameraTaken && frame.worldHolds)
	{
		pose.cameraTaken = true;
		changes.takeCamera = true;
	}
	// The hand takes its pose when the world holds the creature and the creature faces the player. After that, the hand
	// keeps the pose until the end of the hold.
	if (!pose.posed && frame.worldHolds && !frame.finishing)
	{
		pose.posed = true;
		changes.enterPose = true;
	}
	return changes;
}

bool creature_hand_pose::LetsGoNow(ecs::components::HandCreaturePose& pose)
{
	if (pose.cameraTaken)
	{
		return true;
	}
	pose.releaseWaits = true;
	return false;
}

bool creature_hand_pose::WaitedReleaseDue(const ecs::components::HandCreaturePose& pose)
{
	return pose.releaseWaits && pose.cameraTaken;
}
