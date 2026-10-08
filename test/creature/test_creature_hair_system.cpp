/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature hair system on a registry of its own: hidden hair is not moved and is cleared, and a creature without
// its species' rig or a posed body has no hair, the roots ride on the body where it is drawn between turns, and in its
// pen the roots, strands and their width shrink with the body

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureRig.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/Implementations/CreatureHairSystem.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureHairSystem;

namespace
{
constexpr std::chrono::duration<float, std::milli> k_Frame {33.0f};

class CreatureHairSystemTest: public ::testing::Test
{
protected:
	static CreatureHair& HairOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureHair>(creature);
	}
	static void GiveHair(entt::entity creature)
	{
		auto& hair = HairOf(creature);
		hair.groups.resize(1);
		hair.groups.front().strands.push_back({.positions = {glm::vec3(1.0f)}, .velocities = {glm::vec3(0.0f)}});
		hair.started = true;
	}

	// for the rig read from a synthetic block
	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	CreatureHairSystem _system;
};
} // namespace

TEST_F(CreatureHairSystemTest, HiddenHairIsNotMoved)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.SetShown(false);
	EXPECT_FALSE(_system.IsShown());
	GiveHair(creature);
	_system.Update(k_Frame);
	// left as it was: nothing looked at it
	ASSERT_EQ(HairOf(creature).groups.size(), 1u);
	EXPECT_EQ(HairOf(creature).groups.front().strands.size(), 1u);
}

TEST_F(CreatureHairSystemTest, HidingClearsTheHair)
{
	const auto creature = test::creature_world::World::MakeCreature();
	GiveHair(creature);
	_system.SetShown(false);
	EXPECT_TRUE(HairOf(creature).groups.empty());
	EXPECT_FALSE(HairOf(creature).started);
	_system.SetShown(true);
	EXPECT_TRUE(_system.IsShown());
}

TEST_F(CreatureHairSystemTest, NoRigOrPoseMeansNoHair)
{
	const auto creature = test::creature_world::World::MakeCreature();
	GiveHair(creature);
	_system.Update(k_Frame);
	EXPECT_TRUE(HairOf(creature).groups.empty());
}

namespace
{
/// The Giant Ape's rig in the cache with one group of one strand, rooted on a triangle of its first bone
void LoadHairyApe(test::creature_world::World& world, float thickness = 0.1f)
{
	test::creature_block::Block block;
	block.clips = {test::creature_block::Clip {}};
	world.LoadApeRig(block, {{"move", {"Cstand"}}});
	auto rig = Locator::resources::value().GetCreatureRigs().Handle(creature::GetRigId(CreatureType::GiantApe));
	creature::CreatureRig::HairStrand strand {
	    .turned = false, .angles = {}, .vertices = {}, .bones = {0, 0, 0}, .u = 0.25f, .v = 0.25f};
	for (auto& mesh : strand.vertices)
	{
		mesh = {glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(1.0f, 5.0f, 0.0f), glm::vec3(0.0f, 5.0f, 1.0f)};
	}
	const creature_hair::Look look {
	    .colour = glm::ivec3(10), .length = 1.0f, .damping = 0.5f, .stiffness = 0.5f, .thickness = thickness};
	rig->hairGroups = {{.segmentCount = 3, .textured = false, .looks = {look, look, look}, .strands = {strand}}};
}

/// A posed creature standing where its last turn put it, its body drawn at `drawnAt`
void PoseForTheFrame(entt::entity creature, glm::vec3 drawnAt)
{
	auto& registry = test::creature_world::World::Registry();
	registry.Get<CreatureAnimation>(creature).boneMatrices.assign(1, glm::mat4(1.0f));
	const auto& transform = registry.Get<Transform>(creature);
	auto& locomotion = registry.Get<CreatureLocomotion>(creature);
	locomotion.started = true;
	locomotion.toPosition = transform.position;
	registry.Get<CreatureDrawPose>(creature) = {.position = drawnAt, .rotation = transform.rotation};
}
} // namespace

TEST_F(CreatureHairSystemTest, TheRootsRideOnTheBodyWhereItIsDrawnBetweenTurns)
{
	LoadHairyApe(_world);
	// one drawn half a step behind where its turn put it, one standing where the first is drawn
	const auto walking = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	const auto standing = test::creature_world::World::MakeCreature(glm::vec3(96.0f, 0.0f, 102.0f));
	const auto& registry = test::creature_world::World::Registry();
	const auto standingAt = registry.Get<Transform>(standing).position;
	PoseForTheFrame(walking, standingAt);
	PoseForTheFrame(standing, standingAt);
	ASSERT_NE(registry.Get<Transform>(walking).position, standingAt);
	const auto walkingAt = registry.Get<Transform>(walking).position;

	_system.Update(k_Frame);

	ASSERT_EQ(HairOf(walking).groups.size(), 1u);
	ASSERT_EQ(HairOf(standing).groups.size(), 1u);
	const auto& drawn = HairOf(walking).groups.front().strands;
	const auto& reference = HairOf(standing).groups.front().strands;
	ASSERT_EQ(drawn.size(), 1u);
	ASSERT_EQ(reference.size(), 1u);
	ASSERT_EQ(drawn.front().positions.size(), 3u);
	ASSERT_EQ(reference.front().positions.size(), 3u);
	for (size_t i = 0; i < drawn.front().positions.size(); ++i)
	{
		for (glm::length_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(drawn.front().positions[i][axis], reference.front().positions[i][axis], 1e-4f);
		}
	}
	// and its Transform stays where the turn put it
	EXPECT_EQ(registry.Get<Transform>(walking).position, walkingAt);
}

namespace
{
/// A creature's hair after a frame, at its own size and drawn at `share` of it (as in its temple's pen)
std::pair<CreatureHair, CreatureHair> HairAtFullAndShrunk(CreatureHairSystem& system, float share)
{
	auto& registry = test::creature_world::World::Registry();
	const auto full = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	const auto shrunk = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	PoseForTheFrame(full, registry.Get<Transform>(full).position);
	PoseForTheFrame(shrunk, registry.Get<Transform>(shrunk).position);
	const auto own = registry.Get<Transform>(shrunk).scale;
	registry.Get<CreatureDrawPose>(shrunk).scale = own * share;
	system.Update(k_Frame);
	// its own scale stays
	EXPECT_EQ(registry.Get<Transform>(shrunk).scale, own);
	return {registry.Get<CreatureHair>(full), registry.Get<CreatureHair>(shrunk)};
}

constexpr float k_PenShare = 0.22f;
} // namespace

TEST_F(CreatureHairSystemTest, InItsPenTheRootsAndStrandsShrinkWithTheBody)
{
	// with no thickness the root is not sunk into the body, so it lies where the body's surface is drawn
	LoadHairyApe(_world, 0.0f);
	const auto [full, shrunk] = HairAtFullAndShrunk(_system, k_PenShare);
	const auto origin = glm::vec3(100.0f, 0.0f, 100.0f);
	ASSERT_EQ(full.groups.size(), 1u);
	ASSERT_EQ(shrunk.groups.size(), 1u);
	const auto& big = full.groups.front().strands.front().positions;
	const auto& small = shrunk.groups.front().strands.front().positions;
	ASSERT_EQ(big.size(), 3u);
	ASSERT_EQ(small.size(), 3u);
	// the root's offset from the body's origin, and each segment, shrink by the share the body does
	for (glm::length_t axis = 0; axis < 3; ++axis)
	{
		EXPECT_NEAR(small[0][axis] - origin[axis], (big[0][axis] - origin[axis]) * k_PenShare, 1e-4f);
	}
	EXPECT_NEAR(glm::length(small[1] - small[0]), glm::length(big[1] - big[0]) * k_PenShare, 1e-4f);
}

TEST_F(CreatureHairSystemTest, InItsPenTheStrandsAreThinnerWithTheBody)
{
	LoadHairyApe(_world);
	const auto [full, shrunk] = HairAtFullAndShrunk(_system, k_PenShare);
	ASSERT_EQ(full.groups.size(), 1u);
	ASSERT_EQ(shrunk.groups.size(), 1u);
	EXPECT_GT(full.groups.front().halfWidth, 0.0f);
	EXPECT_NEAR(shrunk.groups.front().halfWidth, full.groups.front().halfWidth * k_PenShare, 1e-5f);
}
