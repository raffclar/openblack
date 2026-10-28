/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>

#include <gtest/gtest.h>

#include "ECS/Components/InterfaceLimits.h"
#include "ECS/Registry.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"

using namespace openblack;
using namespace openblack::script::interface_levels;
using openblack::ecs::components::InterfaceLimits;

namespace
{
constexpr InterfaceLimits k_None {};
constexpr InterfaceLimits k_Both {.onlyHighlights = true, .noHolding = true};
constexpr InterfaceLimits k_OnlyHolding {.onlyHighlights = false, .noHolding = true};

bool Same(const InterfaceLimits& a, const InterfaceLimits& b)
{
	return a.onlyHighlights == b.onlyHighlights && a.noHolding == b.noHolding;
}
} // namespace

TEST(ScriptInterfaceLevels, TheLessonLevelsSetBothLimits)
{
	for (const int32_t level : std::array {1, 2, 3, 4, 5, 8, 10, 11, 13, 14, 15})
	{
		EXPECT_TRUE(Same(AfterLevel(k_None, level), k_Both)) << "level " << level;
	}
}

TEST(ScriptInterfaceLevels, TheNormalAndInteractLevelsRemoveTheLimits)
{
	for (const int32_t level : std::array {0, 6, 7})
	{
		EXPECT_TRUE(Same(AfterLevel(k_Both, level), k_None)) << "level " << level;
	}
}

TEST(ScriptInterfaceLevels, TheHandInteractionLevelAddsOnlyTheLimitOnHolding)
{
	EXPECT_TRUE(Same(AfterLevel(k_None, 12), k_OnlyHolding));
	// It does not remove the limit to highlights that an earlier level set
	EXPECT_TRUE(Same(AfterLevel(k_Both, 12), k_Both));
}

TEST(ScriptInterfaceLevels, AnUnknownLevelKeepsTheLimits)
{
	for (const int32_t level : std::array {-1, 9, 16, 100})
	{
		EXPECT_TRUE(Same(AfterLevel(k_Both, level), k_Both)) << "level " << level;
		EXPECT_TRUE(Same(AfterLevel(k_None, level), k_None)) << "level " << level;
	}
}

TEST(ScriptInterfaceLevels, TheLimitsStopHoldingAndPressesOffHighlights)
{
	EXPECT_TRUE(MayHold(k_None));
	EXPECT_FALSE(MayHold(k_OnlyHolding));
	EXPECT_TRUE(MayPress(k_None, false));
	EXPECT_TRUE(MayPress(k_OnlyHolding, false));
	EXPECT_FALSE(MayPress(k_Both, false));
	EXPECT_TRUE(MayPress(k_Both, true));
}

TEST(ScriptInterfaceLevels, ApplyGivesThePlayerTheLimits)
{
	ecs::Registry registry;
	const auto player = registry.Create();
	Apply(registry, player, 1);
	ASSERT_NE(registry.TryGet<const InterfaceLimits>(player), nullptr);
	EXPECT_TRUE(Same(registry.Get<const InterfaceLimits>(player), k_Both));
	Apply(registry, player, 0);
	EXPECT_TRUE(Same(registry.Get<const InterfaceLimits>(player), k_None));
	// No player: nothing changes
	Apply(registry, entt::null, 1);
}

TEST(ScriptInterfaceLevels, APlayerNotInTheGameHasNoLimits)
{
	ecs::Registry registry;
	EXPECT_TRUE(Same(LimitsOf(registry, std::nullopt), k_None));
	const auto player = registry.Create();
	EXPECT_TRUE(Same(LimitsOf(registry, player), k_None));
	Apply(registry, player, 1);
	EXPECT_TRUE(Same(LimitsOf(registry, player), k_Both));
	registry.Destroy(player);
	EXPECT_TRUE(Same(LimitsOf(registry, player), k_None));
}
