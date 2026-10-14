/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <chrono>
#include <numbers>

#include <gtest/gtest.h>

#include "ECS/RewardOpening.h"
#include "Graphics/LightShine.h"

using namespace openblack;
using namespace std::chrono_literals;

TEST(LightShine, TheTextureTurnsOnceEachTwentySecondsTheNegativeWay)
{
	EXPECT_DOUBLE_EQ(graphics::light_shine::TextureTurn(0ms), 0.0);
	EXPECT_NEAR(graphics::light_shine::TextureTurn(5000ms), -std::numbers::pi / 2.0, 1e-6);
	EXPECT_NEAR(graphics::light_shine::TextureTurn(10000ms), -std::numbers::pi, 1e-6);
	// After a full turn it starts again, so a long time keeps its precision
	EXPECT_NEAR(graphics::light_shine::TextureTurn(25000ms), graphics::light_shine::TextureTurn(5000ms), 1e-5);
	EXPECT_NEAR(graphics::light_shine::TextureTurn(3600000ms + 5000ms), graphics::light_shine::TextureTurn(5000ms), 1e-3);
}

TEST(LightShine, TheTextureIsMirroredTopToBottomBeforeItTurns)
{
	// Not turned: the top comes to the bottom, across stays
	const auto still = graphics::light_shine::TurnedUv({0.88f, 0.12f}, 1.0f, 0.0f);
	EXPECT_FLOAT_EQ(still.x, 0.88f);
	EXPECT_FLOAT_EQ(still.y, 0.88f);
	// A quarter turn the negative way: across goes to minus down
	const auto turned = graphics::light_shine::TurnedUv({1.0f, 0.5f}, 0.0f, -1.0f);
	EXPECT_FLOAT_EQ(turned.x, 0.5f);
	EXPECT_FLOAT_EQ(turned.y, 0.0f);
	// The middle never moves
	const auto middle = graphics::light_shine::TurnedUv({0.5f, 0.5f}, std::cos(1.0f), std::sin(1.0f));
	EXPECT_FLOAT_EQ(middle.x, 0.5f);
	EXPECT_FLOAT_EQ(middle.y, 0.5f);
}

TEST(LightShine, SolidOnlyOnTheSecondGroupsBone)
{
	EXPECT_FLOAT_EQ(graphics::light_shine::VertexAlpha(0, 0), 1.0f);
	EXPECT_FLOAT_EQ(graphics::light_shine::VertexAlpha(3, 0), 0.0f);
}

TEST(LightShine, EveryOpenedChestShinesInTheSameBlue)
{
	EXPECT_EQ(ecs::reward::k_OpenedLightColour[0], 32);
	EXPECT_EQ(ecs::reward::k_OpenedLightColour[1], 61);
	EXPECT_EQ(ecs::reward::k_OpenedLightColour[2], 232);
}
