/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <numeric>
#include <vector>

#include <L3DFile.h>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/TempleExteriorMorph.h"

using namespace openblack;
using namespace openblack::TempleExteriorMorph;

namespace
{
float WeightOf(const std::array<Corner, 4>& corners, uint32_t size, uint32_t stage)
{
	float weight = 0.0f;
	for (const auto& corner : corners)
	{
		if (corner.size == size && corner.stage == stage)
		{
			weight += corner.weight;
		}
	}
	return weight;
}

/// Fifteen fake temple meshes of two vertices each: every value of a vertex tells its mesh and its place
std::array<std::vector<l3d::L3DVertex>, k_Sizes * k_Stages> FakeTemples()
{
	std::array<std::vector<l3d::L3DVertex>, k_Sizes * k_Stages> meshes;
	for (uint32_t size = 0; size < k_Sizes; ++size)
	{
		for (uint32_t stage = 0; stage < k_Stages; ++stage)
		{
			auto& mesh = meshes.at((size * k_Stages) + stage);
			for (uint32_t i = 0; i < 2; ++i)
			{
				const auto s = static_cast<float>(size);
				const auto t = static_cast<float>(stage);
				const auto k = static_cast<float>(i);
				mesh.push_back({
				    .position = {s, t, k},
				    .texCoord = {t, s},
				    .normal = {k, s, t},
				});
			}
		}
	}
	return meshes;
}
} // namespace

TEST(TempleExteriorMorph, AlignmentTargetIsThePlayersFromZeroToJustShortOfOne)
{
	EXPECT_FLOAT_EQ(AlignmentTarget(0.0f), 0.5f);
	EXPECT_FLOAT_EQ(AlignmentTarget(-1.0f), 0.0f);
	EXPECT_FLOAT_EQ(AlignmentTarget(0.5f), 0.75f);
	// From 1 up it stops just short of 1, below -1 at 0
	EXPECT_EQ(AlignmentTarget(1.0f), 0.9999f);
	EXPECT_EQ(AlignmentTarget(1.5f), 0.9999f);
	EXPECT_EQ(AlignmentTarget(-1.5f), 0.0f);
}

TEST(TempleExteriorMorph, SizeTargetIsTwiceTheShareOfInfluence)
{
	// The first land always gives the small share, whatever the powers
	EXPECT_FLOAT_EQ(SizeTarget(true, 0.0f, 0.0f), 0.02f);
	EXPECT_FLOAT_EQ(SizeTarget(true, 300.0f, 400.0f), 0.02f);
	// Elsewhere the player's power over everyone's, and the small share when nobody has any
	EXPECT_FLOAT_EQ(SizeTarget(false, 30.0f, 100.0f), 0.6f);
	EXPECT_FLOAT_EQ(SizeTarget(false, 0.0f, 100.0f), 0.0f);
	EXPECT_FLOAT_EQ(SizeTarget(false, 0.0f, 0.0f), 0.02f);
	// More than half of the influence is the largest temple
	EXPECT_EQ(SizeTarget(false, 80.0f, 100.0f), 1.0f);
}

TEST(TempleExteriorMorph, ANewTempleIsNeutralSmallAndBlendedOnceAtOnce)
{
	State state;
	EXPECT_EQ(state.alignment, 0.5f);
	EXPECT_EQ(state.alignmentTarget, 0.5f);
	EXPECT_EQ(state.size, 0.0f);
	EXPECT_EQ(state.sizeTarget, 0.0f);
	// Its first blend is due though it has not moved; after it, none until it does
	EXPECT_TRUE(NeedsBlend(state));
	Blended(state);
	EXPECT_FALSE(NeedsBlend(state));
	EXPECT_FALSE(Turn(state, 0.5f, 0.0f));
	EXPECT_FALSE(NeedsBlend(state));
}

TEST(TempleExteriorMorph, ATurnStepsTowardTheTargetsAndBlendsPastThreeHundredths)
{
	State state;
	Blended(state);
	// 0.5 -> 0.516: not yet far enough from the 0.5 blended
	EXPECT_FALSE(Turn(state, 0.9999f, 0.02f));
	EXPECT_FLOAT_EQ(state.alignment, 0.516f);
	EXPECT_FLOAT_EQ(state.size, 0.016f);
	EXPECT_EQ(state.alignmentTarget, 0.9999f);
	EXPECT_FLOAT_EQ(state.sizeTarget, 0.02f);
	EXPECT_FALSE(NeedsBlend(state));
	// 0.532 is: the turn says so, and the blend keeps that look
	EXPECT_TRUE(Turn(state, 0.9999f, 0.02f));
	EXPECT_FLOAT_EQ(state.alignment, 0.532f);
	EXPECT_EQ(state.size, 0.02f); // the rest of the way, less than a step
	EXPECT_TRUE(NeedsBlend(state));
	Blended(state);
	EXPECT_EQ(state.blendedAlignment, state.alignment);
	EXPECT_EQ(state.blendedSize, state.size);
	EXPECT_FALSE(NeedsBlend(state));
	// The size alone, 0.02 on the first land, never moves far enough to blend
	State small;
	Blended(small);
	for (int turn = 0; turn < 10; ++turn)
	{
		EXPECT_FALSE(Turn(small, 0.5f, 0.02f));
	}
	EXPECT_EQ(small.size, 0.02f);
}

TEST(TempleExteriorMorph, ATurnBlendsWhenTheSizeMovesFarEnough)
{
	State state;
	Blended(state);
	EXPECT_FALSE(Turn(state, 0.5f, 1.0f)); // 0.016
	EXPECT_TRUE(Turn(state, 0.5f, 1.0f));  // 0.032
	Blended(state);
	// Back down: the same steps the other way
	EXPECT_FALSE(Turn(state, 0.5f, 0.0f));
	EXPECT_TRUE(Turn(state, 0.5f, 0.0f));
	EXPECT_EQ(state.size, 0.0f);
}

TEST(TempleExteriorMorph, VerticesAreTheCornersMeshesByTheirWeights)
{
	const auto meshes = FakeTemples();
	const VerticesOf verticesOf = [&meshes](uint32_t size, uint32_t stage) { return &meshes.at((size * k_Stages) + stage); };
	std::vector<l3d::L3DVertex> blended(2);
	// Half of sizes 0 and 1 and half of stages 2 and 3: every value the mean of the four
	ASSERT_TRUE(BlendVertices(Corners(0.25f, 0.625f), verticesOf, blended));
	EXPECT_FLOAT_EQ(blended[1].position.x, 0.5f);
	EXPECT_FLOAT_EQ(blended[1].position.y, 2.5f);
	EXPECT_FLOAT_EQ(blended[1].position.z, 1.0f);
	EXPECT_FLOAT_EQ(blended[1].texCoord.x, 2.5f);
	EXPECT_FLOAT_EQ(blended[1].texCoord.y, 0.5f);
	EXPECT_FLOAT_EQ(blended[1].normal.x, 1.0f);
	EXPECT_FLOAT_EQ(blended[1].normal.y, 0.5f);
	EXPECT_FLOAT_EQ(blended[1].normal.z, 2.5f);
	// The start, small and neutral, is the middle stage's mesh alone
	ASSERT_TRUE(BlendVertices(Corners(0.0f, 0.5f), verticesOf, blended));
	EXPECT_FLOAT_EQ(blended[0].position.x, 0.0f);
	EXPECT_FLOAT_EQ(blended[0].position.y, 2.0f);
	EXPECT_FLOAT_EQ(blended[0].position.z, 0.0f);
}

TEST(TempleExteriorMorph, AMissingOrMisshapenMeshOfAnyWeightFailsTheBlend)
{
	const auto meshes = FakeTemples();
	std::vector<l3d::L3DVertex> blended(2);
	// A mesh of no weight may be missing: Corners(0, 0.5) uses stage 2 of size 0 only
	const VerticesOf onlyTheMiddle = [&meshes](uint32_t size, uint32_t stage) -> const std::vector<l3d::L3DVertex>* {
		return size == 0 && stage == 2 ? &meshes.at(2) : nullptr;
	};
	EXPECT_TRUE(BlendVertices(Corners(0.0f, 0.5f), onlyTheMiddle, blended));
	EXPECT_FALSE(BlendVertices(Corners(0.25f, 0.625f), onlyTheMiddle, blended));
	const std::vector<l3d::L3DVertex> three(3);
	const VerticesOf misshapen = [&three](uint32_t, uint32_t) { return &three; };
	EXPECT_FALSE(BlendVertices(Corners(0.0f, 0.5f), misshapen, blended));
}

TEST(TempleExteriorMorph, BlendedVerticesAreLaidOnTheLand)
{
	// A land rising 0.5 a metre along x, and a temple standing at a height of 4, turned a quarter about y
	const land_morph::Ground ground = [](glm::vec2 xz) { return 0.5f * xz.x; };
	const auto object = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 4.0f, 200.0f)), glm::radians(90.0f),
	                                glm::vec3(0.0f, 1.0f, 0.0f));
	std::vector<l3d::L3DVertex> vertices {
	    {.position = {0.0f, 1.0f, 0.0f}, .texCoord = {0.25f, 0.75f}, .normal = {0.0f, 1.0f, 0.0f}},
	    {.position = {0.0f, 2.0f, 10.0f}, .texCoord = {0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}},
	};
	const auto original = vertices;
	BakeToLand(ground, object, vertices);
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		// y - (the temple's height - the land's under the vertex), no scale; nothing else moves
		const auto& p = original[i].position;
		const auto world = glm::vec3(object * glm::vec4(p.x, p.y, p.z, 1.0f));
		EXPECT_FLOAT_EQ(vertices[i].position.y, p.y - (4.0f - ground(glm::vec2(world.x, world.z))));
		EXPECT_EQ(vertices[i].position.x, p.x);
		EXPECT_EQ(vertices[i].position.z, p.z);
		EXPECT_EQ(vertices[i].texCoord.x, original[i].texCoord.x);
		EXPECT_EQ(vertices[i].normal.x, original[i].normal.x);
	}
	// The vertex at the temple's foot, on land at 50, goes 46 up
	EXPECT_FLOAT_EQ(vertices[0].position.y, 1.0f + 46.0f);
	// The one 10 m off along its own z is turned onto x, where the land is 5 higher or lower
	EXPECT_NEAR(std::abs(vertices[1].position.y - original[1].position.y - 46.0f), 5.0f, 1e-3f);
}

TEST(TempleExteriorMorph, StepsTowardWhereItIsToBe)
{
	EXPECT_FLOAT_EQ(Step(0.5f, 1.0f), 0.516f);
	EXPECT_FLOAT_EQ(Step(0.5f, 0.0f), 0.484f);
	EXPECT_FLOAT_EQ(Step(0.99f, 1.0f), 1.0f);
	// Near enough, it is there
	EXPECT_FLOAT_EQ(Step(0.9995f, 1.0f), 1.0f);
}

TEST(TempleExteriorMorph, ASmallNeutralTempleIsTheMiddleStage)
{
	const auto corners = Corners(0.0f, 0.5f);
	EXPECT_FLOAT_EQ(WeightOf(corners, 0, 2), 1.0f);
	EXPECT_FLOAT_EQ(std::accumulate(corners.begin(), corners.end(), 0.0f,
	                                [](float sum, const Corner& corner) { return sum + corner.weight; }),
	                1.0f);
}

TEST(TempleExteriorMorph, BetweenStagesAndSizesItBlendsTheFour)
{
	const auto corners = Corners(0.25f, 0.625f);
	// Half of size 0 and 1, and half of stage 2 and 3
	EXPECT_FLOAT_EQ(WeightOf(corners, 0, 2), 0.25f);
	EXPECT_FLOAT_EQ(WeightOf(corners, 1, 2), 0.25f);
	EXPECT_FLOAT_EQ(WeightOf(corners, 0, 3), 0.25f);
	EXPECT_FLOAT_EQ(WeightOf(corners, 1, 3), 0.25f);
}

TEST(TempleExteriorMorph, TheEndsAreTheirOwnMeshes)
{
	EXPECT_FLOAT_EQ(WeightOf(Corners(0.0f, 0.0f), 0, 0), 1.0f);
	EXPECT_FLOAT_EQ(WeightOf(Corners(1.0f, 1.0f), 2, 4), 1.0f);
}

TEST(TempleExteriorMorph, MeshNames)
{
	EXPECT_EQ(MeshName(1, 4), "b_temple14_l3d");
}

TEST(TempleExteriorMorph, TexturesGoEvilToNeutralToGood)
{
	const auto evil = TextureOf(0.0f);
	EXPECT_EQ(evil.from, Look::Evil);
	EXPECT_EQ(evil.to, Look::Neutral);
	EXPECT_EQ(evil.weight, 0);
	EXPECT_EQ(TextureOf(0.25f).weight, 127);
	EXPECT_EQ(TextureOf(0.5f).to, Look::Neutral);
	EXPECT_EQ(TextureOf(0.5f).weight, 255);
	const auto good = TextureOf(1.0f);
	EXPECT_EQ(good.from, Look::Neutral);
	EXPECT_EQ(good.to, Look::Good);
	EXPECT_EQ(good.weight, 255);
	EXPECT_EQ(ImageName(Look::Good, 3), "good3");
}

TEST(TempleExteriorMorph, TexelsBlendByChannelKeepingTheFirstsAlpha)
{
	const std::vector<uint16_t> from {0xF000, 0x1FFF};
	const std::vector<uint16_t> to {0x0FFF, 0x0000};
	std::vector<uint16_t> blended(2);
	BlendTexels(from, to, 255, blended);
	EXPECT_EQ(blended[0], 0xFFFF);
	EXPECT_EQ(blended[1], 0x1000);
	BlendTexels(from, to, 0, blended);
	EXPECT_EQ(blended, from);
	// Halfway, 15 by 128 over 255 is 7 in each
	BlendTexels(from, to, 128, blended);
	EXPECT_EQ(blended[0], 0xF777);
}
