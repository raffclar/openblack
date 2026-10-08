/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// DynamicsSystem::RayCastClosestHit keeps its last rays: a ray asked again with the same bits against the same world
// gives the answer a new cast gives, and any body added or removed, or a reset, makes it cast again.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/DynamicsSystem.h"

using openblack::ecs::systems::DynamicsSystem;

namespace
{
/// A static 2 m box at (x, 0, 0)
struct Box
{
	explicit Box(float x)
	    : shape(btVector3(1.0f, 1.0f, 1.0f))
	    , body(0.0f, nullptr, &shape)
	{
		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(btVector3(x, 0.0f, 0.0f));
		body.setWorldTransform(transform);
		body.setUserIndex(1);
		body.setUserIndex2(static_cast<int>(x));
	}
	btBoxShape shape;
	btRigidBody body;
};

const glm::vec3 k_Origin(0.0f, 10.0f, 0.0f);
const glm::vec3 k_Down(0.0f, -1.0f, 0.0f);
} // namespace

TEST(DynamicsRayCache, TheSameRayAgainstTheSameWorldGivesTheSameHit)
{
	DynamicsSystem dynamics;
	Box box(0.0f);
	dynamics.AddRigidBody(&box.body);
	const auto first = dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f);
	ASSERT_TRUE(first.has_value());
	const auto again = dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f);
	ASSERT_TRUE(again.has_value());
	EXPECT_EQ(again->first.position, first->first.position);
	EXPECT_EQ(again->first.rotation, first->first.rotation);
	EXPECT_EQ(again->second.id, first->second.id);
	EXPECT_FLOAT_EQ(first->first.position.y, 1.0f);
	dynamics.RemoveRigidBody(&box.body);
}

TEST(DynamicsRayCache, EveryChangeOfTheWorldCastsAgain)
{
	DynamicsSystem dynamics;
	Box box(0.0f);
	Box other(0.0f);
	other.body.setUserIndex2(7);
	// a miss is kept too, until a body comes
	EXPECT_FALSE(dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f).has_value());
	dynamics.AddRigidBody(&box.body);
	ASSERT_TRUE(dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f).has_value());
	// removed: no hit any more
	dynamics.RemoveRigidBody(&box.body);
	EXPECT_FALSE(dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f).has_value());
	// another body in the same place: its own details
	dynamics.AddRigidBody(&other.body);
	const auto hit = dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->second.id, 7);
	// a reset empties the world
	dynamics.Reset();
	EXPECT_FALSE(dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f).has_value());
}

TEST(DynamicsRayCache, ADifferentRayOrLengthIsCastOnItsOwn)
{
	DynamicsSystem dynamics;
	Box box(0.0f);
	dynamics.AddRigidBody(&box.body);
	ASSERT_TRUE(dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f).has_value());
	// too short to reach the box
	EXPECT_FALSE(dynamics.RayCastClosestHit(k_Origin, k_Down, 5.0f).has_value());
	// beside the box
	EXPECT_FALSE(dynamics.RayCastClosestHit(glm::vec3(5.0f, 10.0f, 0.0f), k_Down, 1e10f).has_value());
	// more rays than the cache keeps, then the first again
	for (int i = 0; i < 8; ++i)
	{
		static_cast<void>(dynamics.RayCastClosestHit(glm::vec3(0.1f * static_cast<float>(i), 10.0f, 0.0f), k_Down, 1e10f));
	}
	EXPECT_TRUE(dynamics.RayCastClosestHit(k_Origin, k_Down, 1e10f).has_value());
	dynamics.RemoveRigidBody(&box.body);
}
