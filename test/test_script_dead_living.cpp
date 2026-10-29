/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ScriptHeaders/ScriptDeadLiving.h"

using openblack::script::dead_living::IsDead;
using openblack::script::dead_living::k_DeadState;
using openblack::script::dead_living::k_DyingState;
using openblack::script::dead_living::k_SetDyingState;
using Facts = openblack::script::dead_living::LivingFacts;

namespace
{
constexpr uint8_t k_Walking = 1;
}

// A creature or a thing that does not live is never dead to the search, whatever it shows
TEST(ScriptDeadLiving, OnlyVillagersAndAnimalsCanBeDead)
{
	EXPECT_FALSE(IsDead({.kind = Facts::Kind::Other, .deathMark = true, .topState = k_DeadState, .life = 0.0f}));
}

// The death mark makes a villager or an animal dead in any state
TEST(ScriptDeadLiving, TheDeathMarkMakesAThingDead)
{
	EXPECT_TRUE(IsDead({.kind = Facts::Kind::Villager, .deathMark = true, .topState = k_Walking}));
	EXPECT_TRUE(IsDead({.kind = Facts::Kind::Animal, .deathMark = true, .topState = k_Walking, .life = 1.0f}));
}

// A villager is dead from the first state of a death, and when its state goes to the dying state
TEST(ScriptDeadLiving, AVillagerIsDeadInTheStatesOfADeath)
{
	EXPECT_FALSE(IsDead({.kind = Facts::Kind::Villager, .topState = k_Walking, .finalState = k_Walking}));
	EXPECT_TRUE(IsDead({.kind = Facts::Kind::Villager, .topState = k_SetDyingState}));
	EXPECT_TRUE(IsDead({.kind = Facts::Kind::Villager, .topState = k_DyingState}));
	EXPECT_TRUE(IsDead({.kind = Facts::Kind::Villager, .topState = k_DeadState}));
	EXPECT_TRUE(IsDead({.kind = Facts::Kind::Villager, .topState = k_Walking, .finalState = k_DyingState}));
	// A villager with no life left is not dead before its death starts
	EXPECT_FALSE(IsDead({.kind = Facts::Kind::Villager, .topState = k_Walking, .life = 0.0f}));
}

// An animal is dead in the dead state or with no life, but not in the states before the dead state
TEST(ScriptDeadLiving, AnAnimalIsDeadWithNoLife)
{
	EXPECT_TRUE(IsDead({.kind = Facts::Kind::Animal, .topState = k_DeadState, .life = 1.0f}));
	EXPECT_TRUE(IsDead({.kind = Facts::Kind::Animal, .topState = k_Walking, .life = 0.0f}));
	EXPECT_FALSE(IsDead({.kind = Facts::Kind::Animal, .topState = k_DyingState, .life = 0.5f}));
	// Only exactly no life counts
	EXPECT_FALSE(IsDead({.kind = Facts::Kind::Animal, .topState = k_Walking, .life = 1e-7f}));
}
