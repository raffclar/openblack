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
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/TempleExteriorMorph.h"

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
} // namespace

TEST(TempleExteriorMorph, StepsTowardWhereItIsToBe)
{
	EXPECT_FLOAT_EQ(Step(0.5f, 1.0f), 0.516f);
	EXPECT_FLOAT_EQ(Step(0.5f, 0.0f), 0.484f);
	EXPECT_FLOAT_EQ(Step(0.99f, 1.0f), 1.0f);
	// Near enough, it is there
	EXPECT_FLOAT_EQ(Step(0.9995f, 1.0f), 1.0f);
}

TEST(TempleExteriorMorph, OnTheFirstLandEveryTempleGrowsTowardTheSameSmallSize)
{
	EXPECT_FLOAT_EQ(SizeTarget(1, std::nullopt), 0.02f);
	EXPECT_FLOAT_EQ(SizeTarget(1, 0.5f), 0.02f);
	// On the other lands the size is twice the share, or twice the share of no influence when nobody has any
	EXPECT_FLOAT_EQ(SizeTarget(2, 0.3f), 0.6f);
	EXPECT_FLOAT_EQ(SizeTarget(2, std::nullopt), 0.02f);
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

// At the lowest detail levels each picture first takes the texel at the start of each two by two square, then the half
// pictures blend into the half-size skin
TEST(TempleExteriorMorph, AHalfSizeSkinBlendsTheFirstTexelOfEachSquare)
{
	constexpr size_t k_Side = 256;
	std::vector<uint16_t> from(k_Side * k_Side, 0xF000);
	std::vector<uint16_t> to(k_Side * k_Side, 0xF000);
	// Each texel of a square has another colour, so that a mean of the square differs from the first texel
	for (size_t row = 0; row < k_Side; ++row)
	{
		for (size_t column = 0; column < k_Side; ++column)
		{
			const auto corner = static_cast<uint16_t>(((row % 2) * 2) + (column % 2));
			to[(row * k_Side) + column] = static_cast<uint16_t>(0xF000 | (corner * 5));
		}
	}
	from[(2 * k_Side) + 2] = 0xFF00;

	const auto full = SkinTexels(from, to, 255, false);
	ASSERT_TRUE(full.has_value());
	EXPECT_EQ(full->size(), k_Side * k_Side);
	EXPECT_EQ((*full)[1], 0xF005);

	const auto half = SkinTexels(from, to, 255, true);
	ASSERT_TRUE(half.has_value());
	ASSERT_EQ(half->size(), (k_Side / 2) * (k_Side / 2));
	EXPECT_EQ((*half)[0], 0xF000);
	EXPECT_EQ((*half)[1], 0xF000);
	// The first picture's texel at row 2 and column 2 is the half picture's texel at row 1 and column 1
	const auto zeroWeight = SkinTexels(from, to, 0, true);
	ASSERT_TRUE(zeroWeight.has_value());
	EXPECT_EQ((*zeroWeight)[(k_Side / 2) + 1], 0xFF00);
	EXPECT_EQ((*zeroWeight)[(k_Side / 2) + 2], 0xF000);

	EXPECT_FALSE(SkinTexels(std::span<const uint16_t>(from).first(100), to, 0, true).has_value());
}

// The mesh is blended again when the look changes, before its first blend, and when its blended mesh is not loaded, as
// after a load of a saved game that keeps the look
TEST(TempleExteriorMorph, TheMeshIsBlendedAgainWhenItIsNotLoaded)
{
	const glm::vec2 look {0.2f, 0.5f};
	EXPECT_TRUE(NeedsBlend(std::nullopt, look, false));
	EXPECT_TRUE(NeedsBlend(std::nullopt, look, true));
	EXPECT_TRUE(NeedsBlend(glm::vec2 {0.1f, 0.5f}, look, true));
	EXPECT_FALSE(NeedsBlend(look, look, true));
	EXPECT_TRUE(NeedsBlend(look, look, false));
}

// After a load each temple with a kept look gets a new blend when the cache has no blend of its own. A network game
// loads the server's world over a world that this computer blended for another look.
TEST(TempleExteriorMorph, EveryTempleWithALookIsBlendedAgainAfterALoad)
{
	const Blend wanted {.look = {0.2f, 0.5f}, .textureSet = 1};
	EXPECT_TRUE(BlendsAfterLoad(glm::vec2 {0.2f, 0.5f}, std::nullopt, wanted));
	EXPECT_TRUE(BlendsAfterLoad(glm::vec2 {0.0f, 0.0f}, std::nullopt, wanted));
	EXPECT_FALSE(BlendsAfterLoad(std::nullopt, std::nullopt, wanted));
}

// A load of a world state over the same world keeps each mesh that the cache blended for the same look and textures.
// A mesh of another look or of another player's textures is blended again.
TEST(TempleExteriorMorph, AMeshInTheCacheOfTheSameBlendStaysAfterALoad)
{
	const Blend wanted {.look = {0.2f, 0.5f}, .textureSet = 1};
	EXPECT_FALSE(BlendsAfterLoad(glm::vec2 {0.2f, 0.5f}, wanted, wanted));
	EXPECT_TRUE(BlendsAfterLoad(glm::vec2 {0.2f, 0.5f}, Blend {.look = {0.2f, 0.6f}, .textureSet = 1}, wanted));
	EXPECT_TRUE(BlendsAfterLoad(glm::vec2 {0.2f, 0.5f}, Blend {.look = {0.3f, 0.5f}, .textureSet = 1}, wanted));
	EXPECT_TRUE(BlendsAfterLoad(glm::vec2 {0.2f, 0.5f}, Blend {.look = {0.2f, 0.5f}, .textureSet = 2}, wanted));
}

// The meshes made for a temple are its own mesh and the mesh of its worship sites in its skin. A land change lets go of
// both, by the names that the cache keeps them under.
TEST(TempleExteriorMorph, TheMeshesMadeForATempleAreItsOwnAndItsSitesMesh)
{
	EXPECT_EQ(OwnMeshName(42), "temple/exterior/42");
	const auto templeMesh = entt::hashed_string(OwnMeshName(42).c_str()).value();
	EXPECT_EQ(SiteMeshName(templeMesh), fmt::format("temple/worship/{}", templeMesh));
	const auto made = MeshesMadeFor(templeMesh);
	EXPECT_EQ(made.at(0), templeMesh);
	EXPECT_EQ(made.at(1), entt::hashed_string(SiteMeshName(templeMesh).c_str()).value());
	EXPECT_NE(made.at(0), made.at(1));
	EXPECT_NE(MeshesMadeFor(templeMesh + 1).at(1), made.at(1));
}
