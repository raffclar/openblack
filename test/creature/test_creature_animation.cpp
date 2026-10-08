/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "3D/SkeletalAnimation.h"
#include "Creature/CreatureAnimation.h"

using namespace openblack;
using skeletal_animation::Animation;

namespace
{
constexpr float k_Tolerance = 1e-4f;

/// An animation of one bone turned about y and moved along x, a keyframe for each angle and distance
Animation OneBone(const std::vector<float>& yaws, const std::vector<float>& xs, uint32_t duration = 1000)
{
	Animation animation {.duration = duration, .looping = true, .rotatedJoints = {0}, .translatedJoints = {0}, .frames = {}};
	for (size_t i = 0; i < yaws.size(); ++i)
	{
		animation.frames.push_back({.eulerAngles = {glm::vec3(0.0f, yaws[i], 0.0f)}, .translations = {glm::vec3(xs[i], 0, 0)}});
	}
	return animation;
}
} // namespace

TEST(SkeletalAnimation, EulerAnglesRoundTrip)
{
	const glm::vec3 euler {0.3f, -0.7f, 1.1f};
	const auto back = skeletal_animation::EulerYXZ(skeletal_animation::RotationYXZ(euler));
	EXPECT_NEAR(back.x, euler.x, k_Tolerance);
	EXPECT_NEAR(back.y, euler.y, k_Tolerance);
	EXPECT_NEAR(back.z, euler.z, k_Tolerance);
}

TEST(SkeletalAnimation, CyclesWrapFromTheLastFrameToTheFirst)
{
	const auto animation = OneBone({0.0f, 1.0f}, {0.0f, 1.0f}, 1000);
	const auto halfway = skeletal_animation::FindFrames(animation, 250);
	EXPECT_EQ(halfway.from, 0u);
	EXPECT_EQ(halfway.to, 1u);
	EXPECT_NEAR(halfway.t, 0.5f, k_Tolerance);
	const auto wrapping = skeletal_animation::FindFrames(animation, 750);
	EXPECT_EQ(wrapping.from, 1u);
	EXPECT_EQ(wrapping.to, 0u);
}

TEST(SkeletalAnimation, SampledTranslationsReplaceTheRest)
{
	const auto animation = OneBone({0.0f, 0.0f}, {2.0f, 4.0f});
	const std::array<uint32_t, 1> parents {skeletal_animation::k_NoParent};
	const std::array<glm::mat4, 1> rest {glm::mat4(1.0f)};
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(parents, rest);
	const auto poses = skeletal_animation::SampleCycle(animation, animation, 250, skeleton);
	ASSERT_EQ(poses.size(), 1u);
	EXPECT_NEAR(poses[0].translation.x, 3.0f, k_Tolerance);
	const auto matrices = skeletal_animation::ComposeBoneMatrices(poses, parents);
	EXPECT_NEAR(matrices[0][3].x, 3.0f, k_Tolerance);
}

TEST(CreatureAnimation, NoWeightKeepsTheBase)
{
	const auto base = OneBone({0.2f}, {1.0f});
	const auto evil = OneBone({0.8f}, {5.0f});
	const auto blended = creature_animation::Blend(base, {.animation = &evil, .stand = &evil, .weight = 0.0f},
	                                               {.animation = nullptr, .stand = nullptr, .weight = 0.0f});
	EXPECT_NEAR(blended.frames[0].eulerAngles[0].y, 0.2f, k_Tolerance);
	EXPECT_NEAR(blended.frames[0].translations[0].x, 1.0f, k_Tolerance);
}

TEST(CreatureAnimation, FullWeightTakesTheVariant)
{
	const auto base = OneBone({0.2f}, {1.0f});
	const auto evil = OneBone({0.8f}, {5.0f});
	const auto blended = creature_animation::Blend(base, {.animation = &evil, .stand = &evil, .weight = 1.0f},
	                                               {.animation = nullptr, .stand = nullptr, .weight = 0.0f});
	EXPECT_NEAR(blended.frames[0].eulerAngles[0].y, 0.8f, k_Tolerance);
	EXPECT_NEAR(blended.frames[0].translations[0].x, 5.0f, k_Tolerance);
}

TEST(CreatureAnimation, TranslationsBlendLinearlyOnBothAxes)
{
	const auto base = OneBone({0.0f}, {0.0f});
	const auto good = OneBone({0.0f}, {4.0f});
	const auto fat = OneBone({0.0f}, {-2.0f});
	const auto blended = creature_animation::Blend(base, {.animation = &good, .stand = &good, .weight = 0.5f},
	                                               {.animation = &fat, .stand = &fat, .weight = 0.5f});
	EXPECT_NEAR(blended.frames[0].translations[0].x, 1.0f, k_Tolerance);
}

TEST(CreatureAnimation, RotationsBlendOnTheMatrices)
{
	// Halfway between two turns about y on the matrices' elements is the halfway turn, only shorter
	const auto base = OneBone({0.0f}, {0.0f});
	const auto evil = OneBone({0.6f}, {0.0f});
	const auto blended = creature_animation::Blend(base, {.animation = &evil, .stand = &evil, .weight = 0.5f},
	                                               {.animation = nullptr, .stand = nullptr, .weight = 0.0f});
	EXPECT_NEAR(blended.frames[0].eulerAngles[0].y, 0.3f, k_Tolerance);
}

TEST(CreatureAnimation, BonesAVariantDoesNotMoveTakeItsStand)
{
	const auto base = OneBone({0.0f}, {0.0f});
	Animation variant {.duration = 1000, .looping = true, .rotatedJoints = {}, .translatedJoints = {}, .frames = {{}}};
	const auto stand = OneBone({0.0f}, {6.0f});
	const auto blended = creature_animation::Blend(base, {.animation = &variant, .stand = &stand, .weight = 1.0f},
	                                               {.animation = nullptr, .stand = nullptr, .weight = 0.0f});
	EXPECT_NEAR(blended.frames[0].translations[0].x, 6.0f, k_Tolerance);
}

TEST(CreatureAnimation, AVariantWithoutItsOwnMovesByItsStand)
{
	const auto animation = OneBone({0.5f}, {1.0f});
	const auto baseStand = OneBone({0.1f}, {2.0f});
	const auto variantStand = OneBone({0.4f}, {3.5f});
	const auto adjusted = creature_animation::AdjustFromStand(animation, baseStand, variantStand);
	// Turned on by the difference between the stands, moved by the difference between them
	EXPECT_NEAR(adjusted.frames[0].eulerAngles[0].y, 0.8f, k_Tolerance);
	EXPECT_NEAR(adjusted.frames[0].translations[0].x, 2.5f, k_Tolerance);
}

TEST(CreatureAnimation, TheRestPoseBlendsAsTheBody)
{
	const std::array<glm::mat4, 1> base {glm::mat4(1.0f)};
	auto moved = glm::mat4(1.0f);
	moved[3] = glm::vec4(4.0f, 0.0f, 0.0f, 1.0f);
	const std::array<glm::mat4, 1> evil {moved};
	const auto rest = creature_animation::BlendRest(base, evil, 0.25f, base, 1.0f);
	EXPECT_NEAR(rest[0][3].x, 1.0f, k_Tolerance);
	EXPECT_NEAR(rest[0][3].w, 1.0f, k_Tolerance);
}

TEST(CreatureAnimation, BreathingTakesFiveSecondsAtSizeOne)
{
	EXPECT_FLOAT_EQ(creature_animation::BreathPeriod(1.0f), 5.0f);
	EXPECT_FLOAT_EQ(creature_animation::BreathPeriod(4.0f), 10.0f);
	EXPECT_NEAR(creature_animation::AdvanceBreath(0.9f, 1.0f, 5.0f), 0.1f, k_Tolerance);
	EXPECT_EQ(creature_animation::BreathTime(0.5f, 2208), 1104u);
	EXPECT_EQ(creature_animation::BreathTime(1.0f, 2208), 2207u);
}

TEST(CreatureAnimation, TheBreathingPeriodEasesToItsTarget)
{
	constexpr float k_Turn = 0.1f;
	constexpr float k_Resting = 5.0f;
	// A creature not yet breathing starts at its target
	EXPECT_FLOAT_EQ(creature_animation::EaseBreathPeriod(0.0f, k_Resting, k_Resting, k_Turn), k_Resting);
	// Faster breathing is taken up over half a second: five turns, a fifth of the gap a turn
	EXPECT_FLOAT_EQ(creature_animation::EaseBreathPeriod(k_Resting, 2.2f, k_Resting, k_Turn),
	                k_Resting - ((k_Resting - 2.2f) / 5.0f));
	// Back to resting over ten seconds: a hundred turns, a hundredth of the gap a turn
	EXPECT_FLOAT_EQ(creature_animation::EaseBreathPeriod(2.0f, k_Resting, k_Resting, k_Turn),
	                2.0f + ((k_Resting - 2.0f) / 100.0f));
	// Turns longer than the time constant close the whole gap
	EXPECT_FLOAT_EQ(creature_animation::EaseBreathPeriod(k_Resting, 3.0f, k_Resting, 1.0f), 3.0f);
}

TEST(CreatureAnimation, BreathingCalmsSlowlyAndQuickensFast)
{
	constexpr float k_Turn = 0.1f;
	constexpr float k_Resting = 5.0f;
	float quickening = k_Resting;
	float calming = 1.4f;
	for (int turn = 0; turn < 10; ++turn)
	{
		quickening = creature_animation::EaseBreathPeriod(quickening, 1.4f, k_Resting, k_Turn);
		calming = creature_animation::EaseBreathPeriod(calming, k_Resting, k_Resting, k_Turn);
	}
	// A second on, quickening has almost arrived; calming has barely started
	EXPECT_LT(quickening - 1.4f, 0.5f);
	EXPECT_LT(calming - 1.4f, 0.5f);
	EXPECT_GT(calming, 1.4f);
}

namespace
{
using Matrix = skeletal_animation::Matrix;

/// The hand's animator poses the same kind of boned mesh with the same maths (src/3D/HandAnimator.cpp): the stored
/// angles become a YXZ rotation by rows, two keyframes are blended on the matrices' elements and each row made unit
/// length again, a bone's turn is its rest rotation before the keyframe and its parent's inverse rest rotation after,
/// and the bones are composed into column-vector matrices. These helpers are the hand's, so that the creature's
/// skeletal animation cannot drift away from the hand's without a test failing.
Matrix HandRotationRows(const glm::vec3& euler)
{
	const auto m = affine::RotationYXZ(euler.y, euler.x, euler.z);
	Matrix rows {};
	for (size_t r = 0; r < 3; ++r)
	{
		for (size_t c = 0; c < 3; ++c)
		{
			rows.at(r).at(c) = m[static_cast<glm::length_t>(r)][static_cast<glm::length_t>(c)];
		}
	}
	return rows;
}

Matrix HandMul3(const Matrix& a, const Matrix& b)
{
	Matrix o {};
	for (size_t i = 0; i < 3; ++i)
	{
		for (size_t j = 0; j < 3; ++j)
		{
			o.at(i).at(j) = (a.at(i)[0] * b.at(0).at(j)) + (a.at(i)[1] * b.at(1).at(j)) + (a.at(i)[2] * b.at(2).at(j));
		}
	}
	return o;
}

Matrix HandNormalizeRows(Matrix r)
{
	for (auto& row : r)
	{
		const auto l = std::hypot(row[0], row[1], row[2]);
		if (l > 0.0f)
		{
			for (auto& value : row)
			{
				value /= l;
			}
		}
	}
	return r;
}

Matrix HandLerpRotation(const Matrix& a, const Matrix& b, float t)
{
	Matrix o {};
	for (size_t r = 0; r < 3; ++r)
	{
		for (size_t c = 0; c < 3; ++c)
		{
			o.at(r).at(c) = a.at(r).at(c) + ((b.at(r).at(c) - a.at(r).at(c)) * t);
		}
	}
	return HandNormalizeRows(o);
}

glm::mat4 HandToColumnMatrix(const Matrix& r, const glm::vec3& t)
{
	return {r[0][0], r[0][1], r[0][2], 0.0f, r[1][0], r[1][1], r[1][2], 0.0f,
	        r[2][0], r[2][1], r[2][2], 0.0f, t.x,     t.y,     t.z,     1.0f};
}
} // namespace

TEST(SkeletalAnimation, PosesABoneAsTheHandsAnimatorDoes)
{
	// A root and a child, their rest poses unturned so that the inverse of a rest rotation is its transpose exactly.
	// The animation moves every bone, because the two animators fall back differently on the bones theirs does not
	// move: the hand takes the bind pose, the creature the first frame of the stand.
	const std::array<glm::mat4, 2> rest {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.0f, 0.0f))};
	const std::array<uint32_t, 2> parents {skeletal_animation::k_NoParent, 0};
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(parents, rest);

	// A duration and a time that put the sample exactly halfway between the keyframes in both animators
	constexpr uint32_t k_Duration = 1024;
	constexpr uint32_t k_TimeMs = 256;
	const std::array<glm::vec3, 2> firstAngles {glm::vec3(0.1f, -0.2f, 0.3f), glm::vec3(-0.4f, 0.5f, 0.15f)};
	const std::array<glm::vec3, 2> secondAngles {glm::vec3(0.6f, 0.7f, -0.8f), glm::vec3(0.25f, -0.35f, 0.45f)};
	const std::array<glm::vec3, 2> firstMoves {glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.0f, 4.0f, 0.0f)};
	const std::array<glm::vec3, 2> secondMoves {glm::vec3(5.0f, -2.0f, 1.0f), glm::vec3(0.5f, 4.5f, -0.5f)};
	Animation animation {
	    .duration = k_Duration,
	    .looping = true,
	    .rotatedJoints = {0, 1},
	    .translatedJoints = {0, 1},
	    .frames = {{.eulerAngles = {firstAngles[0], firstAngles[1]}, .translations = {firstMoves[0], firstMoves[1]}},
	               {.eulerAngles = {secondAngles[0], secondAngles[1]}, .translations = {secondMoves[0], secondMoves[1]}}}};

	// The hand's pipeline, bone by bone
	std::array<glm::mat4, 2> expected {};
	for (size_t bone = 0; bone < 2; ++bone)
	{
		const auto keyframe = HandLerpRotation(HandRotationRows(firstAngles[bone]), HandRotationRows(secondAngles[bone]), 0.5f);
		auto turn = HandMul3(skeleton.restRotations[bone], keyframe);
		if (bone != 0)
		{
			turn = HandMul3(turn, skeletal_animation::Transpose(skeleton.restRotations[parents[bone]]));
		}
		const auto move = firstMoves[bone] + ((secondMoves[bone] - firstMoves[bone]) * 0.5f);
		const auto local = HandToColumnMatrix(turn, move);
		expected.at(bone) = bone == 0 ? local : expected.at(parents[bone]) * local;
	}

	const auto poses = skeletal_animation::SampleCycle(animation, animation, k_TimeMs, skeleton);
	const auto matrices = skeletal_animation::ComposeBoneMatrices(poses, parents);
	ASSERT_EQ(matrices.size(), 2u);
	for (size_t bone = 0; bone < 2; ++bone)
	{
		for (glm::length_t c = 0; c < 4; ++c)
		{
			for (glm::length_t r = 0; r < 4; ++r)
			{
				EXPECT_FLOAT_EQ(matrices.at(bone)[c][r], expected.at(bone)[c][r])
				    << "bone " << bone << " column " << c << " row " << r;
			}
		}
	}
}
