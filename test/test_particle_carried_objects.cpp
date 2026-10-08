/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The objects a particle system carries: the take and the release (the object's carried flag), the vortex's fling
// and PushObject on a carried object.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByParticleSystem.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/LivingPhysics.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using openblack::ecs::physics::PhysicsObjects;
namespace particle_carried_objects = openblack::ecs::physics::particle_carried_objects;

class ParticleCarriedObjectsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		map_cells::Clear();
		object_index::OnLoadMap();
	}
	void TearDown() override
	{
		map_cells::Clear();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A static mobile object (MobileStatic) in the map cells
	static entt::entity Rock(const glm::vec3& p)
	{
		const auto e = Reg().Create();
		object_index::Assign(e);
		Reg().Assign<Transform>(e, p, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<MobileStatic>(e);
		map_cells::InsertMapObject(e);
		return e;
	}
};

TEST_F(ParticleCarriedObjectsTest, FireSeesTheMapFlag)
{
	// IsObjectInMap for the fire: the map flag, so an object taken out of the map (held, flying, carried)
	// is not in the map, and a fireball never is
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	EXPECT_TRUE(fire::traits::IsObjectInMap(rock));
	map_cells::RemoveMapObject(rock);
	EXPECT_FALSE(fire::traits::IsObjectInMap(rock));
	const auto ball = Reg().Create();
	Reg().Assign<Transform>(ball, glm::vec3(10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<MagicFireBall>(ball);
	EXPECT_FALSE(fire::traits::IsObjectInMap(ball));
}

TEST_F(ParticleCarriedObjectsTest, TakeAndReleaseRoundTrip)
{
	const auto rock = Rock(glm::vec3(105.0f, 0.0f, 205.0f));
	ASSERT_TRUE(map_cells::IsObjectInMap(rock));
	EXPECT_TRUE(PhysicsObjects::CanWakeKnockedProxy(rock));

	// Take: out of the map cells, the carried flag set; a second take is refused (already in physics)
	ASSERT_TRUE(particle_carried_objects::Take(rock));
	EXPECT_TRUE(particle_carried_objects::IsCarried(rock));
	EXPECT_TRUE(Reg().AllOf<CarriedByParticleSystem>(rock));
	EXPECT_FALSE(map_cells::IsObjectInMap(rock));
	EXPECT_FALSE(particle_carried_objects::Take(rock));
	// PushObject on a carried object: no body, no force
	EXPECT_EQ(PhysicsObjects::PushObject(rock, entt::null), 0.0f);
	EXPECT_EQ(PhysicsObjects::Find(rock), nullptr);
	// a knocked resting proxy of a carried object is not woken
	EXPECT_FALSE(PhysicsObjects::CanWakeKnockedProxy(rock));

	// Release: the atom's angles (a MobileStatic keeps the tilt), Pos = ftol(x 6553.6) with altitude 0, back in the
	// map, the carried flag cleared
	const auto rows = affine::RotationYXZ(0.4f, 0.1f, -0.2f);
	particle_carried_objects::Release(rock, rows, glm::vec3(305.3f, 40.0f, 405.7f));
	EXPECT_FALSE(particle_carried_objects::IsCarried(rock));
	EXPECT_TRUE(map_cells::IsObjectInMap(rock));
	EXPECT_TRUE(PhysicsObjects::CanWakeKnockedProxy(rock));
	const auto& transform = Reg().Get<const Transform>(rock);
	EXPECT_EQ(transform.position.x, map_coords::ToMetres(map_coords::ToFixed(305.3f)));
	EXPECT_EQ(transform.position.z, map_coords::ToMetres(map_coords::ToFixed(405.7f)));
	EXPECT_EQ(transform.position.y, 0.0f); // no island: the ground is 0 and the altitude 0
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(transform.rotation[c][r], rows[c][r], 1e-6f);
		}
	}
}

TEST_F(ParticleCarriedObjectsTest, ReleasedLivingKeepsItsDrawnYaw)
{
	// a Living's Transform is the drawn rotation (the original's rows + the draw's quarter turn); the atom carries it,
	// and the release's angles (DecomposeYXZ) work on the original's rows: the drawn yaw comes back unchanged, not a
	// quarter turn more
	const auto animal = Reg().Create();
	Reg().Assign<Animal>(animal);
	const auto drawn = affine::AngleY(0.9f + glm::half_pi<float>());
	const auto back = PhysicsObjects::DrawQuarterTurn(PhysicsObjects::RotationFromRows(animal, living::OriginalRows(drawn)));
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(back[c][r], drawn[c][r], 1e-6f);
		}
	}
}

TEST(ParticleCarriedObjects, FlingMotion)
{
	// the vortex's fling with fixed draws: v = (sin a B, C, -cos a B), w = ((0 - v.z) / h, 0, (v.x - 0) / h)
	// (v.z first, then v.x): the spin is (cos a B / h, 0, sin a B / h), NOT -C / h
	constexpr float k_H = 0.8f;
	constexpr float k_Angle = 0.6f;
	constexpr float k_Speed = 1.0f;
	const auto m = particle_carried_objects::FlingMotionOf(k_H, k_Angle, k_Speed, 2.5f, 1.5f);
	const float b = (2.5f + 8.0f) * k_Speed;
	const float c = (1.5f + 10.0f) * k_Speed;
	EXPECT_NEAR(m.velocity.x, std::sin(k_Angle) * b, 1e-4f);
	EXPECT_EQ(m.velocity.y, c);
	EXPECT_NEAR(m.velocity.z, -std::cos(k_Angle) * b, 1e-4f);
	EXPECT_NEAR(m.spin.x, std::cos(k_Angle) * b / k_H, 1e-4f);
	EXPECT_EQ(m.spin.y, 0.0f);
	EXPECT_NEAR(m.spin.z, std::sin(k_Angle) * b / k_H, 1e-4f);
}
