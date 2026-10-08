/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature locomotion system on our route follower and walkable mask, with a synthetic Giant Ape on an open land:
// what it refuses, a walk planned, followed and ended, stopping and turning, running away on the synced stream, the
// Transform moving only once a turn and the body drawn between turns, and the jumps back onto the land it can stand on

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <numbers>
#include <utility>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/sparse_set.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureLocomotion.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CreatureLocomotionSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureLocomotionSystem;
using MoveResult = openblack::ecs::systems::CreatureLocomotionSystemInterface::MoveResult;
using Pace = openblack::ecs::systems::CreatureLocomotionSystemInterface::Pace;
using Motion = CreatureLocomotion::Motion;

namespace
{
/// 64 x 64 cells of land, 640 m a side
constexpr int32_t k_MaskSize = 64;

class CreatureLocomotionSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		auto& state = Locator::landAvoidSystem::value().GetState();
		state.size = k_MaskSize;
		state.avoid.assign(static_cast<size_t>(k_MaskSize * k_MaskSize), land_avoid::k_Land);
		// the ape's speeds
		auto& ape = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
		ape.slowSpeed = 0.5f;
		ape.walkSpeed = 0.7f;
		ape.runSpeed = 0.9f;
		ape.runAwayDistance = 50.0f;
	}

	/// The ape's stand, walk and run, the walk and run moving it along
	void LoadWalkingRig()
	{
		test::creature_block::Block block;
		block.clips = {test::creature_block::Clip {.durationMs = 1000},
		               test::creature_block::Clip {.durationMs = 1000, .displacement = {0.0f, 0.0f, -20.0f}},
		               test::creature_block::Clip {.durationMs = 600, .displacement = {0.0f, 0.0f, -25.0f}}};
		_world.LoadApeRig(block, {{"move", {"Cstand", "Wwalk", "Wrun"}}});
	}

	static void Avoid(int32_t x, int32_t z)
	{
		Locator::landAvoidSystem::value().GetState().avoid[static_cast<size_t>(z * k_MaskSize + x)] = land_avoid::k_Avoid;
	}

	static CreatureLocomotion& Of(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureLocomotion>(creature);
	}
	static const Transform& TransformOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<Transform>(creature);
	}

	/// A walking ape at (100, 100), started on the first turn
	entt::entity StartedApe()
	{
		LoadWalkingRig();
		const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
		_system.ProcessTurn();
		return creature;
	}

	/// Turns until the creature stands again, at most `turns`; how many it took
	int RunUntilStanding(entt::entity creature, int turns)
	{
		for (int turn = 1; turn <= turns; ++turn)
		{
			_system.ProcessTurn();
			if (Of(creature).motion == Motion::Standing)
			{
				return turn;
			}
		}
		return -1;
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	CreatureLocomotionSystem _system;
};
} // namespace

TEST_F(CreatureLocomotionSystemTest, BusyWithoutWalkAndRunAnimations)
{
	const auto creature = test::creature_world::World::MakeCreature();
	// not started before its first turn
	EXPECT_EQ(_system.MoveTo(creature, {200.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Busy);
	_system.ProcessTurn();
	EXPECT_TRUE(Of(creature).started);
	// no rig: nothing to walk with
	EXPECT_EQ(_system.MoveTo(creature, {200.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Busy);
	// what is no creature
	EXPECT_EQ(_system.MoveTo(test::creature_world::World::Registry().Create(), {200.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f),
	          MoveResult::Busy);
	EXPECT_FALSE(_system.IsMoving(creature));
}

TEST_F(CreatureLocomotionSystemTest, NowhereToStandIsAnInvalidDestination)
{
	const auto creature = StartedApe();
	Avoid(30, 10);
	EXPECT_FALSE(_system.IsValidPosition({305.0f, 105.0f}, land_avoid::k_CreatureRadius));
	EXPECT_EQ(_system.MoveTo(creature, {305.0f, 105.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::InvalidDestination);
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_EQ(Of(creature).follower, nullptr);
}

TEST_F(CreatureLocomotionSystemTest, AWalkIsPlannedFollowedAndEnded)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	EXPECT_EQ(Of(creature).motion, Motion::Planning);
	ASSERT_NE(Of(creature).follower, nullptr);
	EXPECT_EQ(Of(creature).follower->GetContext(), static_cast<int32_t>(entt::to_integral(creature)));
	EXPECT_TRUE(_system.IsMoving(creature));

	// planned within a few turns, then it turns to face the way and walks
	bool walked = false;
	for (int turn = 0; turn < 3000 && Of(creature).motion != Motion::Standing; ++turn)
	{
		_system.ProcessTurn();
		walked = walked || Of(creature).motion == Motion::Walking;
	}
	EXPECT_TRUE(walked);
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_FALSE(Of(creature).failed);
	EXPECT_FALSE(Of(creature).destination.has_value());
	EXPECT_LT(glm::distance(glm::xz(TransformOf(creature).position), glm::vec2(300.0f, 100.0f)), 6.0f);
	// it faces the way it went, along +x
	EXPECT_NEAR(ecs::creature_pose::ReadHeading(TransformOf(creature).rotation), creature_locomotion::HeadingOf({1.0f, 0.0f}),
	            1e-3f);
}

TEST_F(CreatureLocomotionSystemTest, StopStandsItWhereItIs)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Run, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 40; ++turn)
	{
		_system.ProcessTurn();
	}
	_system.Stop(creature);
	EXPECT_EQ(Of(creature).motion, Motion::Standing);
	EXPECT_FALSE(Of(creature).destination.has_value());
	EXPECT_FALSE(_system.IsMoving(creature));
	const auto where = TransformOf(creature).position;
	_system.ProcessTurn();
	EXPECT_EQ(TransformOf(creature).position, where);
}

TEST_F(CreatureLocomotionSystemTest, TurnToFaceTurnsOnTheSpot)
{
	const auto creature = StartedApe();
	const auto where = TransformOf(creature).position;
	// a quarter turn round: there are no spin animations, so the heading eases round over the stand's length
	ASSERT_TRUE(_system.TurnToFace(creature, {100.0f, 200.0f}));
	EXPECT_EQ(Of(creature).motion, Motion::Turning);
	EXPECT_GT(RunUntilStanding(creature, 100), 1);
	EXPECT_NEAR(Of(creature).heading, creature_locomotion::HeadingOf({0.0f, 1.0f}), 1e-4f);
	EXPECT_EQ(TransformOf(creature).position, where);
	// a point where it stands has no way to face
	EXPECT_FALSE(_system.TurnToFace(creature, glm::xz(where)));
}

TEST_F(CreatureLocomotionSystemTest, RunningAwayDrawsOnceFromTheSyncedStream)
{
	const auto creature = StartedApe();
	GameRandomSeeds before {};
	GameRandomSeeds after {};
	{
		const game_random::testing::ScopedState state;
		before = game_random::Current();
		ASSERT_EQ(_system.FleeFrom(creature, {90.0f, 100.0f}), MoveResult::Started);
		after = game_random::Current();
	}
	const game_random::testing::ScopedState reference;
	static_cast<void>(game_random::GameRand(40));
	EXPECT_EQ(game_random::Current().synced, after.synced);
	EXPECT_NE(after.synced, before.synced);
	EXPECT_EQ(after.local, before.local);
	// away from the threat, at least the species' distance
	ASSERT_TRUE(Of(creature).destination.has_value());
	EXPECT_GE(Of(creature).destination->x, 100.0f + 50.0f - 1e-3f);
}

TEST_F(CreatureLocomotionSystemTest, TheTransformMovesOnlyOnceATurn)
{
	const auto creature = StartedApe();
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	for (int turn = 0; turn < 3000 && Of(creature).motion != Motion::Walking; ++turn)
	{
		_system.ProcessTurn();
	}
	_system.ProcessTurn();
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	const auto& self = Of(creature);
	ASSERT_NE(self.fromPosition, self.toPosition);
	const auto transform = TransformOf(creature);
	EXPECT_EQ(transform.position, self.toPosition);

	// a frame half way through the turn draws it half way, and leaves the Transform as it is
	_system.Update(0.5f);
	EXPECT_EQ(TransformOf(creature).position, transform.position);
	EXPECT_EQ(TransformOf(creature).rotation, transform.rotation);
	const auto& pose = test::creature_world::World::Registry().Get<CreatureDrawPose>(creature);
	EXPECT_EQ(pose.position, self.fromPosition + ((self.toPosition - self.fromPosition) * 0.5f));
	const auto heading = self.fromHeading + (creature_locomotion::WrapAngle(self.toHeading - self.fromHeading) * 0.5f);
	EXPECT_EQ(pose.rotation, affine::AngleY(-heading));
	// and the body is drawn there, while its Transform stays where the turn put it
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto between = ecs::creature_pose::BetweenTurns(registry, creature);
	ASSERT_TRUE(between.has_value());
	EXPECT_EQ(between->position, pose.position);
	EXPECT_EQ(between->rotation, pose.rotation);
	EXPECT_NE(between->position, transform.position);

	// put somewhere outside its turn, it is drawn there at once
	ecs::NotifyTeleported(creature);
	const auto snapped = ecs::creature_pose::BetweenTurns(registry, creature);
	ASSERT_TRUE(snapped.has_value());
	EXPECT_EQ(snapped->position, TransformOf(creature).position);
	EXPECT_EQ(snapped->rotation, TransformOf(creature).rotation);
	_system.Update(0.5f);
	EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(registry, creature).position, TransformOf(creature).position);

	// the next turn moves it on
	_system.ProcessTurn();
	EXPECT_NE(TransformOf(creature).position, transform.position);
	EXPECT_EQ(TransformOf(creature).position, Of(creature).toPosition);
}

TEST(CreaturePose, HeadingFromTheFollowerIsTheWayItWent)
{
	const glm::vec2 from {10.0f, 20.0f};
	for (const auto way : {glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, -3.0f), glm::vec2(-2.0f, 2.0f), glm::vec2(0.5f, 4.0f)})
	{
		EXPECT_FLOAT_EQ(ecs::creature_pose::HeadingFromFollower(from, from + way, 9.0f), creature_locomotion::HeadingOf(way));
	}
	// standing still it keeps its heading
	EXPECT_FLOAT_EQ(ecs::creature_pose::HeadingFromFollower(from, from, 9.0f), 9.0f);
	// and the heading read back from the rotation a turn's end gives
	EXPECT_NEAR(ecs::creature_pose::ReadHeading(affine::AngleY(-1.25f)), 1.25f, 1e-6f);
}

TEST_F(CreatureLocomotionSystemTest, PutBackOnTheLandItCanStandOnIsAJump)
{
	const auto creature = StartedApe();
	_world.teleported.clear();
	// the land under it can no longer be stood on: setting off, it is put back where it can first (T4)
	Avoid(10, 10);
	ASSERT_EQ(_system.MoveTo(creature, {300.0f, 100.0f}, Pace::Walk, 0.0f, 5.0f), MoveResult::Started);
	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), creature);
	const auto snapped = glm::xz(TransformOf(creature).position);
	EXPECT_TRUE(_system.IsValidPosition(snapped, land_avoid::k_CreatureTurningRadius));
	EXPECT_EQ(Of(creature).fromPosition, Of(creature).toPosition);

	// walking, it strays onto land it can't stand on: it is put back and sets off again (T5)
	for (int turn = 0; turn < 3000 && Of(creature).motion != Motion::Walking; ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_EQ(Of(creature).motion, Motion::Walking);
	_world.teleported.clear();
	const auto at = glm::xz(Of(creature).toPosition);
	const auto cellX = static_cast<int32_t>(at.x * 0.1f);
	const auto cellZ = static_cast<int32_t>(at.y * 0.1f);
	// the cells ahead of it
	for (int32_t x = cellX + 1; x <= cellX + 3; ++x)
	{
		Avoid(x, cellZ);
	}
	for (int turn = 0; turn < 200 && _world.teleported.empty(); ++turn)
	{
		_system.ProcessTurn();
	}
	ASSERT_FALSE(_world.teleported.empty());
	EXPECT_EQ(_world.teleported.front(), creature);
	EXPECT_TRUE(Of(creature).destination.has_value());
}

namespace
{
/// A creature as its locomotion leaves it after a turn: its Transform where the turn ended, its drawn pose half way
struct PosedCreature
{
	ecs::Registry registry;
	entt::entity entity {registry.Create()};
	glm::vec3 at {100.0f, 2.0f, 50.0f};
	glm::vec3 drawnAt {95.0f, 2.0f, 50.0f};
	glm::mat3 drawnRotation {affine::AngleY(0.25f)};

	PosedCreature()
	{
		registry.Assign<Transform>(entity, at, glm::mat3(1.0f), glm::vec3(2.0f));
		auto& locomotion = registry.Assign<CreatureLocomotion>(entity);
		locomotion.started = true;
		locomotion.fromPosition = drawnAt;
		locomotion.toPosition = at;
		registry.Assign<CreatureDrawPose>(entity, CreatureDrawPose {.position = drawnAt, .rotation = drawnRotation});
	}
	[[nodiscard]] const ecs::Registry& Lookup() const { return registry; }
};

using StorageSizes = std::vector<std::pair<entt::id_type, size_t>>;

StorageSizes StorageSizesOf(const ecs::Registry& registry)
{
	StorageSizes sizes;
	registry.EachStorage(
	    [&sizes](entt::id_type id, const entt::sparse_set& storage) { sizes.emplace_back(id, storage.size()); });
	return sizes;
}
} // namespace

TEST(CreaturePose, BetweenTurnsIsTheDrawPoseWhileItsTurnHoldsTheTransform)
{
	const PosedCreature creature;
	const auto between = ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity);
	ASSERT_TRUE(between.has_value());
	EXPECT_EQ(between->position, creature.drawnAt);
	EXPECT_EQ(between->rotation, creature.drawnRotation);
	const auto drawn = ecs::creature_pose::DrawnPlacementOf(creature.Lookup(), creature.entity);
	EXPECT_EQ(drawn.position, creature.drawnAt);
	EXPECT_EQ(drawn.rotation, creature.drawnRotation);
}

TEST(CreaturePose, BetweenTurnsFallsBackWhenTheTransformWasMovedElsewhere)
{
	PosedCreature creature;
	const glm::vec3 elsewhere {300.0f, 0.0f, 300.0f};
	creature.registry.Get<Transform>(creature.entity).position = elsewhere;
	EXPECT_FALSE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
	const auto drawn = ecs::creature_pose::DrawnPlacementOf(creature.Lookup(), creature.entity);
	EXPECT_EQ(drawn.position, elsewhere);
	EXPECT_EQ(drawn.rotation, glm::mat3(1.0f));
}

TEST(CreaturePose, BetweenTurnsFallsBackBeforeTheFirstTurn)
{
	PosedCreature creature;
	creature.registry.Get<CreatureLocomotion>(creature.entity).started = false;
	EXPECT_FALSE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
	EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(creature.Lookup(), creature.entity).position, creature.at);
}

TEST(CreaturePose, BetweenTurnsFallsBackInThePhysicsOrTheHand)
{
	{
		PosedCreature creature;
		creature.registry.Assign<PhysicsDrawPose>(creature.entity);
		EXPECT_FALSE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
	}
	{
		PosedCreature creature;
		const glm::vec3 inHand {10.0f, 40.0f, 12.0f};
		creature.registry.Assign<HandDrawPose>(creature.entity, HandDrawPose {.position = inHand});
		EXPECT_FALSE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
		// drawn at the hand's pose, as ecs::DrawnModel draws it
		EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(creature.Lookup(), creature.entity).position, inHand);
	}
	// and anything that is no creature has no pose between turns
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(ecs::creature_pose::BetweenTurns(std::as_const(registry), thing).has_value());
}

TEST(CreaturePose, BetweenTurnsMakesNoStorage)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(1.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto before = StorageSizesOf(registry);
	EXPECT_FALSE(ecs::creature_pose::BetweenTurns(std::as_const(registry), thing).has_value());
	EXPECT_EQ(ecs::creature_pose::DrawnPlacementOf(std::as_const(registry), thing).position, glm::vec3(1.0f));
	EXPECT_EQ(StorageSizesOf(registry), before);

	// nor for a creature with no hand or physics pose
	const PosedCreature creature;
	const auto posed = StorageSizesOf(creature.registry);
	EXPECT_TRUE(ecs::creature_pose::BetweenTurns(creature.Lookup(), creature.entity).has_value());
	EXPECT_EQ(StorageSizesOf(creature.registry), posed);
}
