/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which trees have their roots drawn under them: none standing in the land; one set while a hand pulls at one, holds
// it, a creature holds it or a tornado carries it; two while it moves in the physics; none once it is at rest or sunk

#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/FallingRoots.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/TreeRoots.h"
#include "Physics/ObjectRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using physics::objects::ShownRootsModel;
using physics::objects::ShownRootsScales;
using physics::objects::TreePlace;

namespace
{
/// A tree standing in the land, with nothing else about it
entt::entity StandingTree(Registry& registry)
{
	const auto tree = registry.Create();
	registry.Assign<Tree>(tree);
	return tree;
}

/// A hand pulling at a thing it hasn't yet taken
entt::entity PullingHand(Registry& registry, entt::entity object)
{
	const auto hand = registry.Create();
	auto& grab = registry.Assign<HandGrab>(hand);
	grab.state = HandGrab::State::Grabbing;
	grab.object = object;
	grab.tug = hand_grab::Tug {};
	return hand;
}

std::optional<TreePlace> ShownPlace(const Registry& registry, entt::entity tree)
{
	const auto* shown = registry.TryGet<const ShownRoots>(tree);
	return shown != nullptr ? std::optional(shown->place) : std::nullopt;
}
} // namespace

TEST(TreeRoots, NoneInTheLandOneSetOutOfItTwoWhileMoving)
{
	EXPECT_TRUE(ShownRootsScales(TreePlace::InTheLand).empty());
	EXPECT_TRUE(ShownRootsScales(TreePlace::Still).empty());
	ASSERT_EQ(ShownRootsScales(TreePlace::OutOfTheLand).size(), 1u);
	EXPECT_FLOAT_EQ(ShownRootsScales(TreePlace::OutOfTheLand)[0], 0.15f);
	ASSERT_EQ(ShownRootsScales(TreePlace::Moving).size(), 2u);
	EXPECT_FLOAT_EQ(ShownRootsScales(TreePlace::Moving)[0], 0.15f);
	EXPECT_FLOAT_EQ(ShownRootsScales(TreePlace::Moving)[1], 0.2f);
}

TEST(TreeRoots, TheRootsSitAtTheTreesBaseTurnedAndStretchedWithIt)
{
	// A tree twice its size, turned a quarter, stretched up half again by the hand's pull
	auto tree = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 2.0f, -4.0f));
	tree = glm::rotate(tree, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	tree = glm::scale(tree, glm::vec3(2.0f, 3.0f, 2.0f));
	const auto roots = ShownRootsModel(tree, 0.15f, 4.0f);
	EXPECT_EQ(glm::vec3(roots[3]), glm::vec3(10.0f, 2.0f, -4.0f));
	for (glm::length_t column = 0; column < 3; ++column)
	{
		EXPECT_NEAR(glm::length(glm::vec3(roots[column])), glm::length(glm::vec3(tree[column])) * 0.6f, 1e-5f);
		EXPECT_NEAR(glm::dot(glm::normalize(glm::vec3(roots[column])), glm::normalize(glm::vec3(tree[column]))), 1.0f, 1e-5f);
	}
}

TEST(TreeRoots, ATreeStandingInTheLandShowsNoRoots)
{
	Registry registry;
	const auto tree = StandingTree(registry);
	EXPECT_EQ(tree_roots::PlaceOf(registry, tree), TreePlace::InTheLand);
	tree_roots::Show(registry);
	EXPECT_FALSE(ShownPlace(registry, tree).has_value());
}

TEST(TreeRoots, ATreeShowsItsRootsFromTheMomentTheHandPullsAtIt)
{
	Registry registry;
	const auto tree = StandingTree(registry);
	const auto hand = PullingHand(registry, tree);
	EXPECT_EQ(tree_roots::PlaceOf(registry, tree), TreePlace::OutOfTheLand);
	tree_roots::Show(registry);
	EXPECT_EQ(ShownPlace(registry, tree), TreePlace::OutOfTheLand);

	// Let go before it comes free, it stands in the land again and its roots are hidden
	registry.Get<HandGrab>(hand).state = HandGrab::State::Empty;
	tree_roots::Show(registry);
	EXPECT_FALSE(ShownPlace(registry, tree).has_value());
}

TEST(TreeRoots, ATreeHeldOrCarriedShowsOneSetOfRoots)
{
	Registry registry;
	const auto inHand = StandingTree(registry);
	registry.Assign<InHand>(inHand);
	const auto byCreature = StandingTree(registry);
	registry.Assign<HeldByCreature>(byCreature);
	const auto byTornado = StandingTree(registry);
	registry.Assign<CarriedByTornado>(byTornado);
	tree_roots::Show(registry);
	EXPECT_EQ(ShownPlace(registry, inHand), TreePlace::OutOfTheLand);
	EXPECT_EQ(ShownPlace(registry, byCreature), TreePlace::OutOfTheLand);
	EXPECT_EQ(ShownPlace(registry, byTornado), TreePlace::OutOfTheLand);
}

TEST(TreeRoots, ATreeMovingInThePhysicsShowsTwoSetsUntilItStops)
{
	Registry registry;
	const auto tree = StandingTree(registry);
	registry.Assign<InHand>(tree);
	tree_roots::Show(registry);

	// Thrown: it flies, drawn where it has got to between turns
	registry.Remove<InHand>(tree);
	registry.Assign<InPhysics>(tree);
	registry.Assign<PhysicsDrawPose>(tree);
	tree_roots::Show(registry);
	EXPECT_EQ(ShownPlace(registry, tree), TreePlace::Moving);

	// Sunk out of sight under the sea, nothing of it is drawn
	registry.Get<PhysicsDrawPose>(tree).underSea = true;
	tree_roots::Show(registry);
	EXPECT_FALSE(ShownPlace(registry, tree).has_value());

	// At rest in the physics it has no pose between turns to draw
	registry.Remove<PhysicsDrawPose>(tree);
	EXPECT_EQ(tree_roots::PlaceOf(registry, tree), TreePlace::Still);
	tree_roots::Show(registry);
	EXPECT_FALSE(ShownPlace(registry, tree).has_value());
}

TEST(TreeRoots, AReplantedTreeHidesItsRootsAgain)
{
	Registry registry;
	const auto tree = StandingTree(registry);
	registry.Assign<InPhysics>(tree);
	registry.Assign<PhysicsDrawPose>(tree);
	tree_roots::Show(registry);
	ASSERT_EQ(ShownPlace(registry, tree), TreePlace::Moving);

	// Taking root where it lands, it is back in the land
	registry.Remove<PhysicsDrawPose>(tree);
	registry.Remove<InPhysics>(tree);
	tree_roots::Show(registry);
	EXPECT_FALSE(ShownPlace(registry, tree).has_value());
}

TEST(TreeRoots, OnlyTreesShowRoots)
{
	Registry registry;
	const auto rock = registry.Create();
	registry.Assign<InHand>(rock);
	PullingHand(registry, rock);
	tree_roots::Show(registry);
	EXPECT_FALSE(ShownPlace(registry, rock).has_value());
}
