/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <bit>
#include <limits>

#include <gtest/gtest.h>

#include "Graphics/FizzDrawOrder.h"
#include "Graphics/ZSort.h"

using namespace openblack::graphics;

TEST(ZSort, KeyIsTheDistanceSquared)
{
	EXPECT_FLOAT_EQ(zsort::Key({3.0f, 4.0f, 12.0f}, {0.0f, 0.0f, 0.0f}), 169.0f);
	EXPECT_FLOAT_EQ(zsort::Key({1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}), 0.0f);
}

TEST(ZSort, DepthOrdersAsTheDistance)
{
	const glm::vec3 camera {100.0f, 50.0f, -20.0f};
	float previous = 0.0f;
	uint32_t previousDepth = 0;
	for (float far = 0.01f; far < 20000.0f; far = (far * 1.37f) + 0.01f)
	{
		const glm::vec3 point = camera + glm::vec3(far, 0.0f, 0.0f);
		const auto key = zsort::Key(point, camera);
		const auto depth = zsort::Depth(point, camera);
		if (key > previous)
		{
			EXPECT_GT(depth, previousDepth);
		}
		previous = key;
		previousDepth = depth;
	}
}

TEST(ZSort, BeforeSortedComesFirstAndTheNearestFirst)
{
	const glm::vec3 camera {100.0f, 50.0f, -20.0f};
	const auto nearer = zsort::BeforeSorted(camera + glm::vec3(10.0f, 0.0f, 0.0f), camera);
	const auto farther = zsort::BeforeSorted(camera + glm::vec3(40.0f, 0.0f, 0.0f), camera);
	EXPECT_GT(nearer, farther);
	// Above anything sorted by its distance, however far, with room for the draws after it
	EXPECT_GT(farther - 15u, zsort::Depth(camera + glm::vec3(1.0e6f, 0.0f, 0.0f), camera));
	EXPECT_EQ(nearer & 0xFu, 0xFu);
}

TEST(ZSort, TheLearningWordsComeAfterTheFizzingCreaturesAndBeforeEverythingSorted)
{
	const glm::vec3 camera {100.0f, 50.0f, -20.0f};
	const auto farthest = std::numeric_limits<float>::infinity();
	// The farthest fizzing creature, with the draws that follow it in turn
	EXPECT_GT(zsort::BeforeSorted(camera + glm::vec3(farthest, 0.0f, 0.0f), camera) - 15u, zsort::k_AfterSolidBeforeSorted);
	EXPECT_GT(zsort::k_AfterSolidBeforeSorted, zsort::Depth(camera + glm::vec3(farthest, 0.0f, 0.0f), camera));
}

TEST(ZSort, EachKindAddsTheSquaresInItsOwnOrder)
{
	// These distances give three different single precision sums, one for each order of the additions
	const glm::vec3 camera {0.0f, 0.0f, 0.0f};
	const glm::vec3 point {1.3f, 7.9f, 2.1f};
	const float x = point.x * point.x;
	const float y = point.y * point.y;
	const float z = point.z * point.z;
	EXPECT_EQ(zsort::Key(point, camera), (x + y) + z);
	EXPECT_EQ(zsort::ModelKey(point, camera), (x + z) + y);
	EXPECT_EQ(zsort::FloatingNumberKey(point, camera), (y + z) + x);
	EXPECT_NE(zsort::Key(point, camera), zsort::ModelKey(point, camera));
	EXPECT_NE(zsort::Key(point, camera), zsort::FloatingNumberKey(point, camera));
	EXPECT_EQ(zsort::ModelDepth(point, camera), std::bit_cast<uint32_t>(zsort::ModelKey(point, camera)));
	EXPECT_EQ(zsort::FloatingNumberDepth(point, camera), std::bit_cast<uint32_t>(zsort::FloatingNumberKey(point, camera)));
}

// The game draws a model that blends and is not sorted at its place in the land's draw list. It comes after the solid
// models and the fizzing creatures, and before every sorted thing, such as the dome of a globe.
TEST(ZSort, AModelDrawnAtOnceComesAtItsPlaceInTheListBeforeEverythingSorted)
{
	const glm::vec3 camera {100.0f, 50.0f, -20.0f};
	const auto farthest = std::numeric_limits<float>::infinity();
	// The first place of the list first
	EXPECT_GT(zsort::AtOnce(0), zsort::AtOnce(1));
	EXPECT_GT(zsort::AtOnce(2998), zsort::AtOnce(2999));
	// A thing out of the list after the things of the list
	EXPECT_GT(zsort::AtOnce(zsort::k_ListPlaces - 1), zsort::AtOnce(zsort::k_Unlisted));
	EXPECT_EQ(zsort::AtOnce(zsort::k_Unlisted), zsort::AtOnce(zsort::k_Unlisted + 10));
	// Before every sorted thing, however near it is
	EXPECT_GT(zsort::AtOnce(zsort::k_Unlisted), zsort::k_AfterSolidBeforeSorted);
	EXPECT_GT(zsort::AtOnce(zsort::k_Unlisted), zsort::ModelDepth(camera, camera));
	EXPECT_GT(zsort::AtOnce(zsort::k_Unlisted), zsort::Depth(camera + glm::vec3(farthest, 0.0f, 0.0f), camera));
	// After the farthest fizzing creature, with the draws that follow it
	EXPECT_LT(zsort::AtOnce(0), zsort::BeforeSorted(camera + glm::vec3(farthest, 0.0f, 0.0f), camera) - 15u);
	// After the segments of many fizzing creatures in the list
	EXPECT_LT(zsort::AtOnce(0), fizz_draw_order::SegmentDepth(1000000));
}
