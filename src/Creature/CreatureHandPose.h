/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Components/HandCreaturePose.h"

/// How a hold of a creature by the button goes on, as the draw of the hand sees it. The press asks the world to hold
/// the creature. A later turn does the hold. The draw after that turn turns the camera to the creature and starts the
/// count of a click. The hand takes its pose on the creature only when the world holds it and the creature finished
/// what it did. Before its pose the hand is the usual hand: it does not stroke or slap. A release before the world holds
/// the creature waits. The hand lets go in the frame after the draw that turned the camera.
namespace openblack::creature_hand_pose
{

/// What the draw of the hand sees in one frame of a hold
struct Frame
{
	/// The world holds the creature: a turn did the hold that the press asked for
	bool worldHolds {false};
	/// The creature did not finish what it did before the hold, and does not face the player yet
	bool finishing {false};
};

/// What changes in one draw of a hold
struct Changes
{
	/// The camera of the hold turns to the creature, and the count of a click starts
	bool takeCamera {false};
	/// The hand takes its pose on the creature with no strokes and no slaps yet
	bool enterPose {false};
};

/// Do the draw of a hold for one frame: first the camera, then the pose
[[nodiscard]] Changes Step(ecs::components::HandCreaturePose& pose, const Frame& frame);

/// The button lets go of the creature. Returns true when the hand lets go now. Before the draw turned the camera, the
/// release waits and this returns false.
[[nodiscard]] bool LetsGoNow(ecs::components::HandCreaturePose& pose);

/// Once a frame, before the turns: whether a release that waited is now due, because an earlier draw turned the camera
[[nodiscard]] bool WaitedReleaseDue(const ecs::components::HandCreaturePose& pose);

} // namespace openblack::creature_hand_pose
