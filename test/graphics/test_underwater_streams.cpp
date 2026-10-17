/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the experiment's streams and of the splash where the hand taps the water

#include <cmath>
#include <cstdint>

#include <array>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/UnderwaterStreams.h"
#include "Graphics/UnderwaterTap.h"

namespace streams = openblack::graphics::underwater::streams;
namespace tap = openblack::graphics::underwater::tap;

namespace
{
/// A land that falls along x: one unit for each ten units along x, from 40 at x = 0
float Sloped(glm::vec2 point)
{
	return std::max(40.0f - point.x * 0.1f, 0.0f);
}

/// A straight river along x, given from its low end to its high end
std::vector<std::vector<glm::vec2>> UphillRiver()
{
	return {{{300.0f, 0.0f}, {200.0f, 0.0f}, {100.0f, 0.0f}, {0.0f, 0.0f}}};
}

/// A channel model as the original's: 32 by 64 texels over 31.8 by 10.6 units, clear in the four middle columns
streams::Channel TestChannel()
{
	std::vector<uint8_t> alpha(32 * 64, 15);
	for (int row = 0; row < 64; ++row)
	{
		for (int column = 14; column < 18; ++column)
		{
			alpha.at(static_cast<size_t>(row * 32 + column)) = 10;
		}
	}
	const std::array<glm::vec2, 3> model {{{30.86f, 5.46f}, {-0.94f, 5.46f}, {30.86f, -5.14f}}};
	const std::array<glm::vec2, 3> texels {{{0.02f, 0.03f}, {0.02f, 63.97f}, {31.98f, 0.03f}}};
	return streams::ChannelFrom({32, 64}, std::move(alpha), model, texels);
}
} // namespace

TEST(UnderwaterStreams, SpeedGrowsWithTheSlopeWithinLimits)
{
	EXPECT_FLOAT_EQ(streams::SpeedForSlope(0.0f), streams::k_LeastSpeed);
	EXPECT_FLOAT_EQ(streams::SpeedForSlope(-1.0f), streams::k_LeastSpeed);
	EXPECT_FLOAT_EQ(streams::SpeedForSlope(10.0f), streams::k_MostSpeed);
	EXPECT_LT(streams::SpeedForSlope(0.05f), streams::SpeedForSlope(0.2f));
}

TEST(UnderwaterStreams, WhiteWaterOnlyOnSteepSlopes)
{
	EXPECT_FLOAT_EQ(streams::WhiteWaterForSlope(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(streams::WhiteWaterForSlope(streams::k_CalmSlope), 0.0f);
	EXPECT_FLOAT_EQ(streams::WhiteWaterForSlope(streams::k_WhiteSlope), 1.0f);
	EXPECT_GT(streams::WhiteWaterForSlope(0.25f), 0.0f);
}

TEST(UnderwaterStreams, BendsSlowTheWater)
{
	EXPECT_FLOAT_EQ(streams::BendFactor({1.0f, 0.0f}, {1.0f, 0.0f}), 1.0f);
	EXPECT_NEAR(streams::BendFactor({1.0f, 0.0f}, {0.0f, 1.0f}), 0.7f, 1e-5f);
	EXPECT_LT(streams::BendFactor({1.0f, 0.0f}, {-1.0f, 0.0f}), streams::BendFactor({1.0f, 0.0f}, {0.0f, 1.0f}));
}

TEST(UnderwaterStreams, RiversFlowDownhillWhateverTheOrderOfTheirPoints)
{
	const auto rivers = UphillRiver();
	const auto network = streams::BuildNetwork(rivers, Sloped);
	ASSERT_EQ(network.segments.size(), 3u);
	// The land falls along x, so the water flows towards +x though the points come the other way
	for (const auto& segment : network.segments)
	{
		EXPECT_GT(segment.to.x, segment.from.x);
		EXPECT_GT(segment.speedFrom, streams::k_LeastSpeed);
	}
	// The original's channel keeps the script's direction
	EXPECT_FLOAT_EQ(network.segments.front().modelFrom.x, 100.0f);
	EXPECT_FLOAT_EQ(network.segments.front().modelTo.x, 0.0f);
}

TEST(UnderwaterStreams, ALowEndIsAMouth)
{
	const std::vector<std::vector<glm::vec2>> rivers {{{0.0f, 0.0f}, {200.0f, 0.0f}, {400.0f, 0.0f}}};
	const auto network = streams::BuildNetwork(rivers, Sloped);
	ASSERT_EQ(network.mouths.size(), 1u);
	EXPECT_FLOAT_EQ(network.mouths[0].position.x, 400.0f);
	EXPECT_NEAR(network.mouths[0].direction.x, 1.0f, 1e-5f);
	// A river that ends high on the land has no mouth
	const auto high = streams::BuildNetwork(UphillRiver(), [](glm::vec2 point) { return 100.0f - point.x * 0.1f; });
	EXPECT_TRUE(high.mouths.empty());
}

TEST(UnderwaterStreams, APoolIsSlowAndCalm)
{
	// The land falls, rises in a dip, then falls again: the water stands in the dip
	const auto dipped = [](glm::vec2 point) {
		const float x = point.x;
		return x < 100.0f ? 60.0f - x * 0.3f : (x < 200.0f ? 30.0f + (x - 100.0f) * 0.05f : 35.0f - (x - 200.0f) * 0.3f);
	};
	std::vector<std::vector<glm::vec2>> rivers {{}};
	for (int i = 0; i <= 30; ++i)
	{
		rivers[0].emplace_back(static_cast<float>(i) * 10.0f, 0.0f);
	}
	const auto network = streams::BuildNetwork(rivers, dipped);
	const auto& steep = network.segments.at(4);
	const auto& pool = network.segments.at(15);
	EXPECT_GT(steep.speedFrom, pool.speedFrom * 2.0f);
	EXPECT_GT(steep.whiteFrom, pool.whiteFrom);
}

TEST(UnderwaterStreams, TheFlowMapFollowsTheRiver)
{
	const auto network = streams::BuildNetwork(UphillRiver(), Sloped);
	const auto map = streams::BuildFlowMap(network);
	ASSERT_FALSE(map.texels.empty());
	const auto onRiver = streams::Sample(map, {150.0f, 0.0f});
	EXPECT_NEAR(onRiver.distance, 0.0f, 0.01f);
	EXPECT_GT(onRiver.flow.x, 0.0f);
	EXPECT_NEAR(onRiver.flow.y, 0.0f, 1e-4f);
	const auto beside = streams::Sample(map, {150.0f, 5.0f});
	EXPECT_NEAR(beside.distance, 5.0f, 0.01f);
	const auto far = streams::Sample(map, {150.0f, 25.0f});
	EXPECT_FLOAT_EQ(far.distance, streams::k_FarDistance);
	EXPECT_FLOAT_EQ(glm::length(far.flow), 0.0f);
	// Outside the map: no flow
	const auto outside = streams::Sample(map, {-500.0f, 0.0f});
	EXPECT_FLOAT_EQ(outside.distance, streams::k_FarDistance);
	EXPECT_EQ(streams::PackFlowMap(map).size(), map.texels.size() * 4);
}

TEST(UnderwaterStreams, TheChannelIsWhereTheOriginalCutsIt)
{
	const auto channel = TestChannel();
	// The middle of the model across its width is the clear column
	EXPECT_NEAR(streams::ChannelAlpha(channel, {15.0f, 0.16f}), 10.0f / 15.0f, 1e-5f);
	EXPECT_FLOAT_EQ(streams::ChannelAlpha(channel, {15.0f, 4.0f}), 1.0f);
	EXPECT_FLOAT_EQ(streams::ChannelAlpha(channel, {50.0f, 0.0f}), 1.0f);

	// Laid along a river, it leaves its alpha along the river's line and not beside it
	const std::vector<std::vector<glm::vec2>> rivers {{{0.0f, 0.0f}, {30.0f, 0.0f}, {60.0f, 0.0f}}};
	const auto network = streams::BuildNetwork(rivers, Sloped);
	EXPECT_LT(streams::ChannelAt(network, channel, {15.0f, 0.16f}), streams::k_WaterAlpha);
	EXPECT_LT(streams::ChannelAt(network, channel, {45.0f, 0.16f}), streams::k_WaterAlpha);
	EXPECT_FLOAT_EQ(streams::ChannelAt(network, channel, {15.0f, 4.0f}), 1.0f);
	EXPECT_FLOAT_EQ(streams::ChannelAt(network, channel, {15.0f, 40.0f}), 1.0f);
}

TEST(UnderwaterStreams, TheWaterIsDeepestInTheMiddle)
{
	EXPECT_FLOAT_EQ(streams::DepthForAlpha(1.0f), 0.0f);
	EXPECT_NEAR(streams::DepthForAlpha(2.0f / 3.0f), streams::k_MostDepth, 1e-5f);
	EXPECT_LT(streams::DepthForAlpha(0.9f), streams::DepthForAlpha(0.7f));
}

TEST(UnderwaterStreams, LoudnessFallsWithDistanceAndGrowsAtRapids)
{
	const auto network = streams::BuildNetwork(UphillRiver(), Sloped);
	const float near = streams::Loudness(network, {150.0f, 2.0f});
	const float far = streams::Loudness(network, {150.0f, 200.0f});
	EXPECT_GT(near, far);
	EXPECT_LE(near, 1.0f);
	EXPECT_GE(far, 0.0f);
	const auto steep = streams::BuildNetwork(UphillRiver(), [](glm::vec2 point) { return 200.0f - point.x * 0.6f; });
	EXPECT_GT(streams::Loudness(steep, {150.0f, 30.0f}), streams::Loudness(network, {150.0f, 30.0f}));
}

TEST(UnderwaterStreams, DriftersKeepToTheMiddle)
{
	const streams::FlowTexel middle {.flow = {1.0f, 0.0f}, .white = 0.0f, .distance = 0.0f};
	EXPECT_FLOAT_EQ(streams::DriftVelocity(middle, {0.0f, 1.0f}).y, 0.0f);
	EXPECT_FLOAT_EQ(streams::DriftVelocity(middle, {0.0f, 1.0f}).x, 1.0f);
	const streams::FlowTexel bank {.flow = {1.0f, 0.0f}, .white = 0.0f, .distance = 1.5f};
	EXPECT_GT(streams::DriftVelocity(bank, {0.0f, 1.0f}).y, 0.0f);
}

TEST(UnderwaterTap, HarderTapsSplashMore)
{
	const auto soft = tap::SplashFor(tap::k_SoftestTap, 0.0f, {0.1f, 0.2f, 0.3f});
	const auto hard = tap::SplashFor(tap::k_HardestTap, 0.0f, {0.1f, 0.2f, 0.3f});
	EXPECT_GT(hard.sheetHeight, soft.sheetHeight);
	EXPECT_GT(hard.dropSpeed, soft.dropSpeed);
	EXPECT_GT(hard.drops, soft.drops);
	EXPECT_LE(hard.drops, tap::k_DropParticles);
	EXPECT_GT(hard.seconds, 0.0f);
	// Out of range strengths are held to the range
	EXPECT_FLOAT_EQ(tap::SplashFor(100.0f, 0.0f, {}).sheetHeight, hard.sheetHeight);
}

TEST(UnderwaterTap, CalmWaterSplashesLess)
{
	const auto sea = tap::SplashFor(1.0f, 0.0f, {});
	const auto pond = tap::SplashFor(1.0f, 1.0f, {});
	EXPECT_LT(pond.sheetHeight, sea.sheetHeight);
	EXPECT_LT(pond.spray, sea.spray);
	EXPECT_LE(pond.drops, sea.drops);
}

TEST(UnderwaterTap, StrengthFromTheHandsSpeed)
{
	EXPECT_LT(tap::StrengthForHandSpeed(0.0f), 1.0f);
	EXPECT_FLOAT_EQ(tap::StrengthForHandSpeed(1000.0f), tap::k_HardestTap);
	EXPECT_LT(tap::StrengthForHandSpeed(2.0f), tap::StrengthForHandSpeed(8.0f));
}

TEST(UnderwaterTap, DropsLandBackOnTheWater)
{
	const auto splash = tap::SplashFor(1.2f, 0.0f, {0.37f, 0.61f, 0.13f});
	for (uint32_t i = 0; i < splash.drops; ++i)
	{
		const auto drop = tap::DropOf(splash, i);
		EXPECT_TRUE(drop.shown);
		EXPECT_GT(drop.velocity.y, 0.0f);
		const auto landing = tap::LandingOf(drop);
		const float flight = landing.seconds - drop.delay;
		// Its height at the landing is back at the level it left from
		EXPECT_NEAR(drop.velocity.y * flight - 0.5f * streams::k_Gravity * flight * flight, 0.0f, 1e-3f);
		// It lands farther out than it started
		EXPECT_GT(glm::length(landing.position), glm::length(drop.start));
	}
	EXPECT_FALSE(tap::DropOf(splash, splash.drops).shown);
	// The same splash gives the same drops
	EXPECT_FLOAT_EQ(tap::DropOf(splash, 5).velocity.x, tap::DropOf(splash, 5).velocity.x);
}

TEST(UnderwaterTap, TheBiggestDropsMakeRingsInTheOrderTheyLand)
{
	const auto splash = tap::SplashFor(1.4f, 0.0f, {0.5f, 0.25f, 0.75f});
	const auto landings = tap::RingLandings(splash);
	ASSERT_EQ(landings.size(), tap::k_MostDropRings);
	for (size_t i = 1; i < landings.size(); ++i)
	{
		EXPECT_LE(landings[i - 1].seconds, landings[i].seconds);
	}
	EXPECT_GT(tap::DropRingDepth(0.08f), tap::DropRingDepth(0.03f));
	EXPECT_LE(tap::DropRingDepth(10.0f), 0.25f);
}
