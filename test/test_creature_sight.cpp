/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>
#include <numbers>

#include <Common/GameHeading.h>
#include <Common/X87Vector.h>
#include <Creature/CreatureFriendAgendas.h>
#include <ECS/Components/CreatureBody.h>
#include <ECS/Components/CreatureLocomotion.h>
#include <ECS/Components/Transform.h>
#include <ECS/CreatureSight.h>
#include <ECS/Registry.h>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

using namespace openblack;
using namespace openblack::ecs::components;
namespace sight = openblack::ecs::creature_sight;

namespace
{
/// The heading that faces +x, the game angle 0
const GameHeading k_FacesPlusX {std::numbers::pi_v<float> / 2.0f};

/// A creature at a point, with its walk started at a heading
entt::entity CreatureAt(ecs::Registry& registry, glm::vec3 position, GameHeading heading)
{
	const auto creature = registry.Create();
	registry.Assign<Transform>(creature,
	                           Transform {.position = position, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
	auto& walk = registry.Assign<CreatureLocomotion>(creature);
	walk.started = true;
	walk.heading = heading;
	registry.Assign<CreatureAnimation>(creature);
	return creature;
}
} // namespace

// With its head turned to nothing, a creature looks along the heading of its body, not the way its model is drawn
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureSight, ItLooksAlongTheHeadingOfItsBody)
{
	ecs::Registry registry;
	const auto creature = CreatureAt(registry, {5.0f, 0.0f, 5.0f}, k_FacesPlusX);
	EXPECT_EQ(sight::FacingHeading(registry, creature), k_FacesPlusX);
	EXPECT_EQ(sight::LookHeading(registry, creature), k_FacesPlusX);
	EXPECT_EQ(sight::LookHeading(registry, creature).GameAngle(), 0);
	// A head that looks at the position of the creature looks along its body too
	registry.Get<CreatureAnimation>(creature).lookAt = glm::vec3(5.0f, 0.0f, 5.0f);
	EXPECT_EQ(sight::LookHeading(registry, creature), k_FacesPlusX);
}

// A creature looks towards the point that its head looks at, across the land
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureSight, ItLooksTowardsThePointOfItsHead)
{
	ecs::Registry registry;
	const auto creature = CreatureAt(registry, {5.0f, 0.0f, 5.0f}, k_FacesPlusX);
	registry.Get<CreatureAnimation>(creature).lookAt = glm::vec3(5.0f, 3.0f, -20.0f);
	EXPECT_EQ(sight::LookHeading(registry, creature).GameAngle(), 0x600);
	// A point straight above it has no way across the land: the heading 0, which looks along -z
	registry.Get<CreatureAnimation>(creature).lookAt = glm::vec3(5.0f, 10.0f, 5.0f);
	EXPECT_EQ(sight::LookHeading(registry, creature), GameHeading(0.0f));
}

// Before its walk starts, a creature faces as its model does
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureSight, BeforeItsWalkItFacesAsItsModel)
{
	ecs::Registry registry;
	const auto creature = CreatureAt(registry, {5.0f, 0.0f, 5.0f}, k_FacesPlusX);
	registry.Get<CreatureLocomotion>(creature).started = false;
	EXPECT_EQ(sight::FacingHeading(registry, creature).GameAngle(), 0x600);
}

// The game angle of a way on the land: +x is 0, +z a quarter turn, -x a half turn, -z three quarters. The game keeps the
// heading as a float and truncates it to a game angle. No way at all is the heading 0, three quarters round.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureSight, TheWayToAPointGivesTheGamesAngle)
{
	EXPECT_EQ(GameHeading::Of(1.0f, 0.0f).GameAngle(), 0);
	EXPECT_EQ(GameHeading::Of(0.0f, 1.0f).GameAngle(), 0x200);
	EXPECT_EQ(GameHeading::Of(-1.0f, 0.0f).GameAngle(), 0x400);
	EXPECT_EQ(GameHeading::Of(0.0f, -1.0f).GameAngle(), 0x600);
	EXPECT_EQ(GameHeading::Of(1.0f, 1.0f).GameAngle(), 0xff);
	EXPECT_EQ(GameHeading::Of(-1.0f, -1.0f).GameAngle(), 0x500);
	EXPECT_EQ(GameHeading::Of(0.0f, 0.0f).GameAngle(), 0x600);
}

// A creature sees a point within two thirds of a half turn of the way it looks, or in its own map cell
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureSight, ItSeesAheadOrInItsOwnCell)
{
	ecs::Registry registry;
	const auto creature = CreatureAt(registry, {5.0f, 0.0f, 5.0f}, k_FacesPlusX);
	EXPECT_TRUE(sight::CanSeePos(registry, creature, {100.0f, 5.0f}));
	EXPECT_TRUE(sight::CanSeePos(registry, creature, {5.0f, 100.0f}));
	EXPECT_FALSE(sight::CanSeePos(registry, creature, {-100.0f, 5.0f}));
	// Behind it, but in its own map cell
	EXPECT_TRUE(sight::CanSeePos(registry, creature, {1.0f, 5.0f}));
	// Its head turns away from the point
	registry.Get<CreatureAnimation>(creature).lookAt = glm::vec3(-50.0f, 0.0f, 5.0f);
	EXPECT_FALSE(sight::CanSeePos(registry, creature, {100.0f, 5.0f}));
}

// A creature sees an object nearer than ten metres, or nearer than fifty times the height of the object. A distance
// that is not a number counts as near.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureSight, ItSeesAnObjectByItsDistanceAndHeight)
{
	EXPECT_TRUE(creature_mind::CanSee(9.99f, 0.0f));
	EXPECT_FALSE(creature_mind::CanSee(10.0f, 0.0f));
	EXPECT_TRUE(creature_mind::CanSee(49.0f, 1.0f));
	EXPECT_FALSE(creature_mind::CanSee(50.0f, 1.0f));
	EXPECT_TRUE(creature_mind::CanSee(std::numeric_limits<float>::quiet_NaN(), 0.0f));
	EXPECT_FALSE(creature_mind::CanSee(500.0f, std::numeric_limits<float>::quiet_NaN()));
}

// The distance goes to the map position of the object, and an object off the map is not seen
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(CreatureSight, ItSeesAnObjectAtItsMapPosition)
{
	const glm::vec3 creature {0.0001f, 0.0f, 0.0f};
	const glm::vec3 object {10.00012f, 0.0f, 0.0f};
	// The point itself is just over ten metres away, but its map position is just inside
	EXPECT_FALSE(creature_mind::CanSee(x87::Distance<x87::SumOrder::Zyx>(object, creature), 0.0f));
	EXPECT_TRUE(creature_mind::CanSeeObject(creature, object, 0.0f));
	// One metre away, but off the map
	EXPECT_FALSE(creature_mind::CanSeeObject({0.5f, 0.0f, 0.5f}, {-0.5f, 0.0f, 0.5f}, 0.0f));
	EXPECT_TRUE(creature_mind::CanSeeObject({1.5f, 0.0f, 0.5f}, {0.5f, 0.0f, 0.5f}, 0.0f));
}
