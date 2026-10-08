/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The SuperVillagers' parameters of the smooth drawing: the clip fade state (ecs::StepCrossFade) and its yaw stage
// (ecs::StepYawFollow). Pure functions.

#include <cmath>

#include <bit>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "ECS/Animations.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/MobileDrawing.h"

using openblack::ecs::StepCrossFade;
using openblack::ecs::StepYawFollow;
using CrossFade = openblack::ecs::components::SkeletalAnimation::CrossFade;

// The condition: with the parameters as they are by default every villager and animal is drawn exactly as
// before, bit for bit. The two new stages only run behind these gates (UpdateAnimations: crossFadeMs > 0;
// UpdateMobileDrawing: followRate > 0), so the defaults must keep both closed and the old code path untouched; and
// their results go only to the SuperVillager's own drawn pose and turn (its draw's bone buffer and local matrix
// copy), which an ordinary villager never has: the bones it is drawn with are its pose (the golden test,
// test_villager_draw_golden.cpp, checks the whole path and ecs::DrawnBodyModel against the frozen HEAD code)
TEST(SuperVillagerSmooth, DefaultsLeaveOrdinaryVillagersUnchanged)
{
	openblack::ecs::components::SkeletalAnimation animation {};
	EXPECT_EQ(animation.crossFadeMs, 0);
	EXPECT_FALSE(animation.crossFadeFrozen);
	EXPECT_FALSE(animation.crossFade.hasLast);
	EXPECT_TRUE(animation.drawnPose.empty());
	animation.pose.resize(3);
	EXPECT_EQ(&openblack::ecs::DrawnPose(animation), &animation.pose);
	// a SuperVillager's fade being drawn: the body takes the blend, the plain pose stays for the others
	animation.drawnPose.resize(3);
	EXPECT_EQ(&openblack::ecs::DrawnPose(animation), &animation.drawnPose);
	const openblack::ecs::components::DrawPosition draw {};
	EXPECT_EQ(draw.followRate, 0.0f);
	EXPECT_FALSE(draw.followSnap);
	EXPECT_FALSE(draw.followFrozen);
	EXPECT_FALSE(draw.hasFollowYaw);
	EXPECT_EQ(draw.followDrawnTurn, 0.0f);
}

TEST(SuperVillagerSmooth, CrossFade)
{
	CrossFade fade;
	// the first draw only keeps the clip
	EXPECT_FALSE(StepCrossFade(fade, 11, 16, 300));
	EXPECT_TRUE(fade.hasLast);
	EXPECT_EQ(fade.lastClip, 11u);
	fade.lastTime = 480.0f; // what UpdateAnimations leaves after the pose
	// a change: from the clip and time drawn last, the whole weight on it
	EXPECT_TRUE(StepCrossFade(fade, 22, 16, 300));
	EXPECT_EQ(fade.oldClip, 11u);
	EXPECT_FLOAT_EQ(fade.oldTime, 480.0f);
	EXPECT_EQ(fade.leftMs, 300);
	EXPECT_FLOAT_EQ(fade.weight, 1.0f);
	EXPECT_EQ(fade.lastClip, 22u);
	// then it counts down by the frame's ms
	EXPECT_TRUE(StepCrossFade(fade, 22, 30, 300));
	EXPECT_EQ(fade.leftMs, 270);
	EXPECT_FLOAT_EQ(fade.weight, 0.9f);
	EXPECT_TRUE(StepCrossFade(fade, 22, 150, 300));
	EXPECT_FLOAT_EQ(fade.weight, 0.4f);
	// exactly 0: weight 0 and no blend; below 0: 0, the weight left as it was
	EXPECT_FALSE(StepCrossFade(fade, 22, 120, 300));
	EXPECT_EQ(fade.leftMs, 0);
	EXPECT_FLOAT_EQ(fade.weight, 0.0f);
	EXPECT_FALSE(StepCrossFade(fade, 22, 16, 300));
	EXPECT_EQ(fade.leftMs, 0);
	// a change in the middle of a fade restarts it from the clip drawn last (no chain of three)
	fade.lastTime = 90.0f;
	EXPECT_TRUE(StepCrossFade(fade, 33, 16, 300));
	EXPECT_TRUE(StepCrossFade(fade, 33, 100, 300));
	fade.lastTime = 12.0f;
	EXPECT_TRUE(StepCrossFade(fade, 44, 16, 300));
	EXPECT_EQ(fade.oldClip, 33u);
	EXPECT_FLOAT_EQ(fade.oldTime, 12.0f);
	EXPECT_EQ(fade.leftMs, 300);
}

TEST(SuperVillagerSmooth, YawFollow)
{
	const float rate = std::bit_cast<float>(0x407B53D2u); // 3.92699 rad/s
	const float step = (16.0f * 0.001f) * rate;
	// equal: no turn
	float yaw = 0.5f;
	EXPECT_EQ(StepYawFollow(yaw, 0.5f, 16.0f, rate, false), 0.0f);
	EXPECT_EQ(yaw, 0.5f);
	// behind: one step towards the target, the body's drawn copy turned by the lag (followYaw - target)
	yaw = 0.0f;
	const float turn = StepYawFollow(yaw, 1.0f, 16.0f, rate, false);
	EXPECT_FLOAT_EQ(yaw, step);
	EXPECT_FLOAT_EQ(turn, step - 1.0f);
	// ahead: down
	yaw = 1.0f;
	EXPECT_FLOAT_EQ(StepYawFollow(yaw, 0.0f, 16.0f, rate, false), 1.0f - step);
	EXPECT_FLOAT_EQ(yaw, 1.0f - step);
	// not more than a step away: it takes the target, no turn
	yaw = 1.0f - step * 0.5f;
	EXPECT_EQ(StepYawFollow(yaw, 1.0f, 16.0f, rate, false), 0.0f);
	EXPECT_EQ(yaw, 1.0f);
	// snap (bit 2): the target at once
	yaw = -2.0f;
	EXPECT_EQ(StepYawFollow(yaw, 2.0f, 16.0f, rate, true), 0.0f);
	EXPECT_EQ(yaw, 2.0f);
	// the short way round over +-pi: from 3.0 to -3.0 goes up, past pi
	yaw = 3.0f;
	StepYawFollow(yaw, -3.0f, 16.0f, rate, false);
	EXPECT_FLOAT_EQ(yaw, 3.0f + step);
	// the yaw is wrapped once into [-pi, pi] first
	yaw = 3.0f + glm::two_pi<float>();
	StepYawFollow(yaw, 3.0f, 16.0f, rate, false);
	EXPECT_NEAR(yaw, 3.0f, 1e-5f);
}
