/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature mind system on a registry of its own: how a species' desires start from the game's tables, the first
// turn's 40 draws on the synced stream, the scans' order and what they pass over, what a creature knows of a villager,
// the night from the day / night clock, and an action's mirroring

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include <gtest/gtest.h>

#include "3D/DayNightClock.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureMindSystem;
namespace mind_detail = openblack::ecs::systems::mind_detail;
using creature_plan_actions::Target;

namespace
{
class CreatureMindSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// each desire's cap, and the range its decay is drawn from, a different one each
		for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
		{
			auto& initial = _world.Info().creatureInitialDesire.at(d);
			initial.field0x3c = 2.0f;
			initial.field0x40 = 0.9f;
			initial.field0x44 = 0.9f + (0.001f * static_cast<float>(d + 1));
		}
	}

	static entt::entity MakeVillager(glm::vec3 position, float life = 1.0f)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		auto& villager = registry.Assign<Villager>(entity);
		villager.life = life;
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::dayNightClock> _restoreClock;
	CreatureMindSystem _system;
};
} // namespace

TEST_F(CreatureMindSystemTest, TheDesiresStartFromTheSpeciesTables)
{
	const auto setup = mind_detail::SetupFor(CreatureType::GiantApe);
	for (size_t d = 0; d < setup.size(); ++d)
	{
		// the row's cap, then the range its decay is drawn from (docs/bw1-notes/creature.md)
		EXPECT_FLOAT_EQ(setup.at(d).max, 2.0f);
		EXPECT_FLOAT_EQ(setup.at(d).decayMin, 0.9f);
		EXPECT_FLOAT_EQ(setup.at(d).decayMax, 0.9f + (0.001f * static_cast<float>(d + 1)));
	}
	// without the tables, the defaults
	Locator::infoConstants::reset();
	EXPECT_FLOAT_EQ(mind_detail::SetupFor(CreatureType::GiantApe).front().max, 1.0f);
}

TEST_F(CreatureMindSystemTest, TheFirstTurnDrawsFortyFloatsInDesireOrder)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	std::vector<float> floats;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&floats](float x) {
		                                  floats.push_back(x);
		                                  return x * 0.5f;
	                                  });
	const auto local = game_random::Current().local;
	const auto crt = game_random::crt::Seed();

	_system.ProcessTurn();

	ASSERT_EQ(floats.size(), creature_desires::k_DesireCount);
	const auto& desires = test::creature_world::World::Registry().Get<CreatureMindState>(creature).desires;
	ASSERT_TRUE(desires.has_value());
	for (size_t d = 0; d < floats.size(); ++d)
	{
		const auto range = (0.9f + (0.001f * static_cast<float>(d + 1))) - 0.9f;
		EXPECT_FLOAT_EQ(floats.at(d), range);
		// low + the draw below high - low
		EXPECT_FLOAT_EQ(desires->desires.at(d).decay, 0.9f + (range * 0.5f));
	}
	EXPECT_EQ(game_random::Current().local, local);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	// the second turn draws them no more
	floats.clear();
	_system.ProcessTurn();
	EXPECT_TRUE(floats.empty());
}

TEST_F(CreatureMindSystemTest, ScansBreakTiesOnTheEntityAndPassOverWhatIsGoing)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto first = MakeVillager(glm::vec3(10.0f, 0.0f, 0.0f));
	const auto second = MakeVillager(glm::vec3(-10.0f, 0.0f, 0.0f));
	const auto nearest = MakeVillager(glm::vec3(0.0f, 0.0f, 5.0f));
	const auto going = MakeVillager(glm::vec3(0.0f, 0.0f, 1.0f));
	test::creature_world::World::Registry().Assign<Unavailable>(going);

	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto found = mind_detail::Gather(registry, Target::Villager, self, glm::vec2(0.0f));
	ASSERT_EQ(found.size(), 3u);
	EXPECT_EQ(found.at(0).entity, nearest);
	// as near, the lower entity first
	EXPECT_EQ(found.at(1).entity,
	          std::min(first, second, [](auto a, auto b) { return entt::to_integral(a) < entt::to_integral(b); }));
	EXPECT_FALSE(mind_detail::Accepts(registry, Target::Villager, going, self));
	EXPECT_FALSE(mind_detail::Accepts(registry, Target::Villager, self, self));
	EXPECT_TRUE(mind_detail::Accepts(registry, Target::Living, nearest, self));
}

TEST_F(CreatureMindSystemTest, AVillagerIsAliveByItsLife)
{
	const auto self = test::creature_world::World::MakeCreature();
	const auto alive = MakeVillager(glm::vec3(0.0f), 0.5f);
	const auto dead = MakeVillager(glm::vec3(0.0f), 0.0f);
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto aliveBelief = mind_detail::BeliefOf(registry, alive, self);
	const auto deadBelief = mind_detail::BeliefOf(registry, dead, self);
	ASSERT_TRUE(aliveBelief.has_value());
	ASSERT_TRUE(deadBelief.has_value());
	EXPECT_EQ(aliveBelief->type, creature_tree::belief_types::k_Villager);
	EXPECT_EQ(aliveBelief->Value(creature_tree::Attribute::Life), 1u);
	EXPECT_EQ(deadBelief->Value(creature_tree::Attribute::Life), 0u);
}

TEST_F(CreatureMindSystemTest, TheNightIsTheDayNightClocks)
{
	Locator::dayNightClock::reset();
	EXPECT_FALSE(mind_detail::IsNight());
	auto& clock = Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>().Clock();
	clock.Reset();
	EXPECT_FALSE(mind_detail::IsNight());
	clock.SetScriptTime(0.0f);
	EXPECT_TRUE(mind_detail::IsNight());
}

TEST_F(CreatureMindSystemTest, AnActionToldItsMirroringDrawsNothing)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t n) {
		    draws.push_back(n);
		    return 1u;
	    },
	    [](float) { return 0.0f; });
	EXPECT_TRUE(_system.PlayAction(creature, 60, false));
	EXPECT_TRUE(draws.empty());
	EXPECT_FALSE(test::creature_world::World::Registry().Get<CreatureAnimation>(creature).body.mirrored);
	// left to the mind, a coin is tossed on the synced stream
	test::creature_world::World::Registry().Get<CreatureAnimation>(creature).body = {};
	EXPECT_TRUE(_system.PlayAction(creature, 60));
	ASSERT_EQ(draws.size(), 1u);
	EXPECT_EQ(draws.front(), 2u);
	EXPECT_TRUE(test::creature_world::World::Registry().Get<CreatureAnimation>(creature).body.mirrored);
}
