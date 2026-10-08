/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature as a spell's target: the fire's creature test, the creature's row of the game's tables for the burn and
// defence multipliers, the explosions and tornadoes leaving a creature alone, and the creature's own damage not ported
// yet, which leaves everything else's as it was

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "Particles/Rules/Explosion.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
using Number = openblack::ecs::effects::EffectValues::Number;

namespace
{
class CreatureSpellHooksTest: public ::testing::Test
{
protected:
	static entt::entity Rock()
	{
		auto& registry = test::creature_world::World::Registry();
		const auto rock = registry.Create();
		registry.Assign<Transform>(rock, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<MobileObject>(rock, MobileObjectInfo::Ball);
		return rock;
	}

	test::creature_world::World _world;
};

TEST_F(CreatureSpellHooksTest, OnlyACreatureThatIsStillThereIsACreatureToTheFire)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto rock = Rock();
	EXPECT_TRUE(ecs::fire::traits::IsCreature(creature));
	EXPECT_FALSE(ecs::fire::traits::IsCreature(rock));
	EXPECT_FALSE(ecs::fire::traits::IsCreature(entt::null));
	test::creature_world::World::Registry().Destroy(creature);
	EXPECT_FALSE(ecs::fire::traits::IsCreature(creature));
}

TEST_F(CreatureSpellHooksTest, ACreaturesObjectInfoIsItsSpeciesRow)
{
	auto& row = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
	row.defenceMultiplierBurn = 0.25f;
	row.defenceMultiplierHit = 0.5f;
	const auto creature = test::creature_world::World::MakeCreature();

	EXPECT_EQ(ecs::physics::PhysicsObjects::ObjectInfo(creature), &row);
	EXPECT_EQ(ecs::fire::traits::InfoOf(creature), &row);
	const auto multipliers = ecs::effects::GetDefenseMultiplier(creature);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::Burn)), 0.25f);
	EXPECT_FLOAT_EQ(multipliers.at(static_cast<size_t>(Number::Hit)), 0.5f);
}

TEST_F(CreatureSpellHooksTest, ExplosionsAndTornadoesCannotDestroyACreature)
{
	const auto creature = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(psys::explosion::CanBeDestroyedBySpell(creature, entt::null));
}

TEST_F(CreatureSpellHooksTest, BurningLeavesACreatureAsItWasAndHurtsAnythingElse)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	registry.Assign<Life>(creature, 0.8f);
	const auto rock = Rock();
	registry.Assign<Life>(rock, 0.8f);

	EXPECT_FLOAT_EQ(ecs::fire::traits::ReduceLifeDueToBurning(creature, 0.25f, std::nullopt), 0.8f);
	EXPECT_FLOAT_EQ(registry.Get<Life>(creature).value, 0.8f);
	EXPECT_FLOAT_EQ(ecs::fire::traits::ReduceLifeDueToBurning(rock, 0.25f, std::nullopt), 0.55f);
	EXPECT_FLOAT_EQ(registry.Get<Life>(rock).value, 0.55f);
}
} // namespace
