/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <optional>

#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS

#include "Creature/TemplePen.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CreaturePenSystem.h"

using namespace openblack;
using namespace openblack::temple_pen;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureLeash;
using openblack::ecs::components::Temple;
using openblack::ecs::components::Transform;
using openblack::ecs::systems::CreaturePenSystem;

namespace
{
/// A point (x, z) turned about the heart as a temple turned by the angle turns its mesh
glm::vec2 Turned(glm::vec2 point, float angle)
{
	const auto turned = glm::mat3(glm::eulerAngleY(-angle)) * glm::vec3(point.x, 0.0f, point.y);
	return {turned.x, turned.z};
}
} // namespace

TEST(TemplePen, ShownAtItsOwnSizeOutOfReach)
{
	EXPECT_EQ(ShownSize(2.0f, 16.5f, true), 2.0f);
	EXPECT_EQ(ShownSize(2.0f, 100.0f, true), 2.0f);
}

TEST(TemplePen, ShownAtItsOwnSizeOutsideTheWalls)
{
	EXPECT_EQ(ShownSize(2.0f, 0.0f, false), 2.0f);
	EXPECT_EQ(ShownSize(2.0f, 15.0f, false), 2.0f);
}

TEST(TemplePen, ShrinksToThePensSizeWithinTheInnerRadius)
{
	EXPECT_EQ(ShownSize(2.0f, 14.0f, true), k_PenSize);
	EXPECT_EQ(ShownSize(2.0f, 3.0f, true), k_PenSize);
	EXPECT_EQ(ShownSize(2.0f, 0.0f, true), k_PenSize);
}

TEST(TemplePen, EasesBetweenTheRadii)
{
	EXPECT_EQ(ShownSize(2.0f, 16.0f, true), 2.0f);
	EXPECT_NEAR(ShownSize(2.0f, 15.0f, true), 1.11f, 1e-6f);
	EXPECT_NEAR(ShownSize(2.0f, 14.5f, true), 0.665f, 1e-6f);
	// A creature smaller than the pen's size grows to it
	EXPECT_EQ(ShownSize(0.1f, 14.0f, true), k_PenSize);
}

TEST(TemplePen, WallsFanOutFromTheHeart)
{
	const glm::vec2 heart(100.0f, 200.0f);
	EXPECT_TRUE(BetweenWalls(heart, 0.0f, heart + glm::vec2(-1.0f, -2.0f)));
	EXPECT_TRUE(BetweenWalls(heart, 0.0f, heart + glm::vec2(-10.0f, -20.0f)));
	EXPECT_FALSE(BetweenWalls(heart, 0.0f, heart + glm::vec2(1.0f, 0.0f)));
	EXPECT_FALSE(BetweenWalls(heart, 0.0f, heart + glm::vec2(0.0f, 1.0f)));
	EXPECT_FALSE(BetweenWalls(heart, 0.0f, heart + glm::vec2(1.0f, 2.0f)));
	// The heart itself is on both walls
	EXPECT_TRUE(BetweenWalls(heart, 0.0f, heart));
}

TEST(TemplePen, WallsTurnWithTheTemple)
{
	const glm::vec2 heart(0.0f, 0.0f);
	for (const float angle : {0.5f, 1.5f, 3.0f, -2.0f})
	{
		EXPECT_TRUE(BetweenWalls(heart, angle, Turned({-1.0f, -2.0f}, angle))) << angle;
		EXPECT_FALSE(BetweenWalls(heart, angle, Turned({1.0f, 0.0f}, angle))) << angle;
	}
}

TEST(TemplePen, MapPlaceTruncatesTowardsZero)
{
	// -4 m is -26214.4 units, kept as -26214
	EXPECT_FLOAT_EQ(MapPlace({-4.0f, 0.0f, 4.0f}).x, -26214.0f * 10.0f / 65536.0f);
	EXPECT_FLOAT_EQ(MapPlace({-4.0f, 0.0f, 4.0f}).y, 26214.0f * 10.0f / 65536.0f);
}

namespace
{
class CreaturePenSystemTest: public ::testing::Test
{
protected:
	static constexpr glm::vec2 k_Pen {-4.0f, -8.0f};
	static constexpr float k_Ground = 3.0f;

	CreaturePenSystemTest()
	    : _system(CreaturePenSystem::World {
	          .penPlace = [](entt::entity) -> std::optional<glm::vec2> { return k_Pen; },
	          .groundAt = [](glm::vec2) { return k_Ground; },
	          .drawnScale = [](CreatureType, float size) { return size * 10.0f; },
	      })
	{
		const auto temple = _registry.Create();
		_registry.Assign<Transform>(temple, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		_registry.Assign<Temple>(temple, Temple {.owner = PlayerNames::PLAYER_ONE, .yAngle = 0.0f});
	}

	entt::entity MakeCreature(PlayerNames owner, glm::vec3 position)
	{
		const auto creature = _registry.Create();
		_registry.Assign<Transform>(creature, position, glm::mat3(1.0f), glm::vec3(1.0f));
		_registry.Assign<Creature>(creature, Creature {.owner = owner, .species = CreatureType::Tiger, .size = 2.0f});
		return creature;
	}

	ecs::Registry _registry;
	CreaturePenSystem _system;
};
} // namespace

TEST_F(CreaturePenSystemTest, HomeFollowsTheTemplesPen)
{
	const auto creature = MakeCreature(PlayerNames::PLAYER_ONE, {50.0f, 0.0f, 50.0f});
	_system.ProcessTurn(_registry);
	const auto* leash = _registry.TryGet<const CreatureLeash>(creature);
	ASSERT_NE(leash, nullptr);
	ASSERT_TRUE(leash->home.has_value());
	EXPECT_EQ(*leash->home, glm::vec3(k_Pen.x, k_Ground, k_Pen.y));
}

TEST_F(CreaturePenSystemTest, ShownAsANewbornInThePen)
{
	const auto creature = MakeCreature(PlayerNames::PLAYER_ONE, {k_Pen.x, 0.0f, k_Pen.y});
	_system.ProcessTurn(_registry);
	const auto& body = _registry.Get<const Creature>(creature);
	ASSERT_TRUE(body.penSize.has_value());
	EXPECT_EQ(*body.penSize, k_PenSize);
	EXPECT_EQ(body.size, 2.0f);
	// Whatever goes by how big it looks takes the pen's size; its own size is kept
	EXPECT_EQ(ShownSize(body), k_PenSize);
	EXPECT_FLOAT_EQ(_registry.Get<const Transform>(creature).scale.x, k_PenSize * 10.0f);
}

TEST_F(CreaturePenSystemTest, GrowsBackWalkingOut)
{
	// Further out along the line from the heart through the pen, 15 m from it
	const auto out = k_Pen + glm::vec2(-1.0f, -2.0f) * (15.0f / std::sqrt(5.0f));
	const auto creature = MakeCreature(PlayerNames::PLAYER_ONE, {out.x, 0.0f, out.y});
	_system.ProcessTurn(_registry);
	ASSERT_TRUE(_registry.Get<const Creature>(creature).penSize.has_value());
	EXPECT_NEAR(*_registry.Get<const Creature>(creature).penSize, 1.11f, 0.01f);

	auto& transform = _registry.Get<Transform>(creature);
	transform.position += glm::vec3(-5.0f, 0.0f, -10.0f);
	_system.ProcessTurn(_registry);
	EXPECT_FALSE(_registry.Get<const Creature>(creature).penSize.has_value());
	EXPECT_EQ(ShownSize(_registry.Get<const Creature>(creature)), 2.0f);
	EXPECT_FLOAT_EQ(transform.scale.x, 20.0f);
}

TEST_F(CreaturePenSystemTest, NotShrunkOutsideTheWalls)
{
	// Near the pen but on the far side of the heart
	const auto creature = MakeCreature(PlayerNames::PLAYER_ONE, {4.0f, 0.0f, 0.0f});
	_system.ProcessTurn(_registry);
	EXPECT_FALSE(_registry.Get<const Creature>(creature).penSize.has_value());
}

TEST_F(CreaturePenSystemTest, AnotherPlayersTempleIsNotItsPen)
{
	const auto creature = MakeCreature(PlayerNames::PLAYER_TWO, {k_Pen.x, 0.0f, k_Pen.y});
	_system.ProcessTurn(_registry);
	EXPECT_EQ(_registry.TryGet<const CreatureLeash>(creature), nullptr);
	EXPECT_FALSE(_registry.Get<const Creature>(creature).penSize.has_value());
	EXPECT_FLOAT_EQ(_registry.Get<const Transform>(creature).scale.x, 20.0f);
}
