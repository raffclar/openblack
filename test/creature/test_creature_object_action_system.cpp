/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature object-action system with fake animations and locomotion: what can be eaten, picked up and knocked
// down, the actions playing on by the turn while the frame only draws, a villager eaten going out as any death does,
// the moments things are taken hold of and let go of drawn at once where they are, and what is held drawn in the hand
// where the body is drawn between turns

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>

#include <gtest/gtest.h>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureThrow.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/PhysicsDrawPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/ToBeDeleted.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureObjectActionSystem;
namespace fakes = openblack::test::creature_fakes;
using creature_object_actions::Kind;
using creature_object_actions::Status;
using Phase = CreatureObjectAction::Phase;

namespace
{
class CreatureObjectActionSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		_animation = &static_cast<fakes::FakeAnimation&>(Locator::creatureAnimationSystem::emplace<fakes::FakeAnimation>());
		_animation->bone = glm::vec3(0.0f, 10.0f, 0.0f);
		Locator::creatureLocomotionSystem::emplace<fakes::FakeLocomotion>();
		auto& info = _world.Info();
		info.villager.at(0).foodValue = 5.0f;
		info.villager.at(0).weight = 2.0f;
		info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::LumpOfPoo)).foodValue = 0.0f;
		info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::Ball)).weight = 1.0f;
	}
	void TearDown() override { test::ResetMapAndVillagerDefaults(); }

	static entt::entity Thing(glm::vec3 position)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}
	static entt::entity Rock(glm::vec3 position)
	{
		const auto entity = Thing(position);
		test::creature_world::World::Registry().Assign<MobileObject>(entity, MobileObjectInfo::Ball);
		return entity;
	}
	/// A Giant Ape with its rig in the cache, so that it has hands to act with
	entt::entity Ape()
	{
		test::creature_block::Block block;
		block.clips = {test::creature_block::Clip {}};
		_world.LoadApeRig(block, {{"move", {"Cstand"}}});
		return test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	}
	/// An action at its moment, playing one animation
	static CreatureObjectAction AtItsMoment(Kind kind, std::optional<entt::entity> target)
	{
		CreatureObjectAction action {.kind = kind, .phase = Phase::Playing, .status = Status::Running, .target = target};
		action.animations.front() = creature_throw::k_Eat;
		action.weights.front() = 1.0f;
		action.animationCount = 1;
		action.durationMs = 1000.0f;
		return action;
	}
	static void Hold(entt::entity creature, entt::entity object)
	{
		auto& registry = test::creature_world::World::Registry();
		registry.AssignState<CreatureHeldObject>(creature, CreatureHeldObject {.object = object});
		registry.AssignState<HeldByCreature>(object, HeldByCreature {.creature = creature});
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	// the world lists a creature eating or knocking a thing down reaches (the dead list, the fires...), reset after the
	// registry as the game's shutdown does
	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	const test::RestoreService<Locator::creatureAnimationSystem> _restoreAnimation;
	const test::RestoreService<Locator::creatureLocomotionSystem> _restoreLocomotion;
	fakes::FakeAnimation* _animation {nullptr};
	CreatureObjectActionSystem _system;
};
} // namespace

TEST_F(CreatureObjectActionSystemTest, WhatCanBeEatenPickedUpAndKnockedDown)
{
	auto& registry = test::creature_world::World::Registry();
	const auto villager = Thing(glm::vec3(0.0f));
	registry.Assign<Villager>(villager);
	const auto rock = Rock(glm::vec3(0.0f));
	const auto tree = Thing(glm::vec3(0.0f));
	registry.Assign<Tree>(tree);
	const auto creature = test::creature_world::World::MakeCreature();

	EXPECT_EQ(_system.FoodValueOf(villager), std::optional(5.0f));
	EXPECT_FALSE(_system.FoodValueOf(rock).has_value());
	EXPECT_FALSE(_system.FoodValueOf(creature).has_value());
	EXPECT_TRUE(_system.CanPickUp(villager));
	EXPECT_TRUE(_system.CanPickUp(rock));
	EXPECT_FALSE(_system.CanPickUp(tree));
	EXPECT_FALSE(_system.CanPickUp(creature));
	EXPECT_TRUE(_system.CanDestroy(tree));
	EXPECT_TRUE(_system.CanDestroy(rock));
	EXPECT_FALSE(_system.CanDestroy(villager));
	// something going is none of these
	ecs::ToBeDeleted(rock);
	EXPECT_FALSE(_system.CanPickUp(rock));
	EXPECT_FALSE(_system.CanDestroy(rock));
}

TEST_F(CreatureObjectActionSystemTest, TheTurnPlaysTheActionAndTheFrameOnlyDraws)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	auto action = AtItsMoment(Kind::Keep, std::nullopt);
	action.eventDone = true;
	action.durationMs = 5000.0f;
	registry.AssignState<CreatureObjectAction>(creature, action);
	_animation->durations[creature_throw::k_Eat] = 5000.0f;
	const auto rate = creature_layers::PlaybackRate(registry.Get<Creature>(creature).size);

	_system.ProcessTurn();
	const auto& played = registry.Get<CreatureObjectAction>(creature);
	EXPECT_FLOAT_EQ(played.timeMs, 100.0f * rate);
	EXPECT_EQ(played.status, Status::Running);

	// a frame half way through the turn draws the animation half a turn on, and moves nothing
	const auto before = played;
	_system.UpdateDraw(0.5f);
	const auto& after = registry.Get<CreatureObjectAction>(creature);
	EXPECT_FLOAT_EQ(after.timeMs, before.timeMs);
	EXPECT_EQ(after.status, before.status);
	EXPECT_EQ(after.eventDone, before.eventDone);
	const auto& slots = registry.Get<CreatureAnimation>(creature).slots;
	ASSERT_EQ(slots.size(), 1u);
	EXPECT_FLOAT_EQ(slots.front().timeMs, before.timeMs + (0.5f * 100.0f * rate));
}

TEST_F(CreatureObjectActionSystemTest, AVillagerEatenDiesAsAnyDeathGoes)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto home = Thing(glm::vec3(0.0f));
	auto& abode = registry.Assign<Abode>(home);
	const auto villager = Thing(glm::vec3(100.0f, 10.0f, 100.0f));
	auto& lives = registry.Assign<Villager>(villager);
	lives.abode = home;
	lives.town = entt::null;
	lives.life = 1.0f;
	abode.inhabitants.push_back(villager);
	Hold(creature, villager);
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::Eat, villager));

	_system.ProcessTurn();

	EXPECT_FALSE(ecs::IsAvailable(villager));
	const auto& inhabitants = registry.Get<Abode>(home).inhabitants;
	EXPECT_EQ(std::ranges::find(inhabitants, villager), inhabitants.end());
	EXPECT_FALSE(_system.GetHeld(creature).has_value());
	EXPECT_FALSE(std::as_const(registry).AllOf<CreatureHeldObject>(creature));
}

TEST_F(CreatureObjectActionSystemTest, TakenHoldOfAndLetGoOfThingsAreDrawnAtOnceWhereTheyAre)
{
	const auto creature = Ape();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = Rock(glm::vec3(110.0f, 0.0f, 100.0f));
	registry.AssignState<CreatureObjectAction>(creature, AtItsMoment(Kind::PickUp, rock));
	_world.teleported.clear();

	// at the moment the hand takes hold, the rock is put in the palm, in the animation as it plays
	_system.ProcessTurn();
	ASSERT_EQ(_system.GetHeld(creature), std::optional(rock));
	EXPECT_EQ(registry.Get<CreatureObjectAction>(creature).status, Status::Contact);
	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), rock);
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(100.0f, 10.0f, 100.0f));
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(rock));

	// every frame it is drawn in the hand where the body is drawn: standing, where the turn put the body
	auto& animation = registry.Get<CreatureAnimation>(creature);
	if (animation.boneMatrices.empty())
	{
		animation.boneMatrices.assign(1, glm::mat4(1.0f));
	}
	const auto body = registry.Get<Transform>(creature);
	auto& locomotion = registry.Get<CreatureLocomotion>(creature);
	locomotion.started = true;
	locomotion.toPosition = body.position;
	auto& pose = registry.Get<CreatureDrawPose>(creature);
	pose = {.position = body.position, .rotation = body.rotation};
	const auto held = registry.Get<Transform>(rock);
	_system.UpdateHeldDraw();
	const auto standing = registry.Get<PhysicsDrawPose>(rock);
	// between turns, moved on with the body by as much as the body is; the rock's Transform stays where the turn put it
	const glm::vec3 step {-4.0f, 0.0f, 2.0f};
	pose.position = body.position + step;
	_system.UpdateHeldDraw();
	const auto& between = registry.Get<PhysicsDrawPose>(rock);
	for (glm::length_t axis = 0; axis < 3; ++axis)
	{
		EXPECT_NEAR(between.position[axis], standing.position[axis] + step[axis], 1e-4f);
	}
	EXPECT_EQ(between.rotation, standing.rotation);
	EXPECT_EQ(registry.Get<Transform>(rock).position, held.position);
	EXPECT_EQ(registry.Get<Transform>(rock).rotation, held.rotation);
	EXPECT_EQ(registry.Get<Transform>(creature).position, body.position);

	// let go of where it is
	_world.teleported.clear();
	CreatureObjectActionSystem::ReleaseHeld(rock, glm::vec3(120.0f, 0.0f, 90.0f), glm::vec3(0.0f), creature);
	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), rock);
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(120.0f, 0.0f, 90.0f));
	EXPECT_FALSE(std::as_const(registry).AllOf<HeldByCreature>(rock));
	// the creature that held it no longer does, so the next turn finds its hands empty of it
	registry.RemoveState<CreatureHeldObject>(creature);
	EXPECT_FALSE(_system.GetHeld(creature).has_value());
}
