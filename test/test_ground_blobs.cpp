/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Graphics/GroundBlobs.h"

using namespace openblack::graphics;

namespace
{
void ExpectNear(const glm::vec3& a, const glm::vec3& b)
{
	EXPECT_NEAR(a.x, b.x, 1e-5f);
	EXPECT_NEAR(a.y, b.y, 1e-5f);
	EXPECT_NEAR(a.z, b.z, 1e-5f);
}
} // namespace

TEST(GroundBlobs, FallsAlongTheDiagonalOnFlatLand)
{
	ExpectNear(ground_blobs::Fall({0.0f, 1.0f, 0.0f}, 1.0f), {1.41421356f, 0.0f, 1.41421356f});
	ExpectNear(ground_blobs::Fall({0.0f, 1.0f, 0.0f}, 2.0f), {2.82842712f, 0.0f, 2.82842712f});
}

TEST(GroundBlobs, FallLiesOnASlope)
{
	// On land tilted towards x, the fall loses the part of it into the land
	const glm::vec3 normal {0.6f, 0.8f, 0.0f};
	const auto fall = ground_blobs::Fall(normal, 1.0f);
	EXPECT_NEAR(glm::dot(fall, normal), 0.0f, 1e-5f);
}

TEST(GroundBlobs, QuadStartsBehindTheFootAndCrossesIt)
{
	const auto quad = ground_blobs::MakeQuad({10.0f, 5.0f, 20.0f}, {1.0f, 0.0f, 1.0f});
	const float a = 0.14142136f;
	ExpectNear(quad.corners[0], {10.0f - 0.02f + a, 5.0f, 20.0f - 0.02f - a});
	ExpectNear(quad.corners[1], {10.0f - 0.02f - a, 5.0f, 20.0f - 0.02f + a});
	ExpectNear(quad.corners[2], {11.0f - a, 5.0f, 21.0f + a});
	ExpectNear(quad.corners[3], {11.0f + a, 5.0f, 21.0f - a});
}

TEST(GroundBlobs, FeetLeanTowardsEachOther)
{
	const glm::vec3 first {0.0f, 0.0f, 0.0f};
	const glm::vec3 second {1.0f, 0.0f, 0.0f};
	const glm::vec3 fall {0.0f, 0.0f, 2.0f};
	const auto feet = ground_blobs::Feet(first, second, fall);
	// Each far end is the foot plus its fall, leaning half the way to the other foot
	ExpectNear((feet[0].corners[2] + feet[0].corners[3]) * 0.5f, first + fall + glm::vec3(0.5f, 0.0f, 0.0f));
	ExpectNear((feet[1].corners[2] + feet[1].corners[3]) * 0.5f, second + fall - glm::vec3(0.5f, 0.0f, 0.0f));
}

TEST(GroundBlobs, FarBlobIsWiderAndDoubled)
{
	const glm::vec3 foot {10.0f, 5.5f, 20.0f};
	const auto quads = ground_blobs::Far(foot, 1.0f);
	// The same quad twice
	for (size_t c = 0; c < 4; ++c)
	{
		ExpectNear(quads[0].corners.at(c), quads[1].corners.at(c));
	}
	// 1.2 across where the ordinary one is 0.4, falling along the flat diagonal
	EXPECT_NEAR(glm::distance(quads[0].corners[0], quads[0].corners[1]), 1.2f, 1e-5f);
	const auto near = ground_blobs::MakeQuad(foot, ground_blobs::Fall({0.0f, 1.0f, 0.0f}, 1.0f));
	EXPECT_NEAR(glm::distance(near.corners[0], near.corners[1]), 0.4f, 1e-5f);
	ExpectNear((quads[0].corners[2] + quads[0].corners[3]) * 0.5f, foot + glm::vec3(1.41421356f, 0.0f, 1.41421356f));
}

TEST(GroundBlobs, SmudgeStandsOverTheVillager)
{
	const glm::vec3 right {1.0f, 0.0f, 0.0f};
	const glm::vec3 up {0.0f, 1.0f, 0.0f};
	// Sized by the first far villager's scale, raised by its own
	const auto corners = ground_blobs::SmudgeCorners({0.0f, 0.0f, 0.0f}, 2.0f, 1.0f, right, up);
	ExpectNear(corners[0], {-0.3f, 1.6f + 0.9f, 0.0f});
	ExpectNear(corners[1], {0.3f, 1.6f + 0.9f, 0.0f});
	ExpectNear(corners[2], {0.3f, 1.6f - 0.9f, 0.0f});
	ExpectNear(corners[3], {-0.3f, 1.6f - 0.9f, 0.0f});
}

TEST(GroundBlobs, FeetComeFromThePoseItIsDrawnIn)
{
	const std::vector<glm::mat4> rest(3, glm::mat4(1.0f));
	const std::vector<glm::mat4> posed(3, glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f)));
	const auto bones = ground_blobs::FootBones(posed, rest);
	ASSERT_EQ(bones.size(), 3u);
	EXPECT_EQ(bones.data(), posed.data());
}

TEST(GroundBlobs, FeetComeFromTheRestingBonesWithoutAPose)
{
	const std::vector<glm::mat4> rest(3, glm::mat4(1.0f));
	EXPECT_EQ(ground_blobs::FootBones({}, rest).data(), rest.data());
	// A pose for another model is no pose for this one
	const std::vector<glm::mat4> other(2, glm::mat4(1.0f));
	EXPECT_EQ(ground_blobs::FootBones(other, rest).data(), rest.data());
}
