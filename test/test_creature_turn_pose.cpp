/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <bit>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureTurnPose.h"

using namespace openblack;
using namespace openblack::creature_turn_pose;

TEST(CreatureTurnPose, TheWeightIsThePartOfTheTurnNotMoreThanTwo)
{
	EXPECT_EQ(DrawWeight(0.0f), 0.0f);
	EXPECT_EQ(DrawWeight(0.37f), 0.37f);
	EXPECT_EQ(DrawWeight(1.0f), 1.0f);
	EXPECT_EQ(DrawWeight(2.0f), 2.0f);
	EXPECT_EQ(DrawWeight(5.0f), 2.0f);
}

TEST(CreatureTurnPose, TheDrawnBodyStartsAtTheOlderPoseAndEndsAtTheNewerPose)
{
	const std::array older {glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f))};
	const std::array newer {glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 2.0f, -1.0f))};
	std::array<glm::mat4, 1> drawn {};
	Blend(older, newer, 0.0f, drawn);
	EXPECT_EQ(drawn[0], older[0]);
	Blend(older, newer, 1.0f, drawn);
	EXPECT_EQ(drawn[0], newer[0]);
	Blend(older, newer, 0.25f, drawn);
	EXPECT_FLOAT_EQ(drawn[0][3].x, 2.0f);
	EXPECT_FLOAT_EQ(drawn[0][3].z, 2.0f);
}

TEST(CreatureTurnPose, TheLastRowOfABoneDoesNotBlend)
{
	const std::array older {glm::mat4(1.0f)};
	const std::array newer {glm::mat4(1.0f)};
	std::array<glm::mat4, 1> drawn {};
	// The weights do not add to exactly one here, but the last row stays (0, 0, 0, 1)
	Blend(older, newer, 0.3f, drawn);
	EXPECT_EQ(drawn[0][0][3], 0.0f);
	EXPECT_EQ(drawn[0][1][3], 0.0f);
	EXPECT_EQ(drawn[0][2][3], 0.0f);
	EXPECT_EQ(drawn[0][3][3], 1.0f);
}

TEST(CreatureTurnPose, TheNewerProductAndTheOlderProductEachRoundToAFloat)
{
	// Each product rounds to a float before the sum. One rounding of the exact sum gives other bits here.
	const float older = 2147.3477f;
	const float newer = 2138.913f;
	const float weight = 0.37f;
	const float olderPart = (1.0f - weight) * older;
	const float newerPart = weight * newer;
	EXPECT_EQ(std::bit_cast<uint32_t>(BlendNumber(older, newer, weight)), std::bit_cast<uint32_t>(olderPart + newerPart));
}

TEST(CreatureTurnPose, BonesInTheWorldGoBackToTheSpaceOfTheMesh)
{
	const auto placement = glm::scale(
	    glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 5.0f, -40.0f)), 0.7f, glm::vec3(0.0f, 1.0f, 0.0f)),
	    glm::vec3(2.0f));
	const std::vector bones {glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 3.0f, 1.0f)), glm::mat4(1.0f)};
	const auto world = ToWorld(bones, placement);
	ASSERT_EQ(world.size(), 2u);
	EXPECT_FLOAT_EQ(world[1][3].x, 100.0f);
	std::vector<glm::mat4> back(world.size());
	ToMesh(world, placement, back);
	for (size_t bone = 0; bone < bones.size(); ++bone)
	{
		for (glm::length_t column = 0; column < 4; ++column)
		{
			for (glm::length_t row = 0; row < 4; ++row)
			{
				EXPECT_NEAR(back[bone][column][row], bones[bone][column][row], 1e-4f);
			}
		}
	}
}
