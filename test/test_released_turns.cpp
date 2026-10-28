/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <glm/gtx/euler_angles.hpp>
#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Common/X87Angle.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/TurnAngles.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/TornadoSystem.h"
#include "ECS/Systems/Implementations/VortexSystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// A turn that leans past a quarter about x, as a particle tumbles a thing
const glm::mat3 k_Tumbled = glm::mat3(glm::eulerAngleY(-0.6f) * glm::eulerAngleX(-2.2f) * glm::eulerAngleZ(0.3f));

class ReleasedTurns: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<Registry>();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	/// A thing that a particle of the given kind carries with the tumbled turn. The particle has gone.
	static entt::entity Carried(particles::CarriedObject::Carrier carrier)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto thing = registry.Create();
		registry.Assign<Transform>(thing, glm::vec3(0.0f), k_Tumbled, glm::vec3(1.0f));
		auto carried = std::make_shared<particles::CarriedObject>(particles::CarriedObject {
		    .object = thing, .carrier = carrier, .position = {10.0f, 5.0f, 20.0f}, .rotation = k_Tumbled});
		registry.Assign<CarriedByTornado>(thing, CarriedByTornado {.carried = carried});
		return thing;
	}
};

} // namespace

// An animal that a tornado lets go of stands upright, turned by the angle about y of its turn in the tornado
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ReleasedTurns, ATornadoPutsALivingThingDownUpright)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto animal = Carried(particles::CarriedObject::Carrier::Tornado);
	registry.Assign<Animal>(animal);
	systems::TornadoSystem tornado;
	tornado.ProcessTurn();
	ASSERT_FALSE(registry.AllOf<CarriedByTornado>(animal));
	const auto& turn = registry.Get<const Transform>(animal).rotation;
	EXPECT_EQ(turn, x87::GameUprightTurn(x87::GameTurnMatrixAngles(k_Tumbled).y));
	EXPECT_EQ(turn[1], glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_FALSE(registry.AllOf<TurnAngles>(animal));
}

// A thing that leans takes the angles of its turn in the vortex, and the turn that the angles make
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ReleasedTurns, AVortexThrowsAThingOutByTheAnglesOfItsTurn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = Carried(particles::CarriedObject::Carrier::Vortex);
	registry.Assign<MobileObject>(thing);
	systems::VortexSystem vortex;
	vortex.ThrowOutOfVortex(thing, 0.5f, 1.0f);
	ASSERT_FALSE(registry.AllOf<CarriedByTornado>(thing));
	const auto angles = x87::GameTurnMatrixAngles(k_Tumbled);
	EXPECT_EQ(registry.Get<const Transform>(thing).rotation, x87::GameTurnMatrix(angles.x, angles.y, angles.z));
	EXPECT_EQ(placed_turn::AnglesOf(registry, thing), angles);
}

// A living thing that a vortex throws out keeps no angles and stands upright with the angle about y
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(ReleasedTurns, AVortexThrowsALivingThingOutUpright)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto animal = Carried(particles::CarriedObject::Carrier::Vortex);
	registry.Assign<Animal>(animal);
	systems::VortexSystem vortex;
	vortex.ThrowOutOfVortex(animal, 0.5f, 1.0f);
	EXPECT_EQ(registry.Get<const Transform>(animal).rotation, x87::GameUprightTurn(x87::GameTurnMatrixAngles(k_Tumbled).y));
	EXPECT_FALSE(registry.AllOf<TurnAngles>(animal));
}
