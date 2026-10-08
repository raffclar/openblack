/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature physiology system on a registry of its own: the body's turns by the time scale, eating and finishing an
// action, a poo dropped as a lump of poo on the synced stream, sick sprayed as drops that are no entity and draw on no
// game stream, and the night from the day / night clock

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <numbers>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/DayNightClock.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "creature/CreatureSystemWorld.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreaturePhysiologySystem;

namespace
{
class CreaturePhysiologySystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		// noon, as a land opens
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>().Clock().Reset();
		// a year of the body's life an hour, and a species that does not wake by itself before 500 turns asleep
		auto& ape = _world.Info().creature.at(creature::InfoRow(CreatureType::GiantApe));
		ape.secondsPerAgeTick = 3600;
		ape.secondsToDehydrate = 5000.0f;
		ape.sleepLength = 100.0f;
		ape.foodToEnergy = 1000.0f;
		ape.startEnergy = 0.5f;
	}
	void TearDown() override { test::ResetMapAndVillagerDefaults(); }

	static CreatureNeeds& NeedsOf(entt::entity creature)
	{
		return test::creature_world::World::Registry().Get<CreatureNeeds>(creature);
	}
	static size_t Things() { return std::as_const(test::creature_world::World::Registry()).Size<Transform>(); }

	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	const test::RestoreService<Locator::dayNightClock> _restoreClock;
	CreaturePhysiologySystem _system;
};
} // namespace

TEST_F(CreaturePhysiologySystemTest, TheBodysTurnsFollowTheTimeScale)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.SetTimeScale(0.5f);
	_system.ProcessTurn();
	EXPECT_TRUE(NeedsOf(creature).started);
	EXPECT_EQ(NeedsOf(creature).needs.turns, 0u);
	_system.ProcessTurn();
	EXPECT_EQ(NeedsOf(creature).needs.turns, 1u);
	_system.SetTimeScale(2.0f);
	_system.ProcessTurn();
	EXPECT_EQ(NeedsOf(creature).needs.turns, 3u);
	// within what can be run in a turn
	_system.SetTimeScale(-1.0f);
	EXPECT_FLOAT_EQ(_system.GetTimeScale(), 0.0f);
	_system.SetTimeScale(1e9f);
	EXPECT_FLOAT_EQ(_system.GetTimeScale(), 3600.0f);
	_system.SetFaintingEnabled(false);
	EXPECT_FALSE(_system.IsFaintingEnabled());
}

TEST_F(CreaturePhysiologySystemTest, EatingAndFinishingAnAction)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.Eat(creature, 100.0f);
	EXPECT_TRUE(NeedsOf(creature).started);
	EXPECT_EQ(NeedsOf(creature).needs.meals, 1u);
	EXPECT_GT(NeedsOf(creature).needs.energy, 0.5f);

	auto& row = _world.Info().creatureAction.at(3);
	constexpr char k_Name[] = "HardWork";
	std::memcpy(row.name.data(), k_Name, sizeof(k_Name));
	row.exhaustionCost = 0.25f;
	row.strengthGain = 0.1f;
	const auto exhaustion = NeedsOf(creature).needs.exhaustion;
	const auto strength = test::creature_world::World::Registry().Get<Creature>(creature).strength;
	_system.FinishAction(creature, "HardWork");
	EXPECT_GT(NeedsOf(creature).needs.exhaustion, exhaustion);
	EXPECT_GT(test::creature_world::World::Registry().Get<Creature>(creature).strength, strength);
	// an action the table does not name costs nothing
	const auto after = NeedsOf(creature).needs.exhaustion;
	_system.FinishAction(creature, "Nothing");
	EXPECT_FLOAT_EQ(NeedsOf(creature).needs.exhaustion, after);
}

TEST_F(CreaturePhysiologySystemTest, APooIsALumpOfPooTurnedOnTheSyncedStream)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_world.teleported.clear();
	std::vector<float> floatDraws;
	int draws = 0;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t) {
		    ++draws;
		    return 0u;
	    },
	    [&floatDraws](float x) {
		    floatDraws.push_back(x);
		    return 1.0f;
	    });
	const auto before = Things();
	_system.Poo(creature);
	ASSERT_EQ(floatDraws.size(), 1u);
	EXPECT_FLOAT_EQ(floatDraws.front(), 2.0f * std::numbers::pi_v<float>);
	EXPECT_EQ(draws, 0);
	ASSERT_EQ(Things(), before + 1);
	ASSERT_EQ(_world.teleported.size(), 1u);
	const auto poo = _world.teleported.front();
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	ASSERT_TRUE(registry.AllOf<MobileObject>(poo));
	EXPECT_EQ(registry.Get<MobileObject>(poo).type, MobileObjectInfo::LumpOfPoo);
	EXPECT_EQ(registry.Get<Transform>(poo).rotation, affine::AngleY(1.0f));
}

TEST_F(CreaturePhysiologySystemTest, SickIsDropsOfItsOwnThatGoAfterFourSeconds)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	const auto seeds = game_random::Current();
	const auto crt = game_random::crt::Seed();
	const auto before = Things();

	_system.Puke(creature);
	ASSERT_EQ(_system.GetPukeDrops().size(), 12u);
	EXPECT_EQ(Things(), before);
	EXPECT_EQ(game_random::Current().synced, seeds.synced);
	EXPECT_EQ(game_random::Current().local, seeds.local);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	// thrown forwards and up from the mouth: the creature faces -z
	for (const auto& drop : _system.GetPukeDrops())
	{
		EXPECT_LT(drop.velocity.z, 0.0f);
		EXPECT_GT(drop.velocity.y, 0.0f);
	}

	for (int second = 0; second < 3; ++second)
	{
		_system.Update(1.0f);
	}
	ASSERT_EQ(_system.GetPukeDrops().size(), 12u);
	// on the land by now, and fading
	EXPECT_FLOAT_EQ(_system.GetPukeDrops().front().position.y, 0.0f);
	EXPECT_LT(_system.GetPukeDrops().front().tint.a, 1.0f);
	_system.Update(1.0f);
	EXPECT_TRUE(_system.GetPukeDrops().empty());
}

TEST_F(CreaturePhysiologySystemTest, TheNightKeepsARestedCreatureAsleep)
{
	auto& clock = Locator::dayNightClock::value();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& needs = NeedsOf(creature);
	needs.rest = CreatureNeeds::Rest::Asleep;
	needs.restTurns = 60;
	_system.ProcessTurn();
	// fully rested after the fifty turns it must sleep: in the day it may wake
	EXPECT_TRUE(NeedsOf(creature).rested);

	clock.Clock().SetScriptTime(0.0f);
	ASSERT_TRUE(clock.Clock().IsVisualNight());
	NeedsOf(creature).rested = false;
	_system.ProcessTurn();
	EXPECT_FALSE(NeedsOf(creature).rested);
}
