/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "ECS/Registry.h"
#include "Graphics/PaintedSkinStamp.h"

using namespace openblack;
using namespace openblack::graphics::painted_skin;

TEST(PaintedSkinStamp, TexturesWithNoPaintingTakeTheSkin)
{
	EXPECT_TRUE(NeedsUpload(std::nullopt, {.registryResets = 0, .revision = 0}));
}

TEST(PaintedSkinStamp, TheSamePaintingIsNotTakenAgain)
{
	const Stamp held {.registryResets = 2, .revision = 5};
	EXPECT_FALSE(NeedsUpload(held, {.registryResets = 2, .revision = 5}));
}

TEST(PaintedSkinStamp, ANewPaintingIsTaken)
{
	const Stamp held {.registryResets = 2, .revision = 5};
	EXPECT_TRUE(NeedsUpload(held, {.registryResets = 2, .revision = 6}));
}

// A loaded world can give the same entity the same revision as the world before it, with a different painting
TEST(PaintedSkinStamp, TheSameRevisionAfterALoadIsTaken)
{
	const Stamp held {.registryResets = 2, .revision = 5};
	EXPECT_TRUE(NeedsUpload(held, {.registryResets = 3, .revision = 5}));
}

TEST(PaintedSkinStamp, EachResetOfTheRegistryCounts)
{
	ecs::Registry registry;
	const auto before = registry.GetResets();
	registry.Reset();
	EXPECT_EQ(registry.GetResets(), before + 1);
	registry.Reset();
	EXPECT_EQ(registry.GetResets(), before + 2);
}
