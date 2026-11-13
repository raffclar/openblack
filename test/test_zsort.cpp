/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <bit>
#include <functional>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/FizzDrawOrder.h"
#include "Graphics/ViewOrder.h"
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
	// After the segments of all the fizzing creatures in the list
	EXPECT_LT(zsort::AtOnce(0), fizz_draw_order::SegmentDepth(fizz_draw_order::k_MaxRanked));
}

TEST(ZSort, AYardCopySortsByTheKeyOfItsWorkshop)
{
	const glm::vec3 camera {0.0f, 0.0f, 0.0f};
	// The sum of the workshop key adds the height second, so it is not the key of a placed model
	const glm::vec3 workshop {1.3f, 7.9f, 2.1f};
	EXPECT_EQ(zsort::YardCopyDepth(workshop, camera), std::bit_cast<uint32_t>(zsort::Key(workshop, camera)));
	EXPECT_NE(zsort::YardCopyDepth(workshop, camera), zsort::ModelDepth(workshop, camera));
	// A copy at a yard place nearer to the camera than a model comes before that model when its workshop is further off
	const glm::vec3 model {0.0f, 0.0f, 20.0f};
	const glm::vec3 copyPlace {0.0f, 0.0f, 18.0f};
	const glm::vec3 farWorkshop {0.0f, 0.0f, 24.0f};
	EXPECT_LT(zsort::ModelDepth(copyPlace, camera), zsort::ModelDepth(model, camera));
	EXPECT_GT(zsort::YardCopyDepth(farWorkshop, camera), zsort::ModelDepth(model, camera));
}

namespace
{

/// The list of the game: a new thing goes before the first thing of a smaller key, thus after the things of the same
/// key. The game draws the list from its start.
std::vector<size_t> GameListOrder(const std::vector<float>& keys)
{
	std::vector<size_t> list;
	for (size_t index = 0; index < keys.size(); ++index)
	{
		const auto place = std::ranges::find_if(list, [&](size_t other) { return keys.at(other) < keys.at(index); });
		list.insert(place, index);
	}
	return list;
}

/// The order of a view sorted by depth, the greatest first, with the ties in the order that the draws came in
std::vector<size_t> DescendingViewOrder(const std::vector<uint32_t>& depths)
{
	std::vector<size_t> order(depths.size());
	std::iota(order.begin(), order.end(), size_t {0});
	std::ranges::stable_sort(order, std::ranges::greater {}, [&](size_t index) { return depths.at(index); });
	return order;
}

} // namespace

TEST(ZSort, TheViewsSortedByDepthKeepTiesInTheOrderTheyCame)
{
	EXPECT_TRUE(TiesInSubmitOrder(ViewOrder::DepthDescending));
	EXPECT_TRUE(TiesInSubmitOrder(ViewOrder::DepthAscending));
	EXPECT_FALSE(TiesInSubmitOrder(ViewOrder::ByProgram));
	EXPECT_FALSE(TiesInSubmitOrder(ViewOrder::Sequential));
}

TEST(ZSort, TheGameDrawsThingsOfTheSameKeyInTheOrderItGetsThem)
{
	// Three things at one place and two at another: the first that the game gets of each key comes first
	const std::vector<float> keys {25.0f, 9.0f, 25.0f, 9.0f, 25.0f};
	EXPECT_EQ(GameListOrder(keys), (std::vector<size_t> {0, 2, 4, 1, 3}));
}

TEST(ZSort, TheTranslucentViewDrawsInTheOrderOfTheGameList)
{
	const glm::vec3 camera {100.0f, 50.0f, -20.0f};
	std::mt19937 random(7);
	std::uniform_int_distribution<int> step(0, 6);
	std::vector<glm::vec3> points;
	for (size_t index = 0; index < 400; ++index)
	{
		// Few places, so that many draws share a place and thus a key
		points.push_back(camera + glm::vec3(static_cast<float>(step(random)) * 3.5f, static_cast<float>(step(random)), 0.25f));
	}
	std::vector<float> keys;
	std::vector<uint32_t> depths;
	for (const auto& point : points)
	{
		keys.push_back(zsort::Key(point, camera));
		depths.push_back(zsort::Depth(point, camera));
	}
	EXPECT_EQ(DescendingViewOrder(depths), GameListOrder(keys));
}

// The game draws all of the copies at the yard places of a workshop in one go, in the order of the workshop's scaffold
// list. Two workshops of the same key draw in the order of the land's draw list.
TEST(ZSort, TheYardCopiesOfAWorkshopComeInTheOrderOfItsScaffoldList)
{
	EXPECT_LT(zsort::YardCopyOrder(7, 0), zsort::YardCopyOrder(7, 1));
	EXPECT_LT(zsort::YardCopyOrder(7, 1), zsort::YardCopyOrder(7, 2));
	EXPECT_LT(zsort::YardCopyOrder(7, 2), zsort::YardCopyOrder(8, 0));
	EXPECT_LT(zsort::YardCopyOrder(zsort::k_ListPlaces - 1, 2), zsort::YardCopyOrder(zsort::k_Unlisted, 0));
}
