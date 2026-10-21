/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <random>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Common/X87.h"
#include "Creature/CreatureRig.h"
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

TEST(CreatureTurnPose, GameRulesReadTheNewestTurnPoseInTheWorld)
{
	const auto placement = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, -4.0f));
	const std::vector drawn {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))};
	const std::vector newer {glm::translate(glm::mat4(1.0f), glm::vec3(11.0f, 0.0f, -4.0f)),
	                         glm::translate(glm::mat4(1.0f), glm::vec3(11.0f, 2.0f, -4.0f))};
	const auto pose = creature::TurnPose(newer, drawn, placement);
	EXPECT_EQ(pose.bones.data(), newer.data());
	EXPECT_EQ(pose.placement, glm::mat4(1.0f));
	EXPECT_EQ(glm::vec3(creature::PosedBone(1, pose.bones, pose.placement)[3]), glm::vec3(11.0f, 2.0f, -4.0f));
}

TEST(CreatureTurnPose, WithoutATurnPoseForEachBoneGameRulesReadTheDrawnPose)
{
	const auto placement = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, -4.0f));
	const std::vector drawn {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))};
	// Before the first turn ends there is no turn pose
	const auto first = creature::TurnPose({}, drawn, placement);
	EXPECT_EQ(first.bones.data(), drawn.data());
	EXPECT_EQ(first.placement, placement);
	// A turn pose for another skeleton is not read
	const std::vector other {glm::mat4(1.0f)};
	const auto rebuilt = creature::TurnPose(other, drawn, placement);
	EXPECT_EQ(rebuilt.bones.data(), drawn.data());
	EXPECT_EQ(glm::vec3(creature::PosedBone(1, rebuilt.bones, rebuilt.placement)[3]), glm::vec3(10.0f, 1.0f, -4.0f));
}

TEST(CreatureTurnPose, TheBlendGivesTheBitsOfTheWideMaths)
{
	// The wide maths rounds each product and the sum to a float one time
	const auto wide = [](float older, float newer, float weight) {
		const auto newerPart = x87::Mul(weight, newer);
		return x87::Add(x87::Mul(x87::Sub(1.0f, weight), older), newerPart);
	};
	std::mt19937 random(77);
	std::uniform_real_distribution<float> number(-500.0f, 500.0f);
	std::uniform_real_distribution<float> part(0.0f, 1.0f);
	for (int i = 0; i < 200000; ++i)
	{
		const float older = number(random);
		const float newer = i % 5 == 0 ? older : number(random);
		const float weight = i % 7 == 0 ? 0.0f : (i % 11 == 0 ? 1.0f : part(random));
		ASSERT_EQ(std::bit_cast<uint32_t>(creature_turn_pose::BlendNumber(older, newer, weight)),
		          std::bit_cast<uint32_t>(wide(older, newer, weight)))
		    << older << " " << newer << " " << weight;
	}
}
