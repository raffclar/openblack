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
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureProportions.h"

using namespace openblack;
using namespace openblack::creature_proportions;

namespace
{
/// A skeleton of eight bones: each odd bone is the mirror of the even bone before it, and bone 7 is its own mirror
constexpr std::array<uint32_t, 8> k_Mirror {1, 0, 3, 2, 5, 4, 6, 7};

skeletal_animation::Pose Identity(glm::vec3 translation)
{
	return {.rotation = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}, .translation = translation};
}
} // namespace

TEST(CreatureProportions, TheHeadIsLargestForTheSmallestCreature)
{
	EXPECT_FLOAT_EQ(Factor(k_Head, 0.0f), 1.8f);
	EXPECT_FLOAT_EQ(Factor(k_Head, 0.5f), 1.55f);
	EXPECT_FLOAT_EQ(Factor(k_Head, 1.0f), 1.3f);
	EXPECT_FLOAT_EQ(Factor(k_Head, 2.0f), 0.8f);
}

TEST(CreatureProportions, HandsFeetAndEyesAreNormalAtSizeTwo)
{
	EXPECT_FLOAT_EQ(Factor(k_HandsAndFeet, 0.0f), 1.5f);
	EXPECT_FLOAT_EQ(Factor(k_HandsAndFeet, 1.0f), 1.25f);
	EXPECT_FLOAT_EQ(Factor(k_HandsAndFeet, 2.0f), 1.0f);
	EXPECT_FLOAT_EQ(Factor(k_Eyes, 0.0f), 1.4f);
	EXPECT_FLOAT_EQ(Factor(k_Eyes, 1.0f), 1.2f);
	EXPECT_FLOAT_EQ(Factor(k_Eyes, 2.0f), 1.0f);
}

TEST(CreatureProportions, SizesOutsideZeroToTwoChangeNothingMore)
{
	EXPECT_FLOAT_EQ(Factor(k_Head, 4.0f), Factor(k_Head, 2.0f));
	EXPECT_FLOAT_EQ(Factor(k_HandsAndFeet, 3.0f), 1.0f);
	EXPECT_FLOAT_EQ(Factor(k_Eyes, -1.0f), 1.4f);
}

TEST(CreatureProportions, EachRightBoneAndThenItsMirrorAreScaled)
{
	const Bones bones {.head = 6u, .rightHand = 0u, .rightFoot = 2u, .rightEye = 4u};
	const auto scales = ScaledBones(bones, k_Mirror, 1.0f);
	ASSERT_EQ(scales.size(), 7u);
	const std::array<uint32_t, 7> expectedBones {6, 0, 1, 2, 3, 4, 5};
	const std::array<float, 7> expectedFactors {1.3f, 1.25f, 1.25f, 1.25f, 1.25f, 1.2f, 1.2f};
	for (size_t i = 0; i < scales.size(); ++i)
	{
		EXPECT_EQ(scales[i].bone, expectedBones.at(i));
		EXPECT_FLOAT_EQ(scales[i].factor, expectedFactors.at(i));
	}
}

TEST(CreatureProportions, MissingBonesAndMirrorsAreLeftOut)
{
	const Bones bones {.head = std::nullopt, .rightHand = 0u, .rightFoot = std::nullopt, .rightEye = 20u};
	const auto scales = ScaledBones(bones, k_Mirror, 0.0f);
	ASSERT_EQ(scales.size(), 3u);
	EXPECT_EQ(scales[0].bone, 0u);
	EXPECT_EQ(scales[1].bone, 1u);
	// The eye bone is not in the mirror table, so only the bone itself is in the list
	EXPECT_EQ(scales[2].bone, 20u);
}

TEST(CreatureProportions, ABoneThatIsItsOwnMirrorGetsTheFactorTwoTimes)
{
	auto poses = std::vector(8, Identity({0.0f, 0.0f, 0.0f}));
	const Bones bones {.rightEye = 7u};
	Apply(poses, ScaledBones(bones, k_Mirror, 0.0f));
	EXPECT_FLOAT_EQ(poses[7].rotation[0][0], 1.4f * 1.4f);
}

TEST(CreatureProportions, ApplyScalesTheRotationButNotThePosition)
{
	auto poses = std::vector(3, Identity({1.0f, 2.0f, 3.0f}));
	const std::array scales {BoneScale {.bone = 1, .factor = 1.5f}, BoneScale {.bone = 9, .factor = 2.0f}};
	Apply(poses, scales);
	for (size_t r = 0; r < 3; ++r)
	{
		for (size_t c = 0; c < 3; ++c)
		{
			EXPECT_FLOAT_EQ(poses[1].rotation.at(r).at(c), r == c ? 1.5f : 0.0f);
			EXPECT_FLOAT_EQ(poses[0].rotation.at(r).at(c), r == c ? 1.0f : 0.0f);
		}
	}
	EXPECT_FLOAT_EQ(poses[1].translation.x, 1.0f);
	EXPECT_FLOAT_EQ(poses[1].translation.z, 3.0f);
}
