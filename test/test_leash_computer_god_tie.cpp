/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "Locator.h"

namespace
{

using namespace openblack::ecs::components;
using openblack::LeashType;
using openblack::Locator;
using openblack::PlayerNames;
using openblack::ecs::systems::LeashSystem;

/// A creature of a computer god and a thing for its leash, with no other systems
class LeashComputerGodTie: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The leash writes to the game log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_creature = registry.Create();
		registry.Assign<Transform>(_creature, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Creature>(_creature, Creature {.owner = PlayerNames::PLAYER_TWO, .leashable = true});
		registry.Assign<CreatureLeash>(_creature);
		_thing = registry.Create();
		registry.Assign<Transform>(_thing, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	[[nodiscard]] const CreatureLeash& Leash() const
	{
		return Locator::entitiesRegistry::value().Get<const CreatureLeash>(_creature);
	}

	LeashSystem _leashes;
	entt::entity _creature {entt::null};
	entt::entity _thing {entt::null};
};

TEST_F(LeashComputerGodTie, NoLeashGoesOnACreatureThatDoesNotKnowTheLearningLeash)
{
	auto& record = Locator::entitiesRegistry::value().Get<CreatureLeash>(_creature);
	record.lastTiedTurn = 77;
	record.confinementRadius = 30.0f;
	EXPECT_FALSE(_leashes.ComputerGodTie(_creature, _thing));
	EXPECT_FALSE(Leash().worn.has_value());
	// The tie still occurs without the leash: the turn of the tie changes and the creature is free of its home
	EXPECT_EQ(Leash().lastTiedTurn, 0U);
	EXPECT_EQ(Leash().confinementRadius, 0.0f);
}

TEST_F(LeashComputerGodTie, TheLeashHoldsOnACreatureThatKnowsTheLearningLeash)
{
	for (const auto type : openblack::creature_leash::k_Types)
	{
		_leashes.SetKnown(_creature, type, true);
	}
	EXPECT_TRUE(_leashes.ComputerGodTie(_creature, _thing));
	ASSERT_TRUE(Leash().worn.has_value());
	EXPECT_EQ(Leash().worn->tiedTo, _thing);
	EXPECT_EQ(Leash().worn->holder, PlayerNames::PLAYER_TWO);
}

TEST_F(LeashComputerGodTie, TheHandOfAComputerGodPicksALeashThatTheCreatureDoesNotKnow)
{
	EXPECT_FALSE(_leashes.ChangeType(_creature, LeashType::Evil));
	EXPECT_EQ(Leash().selected, LeashType::Rope);
	EXPECT_TRUE(_leashes.ComputerGodChangeType(_creature, LeashType::Evil));
	EXPECT_EQ(Leash().selected, LeashType::Evil);
}

} // namespace
