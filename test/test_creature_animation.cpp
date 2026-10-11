/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <gtest/gtest.h>

#include "3D/SkeletalAnimation.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureRig.h"

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
constexpr float k_QuarterTurn = 1.5707964f;

/// Lengths of a 3 by 3 matrix's rows, the largest dot product between two of them and its determinant
struct Shape
{
	std::array<float, 3> lengths;
	float skew;
	float determinant;
};

Shape ShapeOf(const skeletal_animation::Matrix& m)
{
	Shape shape {};
	for (size_t r = 0; r < 3; ++r)
	{
		shape.lengths.at(r) = std::sqrt((m.at(r)[0] * m.at(r)[0]) + (m.at(r)[1] * m.at(r)[1]) + (m.at(r)[2] * m.at(r)[2]));
	}
	const auto dot = [&m](size_t a, size_t b) {
		return (m.at(a)[0] * m.at(b)[0]) + (m.at(a)[1] * m.at(b)[1]) + (m.at(a)[2] * m.at(b)[2]);
	};
	shape.skew = std::max({std::abs(dot(0, 1)), std::abs(dot(0, 2)), std::abs(dot(1, 2))});
	shape.determinant = (m[0][0] * ((m[1][1] * m[2][2]) - (m[1][2] * m[2][1]))) -
	                    (m[0][1] * ((m[1][0] * m[2][2]) - (m[1][2] * m[2][0]))) +
	                    (m[0][2] * ((m[1][0] * m[2][1]) - (m[1][1] * m[2][0])));
	return shape;
}

void ExpectTrueRotation(const skeletal_animation::Matrix& m)
{
	const auto shape = ShapeOf(m);
	for (const auto length : shape.lengths)
	{
		EXPECT_NEAR(length, 1.0f, k_Tolerance);
	}
	EXPECT_NEAR(shape.skew, 0.0f, k_Tolerance);
	EXPECT_NEAR(shape.determinant, 1.0f, k_Tolerance);
}

/// A spin on the spot: the root turning about y from nothing to its full angle over its keyframes
Animation Spin(float angle, uint32_t duration, size_t frames = 5)
{
	Animation spin {.duration = duration, .looping = false, .rotatedJoints = {0}, .translatedJoints = {}, .frames = {}};
	for (size_t i = 0; i < frames; ++i)
	{
		const auto share = static_cast<float>(i) / static_cast<float>(frames - 1);
		spin.frames.push_back({.eulerAngles = {glm::vec3(0.1f * share, angle * share, 0.0f)}, .translations = {}});
	}
	return spin;
}
} // namespace

TEST(SkeletalAnimation, AnglesOfABlendedRotationAreTheGamesInTurn)
{
	// Half way between a quarter turn about y and none, blended on the elements: y comes first from the third row,
	// then x and z from what is left once it is turned back
	const auto blended = skeletal_animation::Lerp(skeletal_animation::k_Identity,
	                                              skeletal_animation::RotationYXZ(glm::vec3(0.0f, k_QuarterTurn, 0.0f)), 0.5f);
	const auto angles = skeletal_animation::EulerYXZ(blended);
	EXPECT_NEAR(angles.y, k_QuarterTurn * 0.5f, k_Tolerance);
	EXPECT_NEAR(angles.x, 0.0f, k_Tolerance);
	EXPECT_NEAR(angles.z, 0.0f, k_Tolerance);
}

TEST(CreatureAnimation, ATurnsPairPlaysAsTrueRotations)
{
	// A turn between the quarter and the half spin, the angle a share of the way between them
	const auto quarter = Spin(k_QuarterTurn, 1000);
	const auto half = Spin(2.0f * k_QuarterTurn, 1400);
	const auto stand = OneBone({0.0f}, {0.0f});
	for (const auto weight : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
	{
		const auto pair = creature_animation::BlendPair(quarter, half, stand, weight);
		ASSERT_EQ(pair.frames.size(), quarter.frames.size());
		for (const auto& frame : pair.frames)
		{
			ExpectTrueRotation(skeletal_animation::RotationYXZ(frame.eulerAngles[0]));
		}
		// It ends turned between the two, as the blended matrix's third row points
		const auto a = k_QuarterTurn;
		const auto b = 2.0f * k_QuarterTurn;
		const auto expected = std::atan2(((1.0f - weight) * std::sin(a)) + (weight * std::sin(b)),
		                                 ((1.0f - weight) * std::cos(a)) + (weight * std::cos(b)));
		EXPECT_NEAR(pair.frames.back().eulerAngles[0].y, expected, 1e-3f);
	}
}

TEST(CreatureAnimation, PlayingAPairKeepsTheBodysSize)
{
	// The old way, the two playing together by weight, shrank the body across the turn: a half and a quarter spin
	// summed half and half leave the root's rows 0.71 long at the end. The blended pair keeps them whole.
	const auto quarter = Spin(k_QuarterTurn, 1000);
	const auto half = Spin(2.0f * k_QuarterTurn, 1000);
	const auto stand = OneBone({0.0f}, {0.0f});
	const std::array<uint32_t, 1> parents {skeletal_animation::k_NoParent};
	const std::array<glm::mat4, 1> rest {glm::mat4(1.0f)};
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(parents, rest);

	const std::vector<std::vector<skeletal_animation::Pose>> both {
	    skeletal_animation::SampleCycle(quarter, stand, 999, skeleton),
	    skeletal_animation::SampleCycle(half, stand, 999, skeleton)};
	const std::array<float, 2> halves {0.5f, 0.5f};
	const auto summed = skeletal_animation::WeightedSum(both, halves);
	EXPECT_LT(ShapeOf(summed[0].rotation).lengths[0], 0.8f);

	// On its keyframes the pair is a true rotation; between them the keyframes blend on their elements with each row
	// made whole again, as the game plays every animation, which keeps the size and leaves only a slight skew
	const auto pair =
	    creature_animation::BlendPair(Spin(k_QuarterTurn, 1000, 31), Spin(2.0f * k_QuarterTurn, 1000, 31), stand, 0.5f);
	for (uint32_t time = 0; time < pair.duration; time += 10)
	{
		const auto shape = ShapeOf(skeletal_animation::SampleCycle(pair, stand, time, skeleton)[0].rotation);
		for (const auto length : shape.lengths)
		{
			EXPECT_NEAR(length, 1.0f, k_Tolerance);
		}
		EXPECT_LT(shape.skew, 1e-3f);
		EXPECT_GT(shape.determinant, 0.999f);
	}
}

TEST(CreatureAnimation, APairTakesTheStandForBonesOnlyOneMoves)
{
	auto quarter = Spin(k_QuarterTurn, 1000, 2);
	auto other = Spin(k_QuarterTurn, 1000, 2);
	// The second moves bone 1 as well, along x; the first leaves it to the stand, which holds it at 4
	other.translatedJoints = {1};
	for (auto& frame : other.frames)
	{
		frame.translations = {glm::vec3(8.0f, 0.0f, 0.0f)};
	}
	Animation stand {.duration = 1000, .looping = true, .rotatedJoints = {}, .translatedJoints = {0, 1}, .frames = {}};
	stand.frames.push_back({.eulerAngles = {}, .translations = {glm::vec3(0.0f), glm::vec3(4.0f, 0.0f, 0.0f)}});
	const auto pair = creature_animation::BlendPair(quarter, other, stand, 0.25f);
	ASSERT_EQ(pair.translatedJoints, std::vector<uint32_t> {1});
	EXPECT_NEAR(pair.frames[0].translations[0].x, 5.0f, k_Tolerance);
}

TEST(CreatureAnimation, APairLastsBetweenItsTwoToTheMillisecond)
{
	const auto stand = OneBone({0.0f}, {0.0f});
	const auto pair = creature_animation::BlendPair(Spin(1.0f, 1000), Spin(2.0f, 1167), stand, 0.5f);
	EXPECT_EQ(pair.duration, 1084u);
	EXPECT_FALSE(pair.looping);
}

TEST(CreatureAnimation, APairMovesAsFarAsBetweenItsTwoMoves)
{
	// A step back and one to the side, half way: the way between them, as long as either rather than the shorter
	// chord between them
	const auto move = creature_animation::BlendMoves(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.5f);
	EXPECT_NEAR(glm::length(move), 10.0f, k_Tolerance);
	EXPECT_NEAR(move.x, move.z, k_Tolerance);
	EXPECT_EQ(creature_animation::BlendMoves(glm::vec3(0.0f), glm::vec3(0.0f), 0.5f), glm::vec3(0.0f));
}

TEST(SkeletalAnimation, AnimationsPlayingTogetherSumInTheMeshsSpace)
{
	// A root and a child, the root turned differently in the two: summed in the mesh's space the child sits where the
	// two put it on average, rather than where the averaged root would carry the averaged child
	const std::array<uint32_t, 2> parents {skeletal_animation::k_NoParent, 0};
	const auto pose = [](float yaw) {
		return std::vector<skeletal_animation::Pose> {
		    {.rotation = skeletal_animation::RotationYXZ(glm::vec3(0.0f, yaw, 0.0f)), .translation = glm::vec3(0.0f)},
		    {.rotation = skeletal_animation::k_Identity, .translation = glm::vec3(0.0f, 0.0f, 2.0f)}};
	};
	const std::vector<std::vector<skeletal_animation::Pose>> both {pose(0.0f), pose(k_QuarterTurn)};
	const std::array<float, 2> weights {0.25f, 0.75f};
	const auto summed = skeletal_animation::WeightedSumInMeshSpace(both, weights, parents);
	const auto placed = skeletal_animation::ComposeBoneMatrices(summed, parents);
	const auto a = skeletal_animation::ComposeBoneMatrices(both[0], parents);
	const auto b = skeletal_animation::ComposeBoneMatrices(both[1], parents);
	for (glm::length_t c = 0; c < 4; ++c)
	{
		for (glm::length_t r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(placed[1][c][r], (0.25f * a[1][c][r]) + (0.75f * b[1][c][r]), k_Tolerance);
		}
	}
}

TEST(CreatureAnimation, TheDrawnBodyKeepsItsSizeTurningRound)
{
	// Turned through half a turn a step at a time, as it is drawn between game turns, the body's placing turns it
	// without squashing: each axis stays as long as the creature's scale, square to the others
	constexpr float k_Scale = 0.19f;
	for (int step = 0; step <= 18; ++step)
	{
		const auto heading = static_cast<float>(step) * (2.0f * k_QuarterTurn / 18.0f);
		const auto model =
		    creature::PlacementMatrix(glm::vec3(10.0f, 2.0f, -3.0f), glm::mat3(glm::eulerAngleY(heading)), glm::vec3(k_Scale));
		const auto axes = glm::mat3(model);
		for (glm::length_t c = 0; c < 3; ++c)
		{
			EXPECT_NEAR(glm::length(axes[c]), k_Scale, k_Tolerance);
		}
		EXPECT_NEAR(glm::dot(axes[0], axes[2]), 0.0f, k_Tolerance);
		EXPECT_NEAR(glm::determinant(axes), k_Scale * k_Scale * k_Scale, k_Tolerance);
		EXPECT_NEAR(model[3].x, 10.0f, k_Tolerance);
	}
}
