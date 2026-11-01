/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <tuple>

#include <gtest/gtest.h>

#include "Creature/CreatureHandPose.h"
#include "ECS/Components/HandCreaturePose.h"

using namespace openblack;
using namespace openblack::creature_hand_pose;

namespace
{

/// A draw before the turn that holds the creature
constexpr Frame k_BeforeHold {.worldHolds = false, .finishing = false};
/// A draw after the turn that holds the creature, which still finishes what it did
constexpr Frame k_HeldFinishing {.worldHolds = true, .finishing = true};
/// A draw after the turn that holds the creature, which faces the player
constexpr Frame k_HeldFacing {.worldHolds = true, .finishing = false};

/// A model of the frames of a hold: the frame of the input, then the turn that is due, then the draw of the hand. The
/// press asks for the hold in the input of frame 0 and the turn of a later frame does it.
struct HoldFrames
{
	/// The frame whose turn does the hold of the world
	uint32_t holdTurnFrame;
	/// The frame whose turn tells that the creature faces the player
	uint32_t facingTurnFrame;
	/// The frame whose input lets go of the button, if any
	std::optional<uint32_t> releaseFrame;

	struct Result
	{
		std::optional<uint32_t> cameraFrame;
		std::optional<uint32_t> poseFrame;
		std::optional<uint32_t> letGoFrame;
	};

	[[nodiscard]] Result Run(uint32_t frames) const
	{
		Result result;
		ecs::components::HandCreaturePose pose;
		for (uint32_t frame = 0; frame < frames && !result.letGoFrame.has_value(); ++frame)
		{
			// The input: the release of the button, then a release that waited
			if (releaseFrame == frame && LetsGoNow(pose))
			{
				result.letGoFrame = frame;
				break;
			}
			if (WaitedReleaseDue(pose))
			{
				result.letGoFrame = frame;
				break;
			}
			// The turn, then the draw
			const Frame seen {.worldHolds = frame >= holdTurnFrame, .finishing = frame < facingTurnFrame};
			const auto changes = Step(pose, seen);
			if (changes.takeCamera)
			{
				result.cameraFrame = frame;
			}
			if (changes.enterPose)
			{
				result.poseFrame = frame;
			}
		}
		return result;
	}
};

} // namespace

// The draw that first sees the hold of the world turns the camera of the hold. It does this one time.
TEST(CreatureHandPose, TheCameraTurnsInTheFirstDrawAfterTheHoldOfTheWorld)
{
	ecs::components::HandCreaturePose pose;
	EXPECT_FALSE(Step(pose, k_BeforeHold).takeCamera);
	EXPECT_FALSE(pose.cameraTaken);
	EXPECT_TRUE(Step(pose, k_HeldFinishing).takeCamera);
	EXPECT_TRUE(pose.cameraTaken);
	EXPECT_FALSE(Step(pose, k_HeldFinishing).takeCamera);
	EXPECT_FALSE(Step(pose, k_HeldFacing).takeCamera);
}

// The hand takes its pose only when the world holds the creature and the creature faces the player
TEST(CreatureHandPose, TheHandTakesItsPoseOnlyWhenTheCreatureFacesThePlayer)
{
	ecs::components::HandCreaturePose pose;
	// A creature that faces the player before the hold of the world gives no pose
	EXPECT_FALSE(Step(pose, Frame {.worldHolds = false, .finishing = false}).enterPose);
	EXPECT_FALSE(Step(pose, k_HeldFinishing).enterPose);
	EXPECT_FALSE(pose.posed);
	EXPECT_TRUE(Step(pose, k_HeldFacing).enterPose);
	EXPECT_TRUE(pose.posed);
	// The hand keeps the pose, and it does not start it again
	EXPECT_FALSE(Step(pose, k_HeldFinishing).enterPose);
	EXPECT_TRUE(pose.posed);
}

// A creature that faces the player at the turn of the hold gets the camera and the pose in the same draw
TEST(CreatureHandPose, TheCameraAndThePoseCanStartInOneDraw)
{
	ecs::components::HandCreaturePose pose;
	const auto changes = Step(pose, k_HeldFacing);
	EXPECT_TRUE(changes.takeCamera);
	EXPECT_TRUE(changes.enterPose);
}

// A release before the camera of the hold waits. It is due in the frame after the draw that turns the camera.
TEST(CreatureHandPose, AReleaseBeforeTheCameraWaitsForIt)
{
	ecs::components::HandCreaturePose pose;
	EXPECT_FALSE(LetsGoNow(pose));
	EXPECT_TRUE(pose.releaseWaits);
	EXPECT_FALSE(WaitedReleaseDue(pose));
	std::ignore = Step(pose, k_BeforeHold);
	EXPECT_FALSE(WaitedReleaseDue(pose));
	std::ignore = Step(pose, k_HeldFinishing);
	EXPECT_TRUE(WaitedReleaseDue(pose));
}

// After the camera of the hold, the release lets go at once
TEST(CreatureHandPose, AReleaseAfterTheCameraLetsGoAtOnce)
{
	ecs::components::HandCreaturePose pose;
	std::ignore = Step(pose, k_HeldFinishing);
	EXPECT_TRUE(LetsGoNow(pose));
	EXPECT_FALSE(pose.releaseWaits);
	EXPECT_FALSE(WaitedReleaseDue(pose));
}

// The frames of a hold: the press in frame 0, the hold of the world at the turn of frame 6 and the creature facing the
// player at the turn of frame 12. The camera turns in frame 6 and the pose starts in frame 12.
TEST(CreatureHandPose, TheFramesOfAHold)
{
	const auto result = HoldFrames {.holdTurnFrame = 6, .facingTurnFrame = 12, .releaseFrame = std::nullopt}.Run(40);
	EXPECT_EQ(result.cameraFrame, 6u);
	EXPECT_EQ(result.poseFrame, 12u);
	EXPECT_FALSE(result.letGoFrame.has_value());
}

// A quick click: the button lets go in frame 2, before the hold of the world at the turn of frame 6. The camera turns in
// the draw of frame 6 and the hand lets go in frame 7, with no pose.
TEST(CreatureHandPose, AQuickClickLetsGoInTheFrameAfterTheCamera)
{
	const auto result = HoldFrames {.holdTurnFrame = 6, .facingTurnFrame = 12, .releaseFrame = 2u}.Run(40);
	EXPECT_EQ(result.cameraFrame, 6u);
	EXPECT_FALSE(result.poseFrame.has_value());
	EXPECT_EQ(result.letGoFrame, 7u);
}

// A release after the camera lets go in its own frame
TEST(CreatureHandPose, ALateReleaseLetsGoInItsOwnFrame)
{
	const auto result = HoldFrames {.holdTurnFrame = 6, .facingTurnFrame = 12, .releaseFrame = 20u}.Run(40);
	EXPECT_EQ(result.cameraFrame, 6u);
	EXPECT_EQ(result.poseFrame, 12u);
	EXPECT_EQ(result.letGoFrame, 20u);
}
