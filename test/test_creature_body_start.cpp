/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>

#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/CreaturePhysiologySystem.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// A world with a wolf whose body has not had its first turn
class CreatureBodyStart: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// The tables are large: made on the heap
		auto info = std::make_unique<InfoConstants>();
		auto& wolf = info->creature.at(creature::InfoRow(CreatureType::Wolf));
		wolf.startEnergy = 0.75f;
		wolf.startWarmth = 0.5f;
		Locator::infoConstants::emplace(*info);
		auto& registry = Locator::entitiesRegistry::emplace<Registry>();
		_wolf = registry.Create();
		registry.Assign<Creature>(_wolf, Creature {.species = CreatureType::Wolf});
		registry.Assign<CreatureNeeds>(_wolf);
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	[[nodiscard]] static CreatureNeeds& Needs(entt::entity creature)
	{
		return Locator::entitiesRegistry::value().Get<CreatureNeeds>(creature);
	}

	systems::CreaturePhysiologySystem _system;
	entt::entity _wolf {entt::null};
};

} // namespace

TEST_F(CreatureBodyStart, TheBodyTakesTheStartOfItsSpeciesOnce)
{
	ASSERT_FALSE(Needs(_wolf).started);
	_system.StartBody(_wolf);
	EXPECT_TRUE(Needs(_wolf).started);
	EXPECT_FLOAT_EQ(Needs(_wolf).needs.energy, 0.75f);
	EXPECT_FLOAT_EQ(Needs(_wolf).needs.warmth, 0.5f);
	// A value set after the start stays when the body is asked to start again, as at its first turn
	Needs(_wolf).needs.energy = 0.2f;
	Needs(_wolf).needs.warmth = 0.2f;
	_system.StartBody(_wolf);
	EXPECT_FLOAT_EQ(Needs(_wolf).needs.energy, 0.2f);
	EXPECT_FLOAT_EQ(Needs(_wolf).needs.warmth, 0.2f);
}

TEST_F(CreatureBodyStart, WhatTheFileKeptIsTakenUpAtTheStart)
{
	Needs(_wolf).kept = creature_physiology::Kept {.age = 3, .turns = 40, .energy = 0.5f, .exhaustion = 0.25f};
	_system.StartBody(_wolf);
	EXPECT_FALSE(Needs(_wolf).kept.has_value());
	EXPECT_FLOAT_EQ(Needs(_wolf).needs.energy, 0.5f);
	EXPECT_FLOAT_EQ(Needs(_wolf).needs.exhaustion, 0.25f);
	EXPECT_FLOAT_EQ(Needs(_wolf).needs.warmth, 0.5f);
	EXPECT_EQ(Needs(_wolf).needs.turns, 40u);
}
