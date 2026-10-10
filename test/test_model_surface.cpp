/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <utility>
#include <vector>

#include <L3DFile.h>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/ModelSurface.h"
#include "Common/GameRandom.h"

using namespace openblack;
using namespace openblack::model_surface;

namespace
{

/// Whole-number draws give the next of a list, fractions the next of another; it counts the draws
class ListedRandom final: public GameRandomInterface
{
public:
	ListedRandom(std::vector<uint32_t> wholes, std::vector<float> fractions)
	    : _wholes(std::move(wholes))
	    , _fractions(std::move(fractions))
	{
	}
	uint32_t GameRand(uint32_t /*n*/) override { return 0; }
	float GameFloatRand(float /*x*/) override { return 0.0f; }
	uint32_t LocalRand(int32_t n) override
	{
		lastCount = n;
		++draws;
		return _wholes.empty() ? 0 : _wholes.at(_whole++ % _wholes.size());
	}
	float LocalFloatRand(float /*x*/) override { return _fractions.at(_fraction++ % _fractions.size()); }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds /*seeds*/) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) override {}

	int32_t lastCount {0};
	uint32_t draws {0};

private:
	std::vector<uint32_t> _wholes;
	std::vector<float> _fractions;
	size_t _whole {0};
	size_t _fraction {0};
};

l3d::L3DVertex Vertex(float x, float y, float z, float ny)
{
	l3d::L3DVertex vertex {};
	vertex.position = {x, y, z};
	vertex.normal = {0.0f, ny, 0.0f};
	return vertex;
}

/// A model of one part of two primitives, a triangle each, the second's indices counting from its own first vertex
l3d::L3DFile OnePartModel(uint32_t lodMask, uint32_t status)
{
	l3d::L3DFile model;
	l3d::L3DSubmeshHeader part {};
	part.flags.lodMask = lodMask;
	part.flags.status = status;
	model.AddSubmesh(part);
	l3d::L3DPrimitiveHeader first {};
	first.numVertices = 3;
	first.numTriangles = 1;
	auto second = first;
	model.AddPrimitives({first, second});
	model.AddVertices({Vertex(0, 0, 0, 1), Vertex(1, 0, 0, 1), Vertex(0, 0, 1, 1), Vertex(0, 5, 0, -1), Vertex(1, 5, 0, -1),
	                   Vertex(0, 5, 1, -1)});
	model.AddIndices({0, 1, 2, 2, 1, 0});
	return model;
}

} // namespace

TEST(ModelSurface, TheDrawnTrianglesAreThoseOfTheDrawnPartsInOrder)
{
	const auto model = OnePartModel(7, 0);
	const auto triangles = DrawnTriangles(model, {.detailLevels = 1, .latestState = 0});
	ASSERT_EQ(triangles.size(), 2U);
	EXPECT_EQ(triangles[0][1].position, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(triangles[0][0].normal, glm::vec3(0.0f, 1.0f, 0.0f));
	// The second primitive's first index is its own first vertex
	EXPECT_EQ(triangles[1][0].position, glm::vec3(0.0f, 5.0f, 1.0f));
	EXPECT_EQ(triangles[1][2].position, glm::vec3(0.0f, 5.0f, 0.0f));

	// A part at no detail level (the physics' shape) is never drawn; one of a later state only once it is reached
	EXPECT_TRUE(DrawnTriangles(OnePartModel(0, 0), {.detailLevels = 7, .latestState = 63}).empty());
	EXPECT_TRUE(DrawnTriangles(OnePartModel(2, 0), {.detailLevels = 1, .latestState = 0}).empty());
	EXPECT_TRUE(DrawnTriangles(OnePartModel(7, 1), {.detailLevels = 1, .latestState = 0}).empty());
	EXPECT_EQ(DrawnTriangles(OnePartModel(7, 1), {.detailLevels = 1, .latestState = 1}).size(), 2U);
}

TEST(ModelSurface, ARandomPointIsBlendedFromItsTriangleAndPlaced)
{
	const glm::vec3 slanted = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
	const std::vector<Triangle> triangles {
	    {{{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, {{2.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 2.0f}, slanted}}},
	    {{{{0.0f, 9.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	      {{2.0f, 9.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	      {{0.0f, 9.0f, 2.0f}, {0.0f, 1.0f, 0.0f}}}},
	};
	ListedRandom random({0}, {0.5f, 0.25f});
	// Moved, turned a quarter about y (x goes to -z) and doubled
	const auto placement = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f)) *
	                       glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
	                       glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
	const auto point = RandomPoint(triangles, placement, random);
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(random.lastCount, 2);
	// In the model: (1, 0, 0.5), placed at (10 + 1, 20, 30 - 2)
	EXPECT_NEAR(point->position.x, 11.0f, 1e-5f);
	EXPECT_NEAR(point->position.y, 20.0f, 1e-5f);
	EXPECT_NEAR(point->position.z, 28.0f, 1e-5f);
	// The normal is blended as the point is, turned and made of unit length
	const auto blended = glm::vec3(0.5f + 0.25f * slanted.x, 1.0f - 0.5f + 0.25f * (slanted.y - 1.0f), 0.0f);
	const auto expected = glm::normalize(glm::vec3(0.0f, blended.y, -blended.x));
	EXPECT_NEAR(point->normal.x, expected.x, 1e-5f);
	EXPECT_NEAR(point->normal.y, expected.y, 1e-5f);
	EXPECT_NEAR(point->normal.z, expected.z, 1e-5f);
}

TEST(ModelSurface, FractionsAddingUpPastOneAreFoldedBack)
{
	const std::vector<Triangle> triangles {
	    {{{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	      {{4.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	      {{0.0f, 0.0f, 4.0f}, {0.0f, 1.0f, 0.0f}}}},
	};
	ListedRandom past({0}, {0.75f, 0.5f});
	EXPECT_EQ(RandomPoint(triangles, glm::mat4(1.0f), past)->position, glm::vec3(1.0f, 0.0f, 2.0f));
	// Exactly one stays as it is
	ListedRandom edge({0}, {0.75f, 0.25f});
	EXPECT_EQ(RandomPoint(triangles, glm::mat4(1.0f), edge)->position, glm::vec3(3.0f, 0.0f, 1.0f));
}

TEST(ModelSurface, PointsAreDrawnUntilOneFacesNoMoreThanALittleDown)
{
	const auto facing = [](float y) {
		const glm::vec3 normal = glm::normalize(glm::vec3(1.0f, y, 0.0f));
		return Triangle {{{{0.0f, 0.0f, 0.0f}, normal}, {{1.0f, 0.0f, 0.0f}, normal}, {{0.0f, 0.0f, 1.0f}, normal}}};
	};
	// The first faces a little too far down; the second just far enough
	const std::vector<Triangle> triangles {facing(-0.2f), facing(-0.05f)};
	ListedRandom random({0, 1}, {0.1f, 0.1f});
	const auto point = RandomUpwardPoint(triangles, glm::mat4(1.0f), random);
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(random.draws, 2U);
	EXPECT_GE(point->normal.y, k_LowestNormalY);

	// Nothing to draw from, or nothing facing up: none
	EXPECT_FALSE(RandomUpwardPoint({}, glm::mat4(1.0f), random).has_value());
	ListedRandom down({0}, {0.1f});
	EXPECT_FALSE(RandomUpwardPoint(std::vector<Triangle> {facing(-1.0f)}, glm::mat4(1.0f), down).has_value());
	EXPECT_EQ(down.draws, k_MostDraws);
}
