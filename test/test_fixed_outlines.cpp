/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/FixedOutlines.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/FixedOutlineSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::fixed_outlines;

namespace
{
/// The rows of a thing that is not turned, at a scale
Rows Unturned(float scale)
{
	return {glm::vec3(scale, 0.0f, 0.0f), glm::vec3(0.0f, scale, 0.0f), glm::vec3(0.0f, 0.0f, scale)};
}
} // namespace

TEST(FixedOutlines, RoundBoxIsOneCircleOfTheLongSide)
{
	const auto outline = OfModelBox(Unturned(1.0f), {100.0f, 200.0f}, 0.0f, 1.0f, {1.0f, 5.0f, -2.0f}, {3.0f, 4.0f, 2.5f});
	EXPECT_TRUE(outline.parts.empty());
	EXPECT_EQ(outline.bounds.centre, glm::vec2(101.0f, 198.0f));
	EXPECT_EQ(outline.bounds.radius, 3.0f);
	EXPECT_EQ(outline.bounds.radiusSquared, 9.0f);
}

TEST(FixedOutlines, SmallSidesCountAsAUnit)
{
	const auto outline = OfModelBox(Unturned(0.5f), {0.0f, 0.0f}, 0.0f, 0.5f, glm::vec3(0.0f), {0.4f, 1.0f, 1.2f});
	EXPECT_TRUE(outline.parts.empty());
	EXPECT_EQ(outline.bounds.radius, 1.0f);
}

TEST(FixedOutlines, LongBoxIsARowOfCircles)
{
	// Seven by two: three whole times and one more, each a circle of the short half side
	const auto outline = OfModelBox(Unturned(1.0f), {50.0f, 50.0f}, 0.0f, 1.0f, glm::vec3(0.0f), {7.0f, 1.0f, 2.0f});
	EXPECT_EQ(outline.bounds.radiusSquared, 53.0f);
	EXPECT_EQ(outline.bounds.radius, std::sqrt(53.0f));
	ASSERT_EQ(outline.parts.size(), 4u);
	EXPECT_EQ(outline.parts.front().centre, glm::vec2(50.0f - 5.25f, 50.0f));
	EXPECT_EQ(outline.parts.back().centre, glm::vec2(50.0f + 5.25f, 50.0f));
	EXPECT_EQ(outline.parts.front().radius, 2.0f);
}

TEST(FixedOutlines, ProbeMeetsWhenItTouches)
{
	const auto trunk = OfTrunk({10.0f, 10.0f});
	EXPECT_TRUE(Collides(trunk, Probe({10.75f, 10.0f})));
	EXPECT_TRUE(Collides(trunk, Probe({10.0f, 10.79f})));
	EXPECT_FALSE(Collides(trunk, Probe({10.0f, 10.81f})));
}

TEST(FixedOutlines, LongBoxNeedsAPartToo)
{
	const auto outline = OfModelBox(Unturned(1.0f), {0.0f, 0.0f}, 0.0f, 1.0f, glm::vec3(0.0f), {7.0f, 1.0f, 1.0f});
	// In the bounding circle, but away from the row along x
	EXPECT_FALSE(Collides(outline, Probe({0.0f, 5.0f})));
	EXPECT_TRUE(Collides(outline, Probe({6.0f, 1.2f})));
}

TEST(FixedOutlines, TempleHasThirtyThreeCircles)
{
	const auto ring = TempleRing({500.0f, 500.0f}, 0.0f);
	ASSERT_EQ(ring.size(), 33u);
	EXPECT_EQ(ring.front().centre, glm::vec2(500.0f, 500.0f));
	EXPECT_EQ(ring.front().radius, 21.5f * 0.7f);
}

TEST(FixedOutlines, OkToCreateOnFreeLandOrWater)
{
	EXPECT_TRUE(OkToCreate(false, false));
	EXPECT_TRUE(OkToCreate(false, true));
	EXPECT_TRUE(OkToCreate(true, true));
	EXPECT_FALSE(OkToCreate(true, false));
}

TEST(FixedOutlines, TreeHasItsTrunkAtItsMapPlace)
{
	Registry registry;
	const auto tree = registry.Create();
	registry.Assign<components::Transform>(tree, glm::vec3(123.456f, 0.0f, 78.9f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::Tree>(tree);
	const auto outline = systems::FixedOutlineSystem::OutlineOf(registry, tree);
	ASSERT_TRUE(outline.has_value());
	EXPECT_EQ(outline->bounds.centre, glm::vec2(map_coords::Quantise(123.456f), map_coords::Quantise(78.9f)));
	EXPECT_EQ(outline->bounds.radius, k_TrunkRadius);
}

TEST(FixedOutlines, PilesAndModelsNotLoadedHaveNone)
{
	Registry registry;
	const auto pot = registry.Create();
	registry.Assign<components::Transform>(pot, glm::vec3(10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::Pot>(pot);
	EXPECT_FALSE(systems::FixedOutlineSystem::OutlineOf(registry, pot).has_value());
	const auto feature = registry.Create();
	registry.Assign<components::Transform>(feature, glm::vec3(10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::Feature>(feature);
	EXPECT_FALSE(systems::FixedOutlineSystem::OutlineOf(registry, feature).has_value());
}
