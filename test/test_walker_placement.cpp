/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/WalkerPlacement.h"
#include "ECS/WallHugRules.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{

constexpr glm::vec3 k_Old {100.0f, 5.0f, 100.0f};
constexpr glm::vec3 k_New {300.0f, 7.0f, 250.0f};
constexpr glm::vec2 k_Goal {150.0f, 120.0f};

/// A walker at the old place, heading for the goal and part way round a circle in its way
template <typename Tag>
entt::entity Walker(ecs::Registry& registry)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, k_Old, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& wallHug = registry.Assign<WallHug>(entity);
	wallHug.goal = k_Goal;
	wallHug.step = {120, -40};
	wallHug.position = ecs::wall_hug::ToWhole({k_Old.x, k_Old.z});
	wallHug.placedAt = {k_Old.x, k_Old.z};
	wallHug.turnsUntilStepRebuild = 5;
	registry.Assign<Tag>(entity, MoveStateClockwise::Clockwise, glm::vec2(k_Old.x, k_Old.z));
	registry.Assign<WallHugObjectReference>(entity, uint8_t {3}, entt::entity {entt::null}, glm::vec2(110.0f, 105.0f), 8.0f,
	                                        40u);
	return entity;
}

void ExpectHeldAtTheNewPlace(const ecs::Registry& registry, entt::entity entity)
{
	EXPECT_EQ(registry.Get<const Transform>(entity).position, k_New);
	const auto& wallHug = registry.Get<const WallHug>(entity);
	EXPECT_EQ(wallHug.position, ecs::wall_hug::ToWhole({k_New.x, k_New.z}));
	EXPECT_EQ(wallHug.placedAt, glm::vec2(k_New.x, k_New.z));
	EXPECT_EQ(wallHug.step, glm::ivec2(0, 0));
	EXPECT_FALSE(registry.AllOf<WallHugObjectReference>(entity));
}

} // namespace

// Moved part way round a circle, a walker drops the circle and sets off for the same goal from where it is put
TEST(WalkerPlacement, AWalkerOnItsWayHeadsForItsGoalFromTheNewPlace)
{
	ecs::Registry registry;
	const auto walker = Walker<MoveStateOrbitTag>(registry);
	ecs::walker_placement::Place(registry, walker, k_New);
	ExpectHeldAtTheNewPlace(registry, walker);
	EXPECT_EQ(registry.Get<const WallHug>(walker).goal, k_Goal);
	EXPECT_FALSE(registry.AllOf<MoveStateOrbitTag>(walker));
	ASSERT_TRUE(registry.AllOf<MoveStateLinearTag>(walker));
	EXPECT_EQ(registry.Get<const MoveStateLinearTag>(walker).clockwise, MoveStateClockwise::Undefined);
}

// About to step onto its goal, it would otherwise jump back there on its next turn
TEST(WalkerPlacement, AWalkerAboutToArriveWalksThereInstead)
{
	ecs::Registry registry;
	const auto walker = Walker<MoveStateFinalStepTag>(registry);
	ecs::walker_placement::Place(registry, walker, k_New);
	ExpectHeldAtTheNewPlace(registry, walker);
	EXPECT_FALSE(registry.AllOf<MoveStateFinalStepTag>(walker));
	EXPECT_TRUE(registry.AllOf<MoveStateLinearTag>(walker));
}

// One that had arrived would otherwise walk back to where it stood: it stays where it is put
TEST(WalkerPlacement, AWalkerThatHadArrivedStaysWhereItIsPut)
{
	ecs::Registry registry;
	const auto walker = Walker<MoveStateArrivedTag>(registry);
	ecs::walker_placement::Place(registry, walker, k_New);
	ExpectHeldAtTheNewPlace(registry, walker);
	EXPECT_EQ(registry.Get<const WallHug>(walker).goal, glm::vec2(k_New.x, k_New.z));
	EXPECT_TRUE(registry.AllOf<MoveStateArrivedTag>(walker));
	EXPECT_FALSE(registry.AllOf<MoveStateLinearTag>(walker));
}

// A walker that isn't walking only takes up its new place, and a thing that doesn't walk only moves
TEST(WalkerPlacement, AnythingElseOnlyMoves)
{
	ecs::Registry registry;
	const auto standing = registry.Create();
	registry.Assign<Transform>(standing, k_Old, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& wallHug = registry.Assign<WallHug>(standing);
	wallHug.goal = k_Goal;
	ecs::walker_placement::Place(registry, standing, k_New);
	EXPECT_EQ(registry.Get<const Transform>(standing).position, k_New);
	EXPECT_EQ(registry.Get<const WallHug>(standing).placedAt, glm::vec2(k_New.x, k_New.z));
	EXPECT_EQ(registry.Get<const WallHug>(standing).goal, k_Goal);
	EXPECT_FALSE(registry.AllOf<MoveStateLinearTag>(standing));

	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, k_Old, glm::mat3(1.0f), glm::vec3(1.0f));
	ecs::walker_placement::Place(registry, rock, k_New);
	EXPECT_EQ(registry.Get<const Transform>(rock).position, k_New);
	EXPECT_FALSE(registry.AllOf<WallHug>(rock));

	// Nothing to place
	ecs::walker_placement::Place(registry, registry.Create(), k_New);
}

// A walker stopped where it stands no longer heads for its goal, whichever way it was going
TEST(WalkerPlacement, AStoppedWalkerStaysWhereItStands)
{
	ecs::Registry registry;
	const auto walker = Walker<MoveStateOrbitTag>(registry);
	ecs::walker_placement::Stop(registry, walker);
	EXPECT_EQ(registry.Get<const Transform>(walker).position, k_Old);
	EXPECT_FALSE((registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                             MoveStateFinalStepTag, MoveStateArrivedTag>(walker)));
	EXPECT_FALSE(registry.AllOf<WallHugObjectReference>(walker));
	EXPECT_EQ(registry.Get<const WallHug>(walker).step, glm::ivec2(0, 0));
}

// Put somewhere else and then stopped, as a script's SET_POSITION does to a villager it controls, it stays at the new
// place rather than walking on from there or back to its goal
TEST(WalkerPlacement, PlacedThenStoppedItStaysAtTheNewPlace)
{
	ecs::Registry registry;
	const auto walker = Walker<MoveStateFinalStepTag>(registry);
	ecs::walker_placement::Place(registry, walker, k_New);
	ecs::walker_placement::Stop(registry, walker);
	ExpectHeldAtTheNewPlace(registry, walker);
	EXPECT_FALSE((registry.AnyOf<MoveStateLinearTag, MoveStateFinalStepTag, MoveStateArrivedTag>(walker)));
}

// Something that doesn't walk is left as it is
TEST(WalkerPlacement, StoppingAThingThatDoesntWalkDoesNothing)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, k_Old, glm::mat3(1.0f), glm::vec3(1.0f));
	ecs::walker_placement::Stop(registry, thing);
	EXPECT_EQ(registry.Get<const Transform>(thing).position, k_Old);
}

// A villager put somewhere at once is drawn there at once, not gliding from where it was through the turn
TEST(WalkerPlacement, AVillagerPutSomewhereIsDrawnThereAtOnce)
{
	ecs::Registry registry;
	const auto walker = Walker<MoveStateLinearTag>(registry);
	registry.Assign<VillagerPose>(walker).turnStart = k_Old;
	ecs::walker_placement::Place(registry, walker, k_New);
	EXPECT_EQ(registry.Get<const VillagerPose>(walker).turnStart, k_New);
}
