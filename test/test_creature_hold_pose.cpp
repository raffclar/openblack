/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <glm/gtx/transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/SkeletalAnimation.h"
#include "Creature/CreatureObjectActions.h"
#include "ECS/Components/Transform.h"

using namespace openblack;
using creature_object_actions::Kind;
using creature_object_actions::ShowsHoldPose;
namespace sa = skeletal_animation;

namespace
{
/// A root with two children, side by side, in a rest pose with no turns
sa::Skeleton ThreeBones()
{
	return {.parents = {sa::k_NoParent, 0, 0},
	        .restRotations = {sa::k_Identity, sa::k_Identity, sa::k_Identity},
	        .inverseRestRotations = {sa::k_Identity, sa::k_Identity, sa::k_Identity}};
}

/// An animation that turns only the first child, the same in both of its frames
sa::Animation TurnsFirstChild(glm::vec3 euler)
{
	return {.duration = 100,
	        .looping = false,
	        .rotatedJoints = {1},
	        .translatedJoints = {},
	        .frames = {{.eulerAngles = {euler}, .translations = {}}, {.eulerAngles = {euler}, .translations = {}}}};
}

std::vector<sa::Pose> RestPoses()
{
	return std::vector<sa::Pose>(3, sa::Pose {.rotation = sa::k_Identity, .translation = glm::vec3(1.0f)});
}

void ExpectNear(const sa::Matrix& actual, const sa::Matrix& expected)
{
	for (size_t r = 0; r < 3; ++r)
	{
		for (size_t c = 0; c < 3; ++c)
		{
			EXPECT_NEAR(actual.at(r).at(c), expected.at(r).at(c), 1e-5f) << "row " << r << " column " << c;
		}
	}
}
} // namespace

TEST(CreatureHoldPose, ShownWhenTheBodyPlaysNoAction)
{
	EXPECT_TRUE(ShowsHoldPose(std::nullopt));
}

TEST(CreatureHoldPose, ReplacedByTheAnimationsThatMoveTheHeldThing)
{
	for (const auto kind :
	     {Kind::PickUp, Kind::PutDown, Kind::Discard, Kind::Eat, Kind::Keep, Kind::Throw, Kind::Catch, Kind::TakeFromHand})
	{
		EXPECT_FALSE(ShowsHoldPose(kind)) << creature_object_actions::Name(kind);
	}
}

TEST(CreatureHoldPose, KeptWhileKnockingDownKickingAndPointing)
{
	for (const auto kind : {Kind::Destroy, Kind::Kick, Kind::Point})
	{
		EXPECT_TRUE(ShowsHoldPose(kind)) << creature_object_actions::Name(kind);
	}
}

TEST(CreatureHoldPose, LayerSetsOnlyTheBonesItTurns)
{
	const auto skeleton = ThreeBones();
	auto poses = RestPoses();
	const glm::vec3 euler {0.1f, 0.2f, 0.3f};
	sa::SetLayer(poses, TurnsFirstChild(euler), 99, skeleton);
	ExpectNear(poses[1].rotation, sa::RotationYXZ(euler));
	ExpectNear(poses[0].rotation, sa::k_Identity);
	ExpectNear(poses[2].rotation, sa::k_Identity);
	EXPECT_EQ(poses[1].translation, glm::vec3(1.0f));
}

TEST(CreatureHoldPose, MirroredLayerTurnsTheMirrorBoneReflected)
{
	const auto skeleton = ThreeBones();
	auto poses = RestPoses();
	const std::array<uint32_t, 3> mirror {0, 2, 1};
	sa::SetLayer(poses, TurnsFirstChild({0.1f, 0.2f, 0.3f}), 99, skeleton, nullptr, mirror);
	// Reflected across the body: the turns about the up and forward axes change direction
	ExpectNear(poses[2].rotation, sa::RotationYXZ({0.1f, -0.2f, -0.3f}));
	ExpectNear(poses[1].rotation, sa::k_Identity);
	ExpectNear(poses[0].rotation, sa::k_Identity);
	EXPECT_EQ(poses[2].translation, glm::vec3(1.0f));
}

TEST(CreatureHoldPose, HeldThingIsDrawnAtItsPositionWhenTheTurnIsNotARotation)
{
	// A hand's turn after animations are blended: each axis has unit length, but the axes are not square to each other
	const glm::mat3 skewed(glm::normalize(glm::vec3(1.0f, 0.3f, 0.0f)), glm::normalize(glm::vec3(0.2f, 1.0f, 0.1f)),
	                       glm::vec3(0.0f, 0.0f, 1.0f));
	const glm::vec3 position {2554.0f, 31.0f, 2550.0f};
	const auto matrix = ecs::components::PlacedMatrix(position, skewed, glm::vec3(2.0f));
	EXPECT_EQ(glm::vec3(matrix[3]), position);
	EXPECT_EQ(glm::vec3(matrix[0]), skewed[0] * 2.0f);
	EXPECT_EQ(glm::vec3(matrix[1]), skewed[1] * 2.0f);
}

TEST(CreatureHoldPose, PlacedMatrixScalesThenTurnsThenMoves)
{
	const auto turn = glm::mat3(glm::rotate(0.7f, glm::vec3(0.0f, 1.0f, 0.0f)));
	const glm::vec3 position {10.0f, 2.0f, -4.0f};
	const glm::vec3 scale {1.5f, 1.5f, 1.5f};
	const auto expected = glm::translate(position) * glm::mat4(turn) * glm::scale(scale);
	const auto matrix = ecs::components::PlacedMatrix(position, turn, scale);
	for (glm::length_t c = 0; c < 4; ++c)
	{
		for (glm::length_t r = 0; r < 4; ++r)
		{
			EXPECT_NEAR(matrix[c][r], expected[c][r], 1e-5f);
		}
	}
}
