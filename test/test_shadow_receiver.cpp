/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which objects receive the projected shadows that fall on objects (src/ECS/ShadowReceiver.h): every game object but
// the classes that turn the flag off, the bare 3D objects, and the creature, whose drawn body never asks for it.

#include <gtest/gtest.h>

#include "ECS/ShadowReceiver.h"

using openblack::PotInfo;
using openblack::ecs::shadow_receiver::Object;
using openblack::ecs::shadow_receiver::Receives;

TEST(ShadowReceiver, AnOrdinaryObjectReceives)
{
	EXPECT_TRUE(Receives({}));
	EXPECT_TRUE(Receives({.pot = PotInfo::FoodPile}));
	EXPECT_TRUE(Receives({.pot = PotInfo::HandWood}));
}

TEST(ShadowReceiver, ACreatureDoesNotReceive)
{
	EXPECT_FALSE(Receives({.creature = true}));
}

TEST(ShadowReceiver, TheClassesThatTurnTheFlagOffDoNotReceive)
{
	EXPECT_FALSE(Receives({.powerUpBand = true}));
	EXPECT_FALSE(Receives({.tree = true}));
	EXPECT_FALSE(Receives({.forest = true}));
	EXPECT_FALSE(Receives({.hand = true}));
	EXPECT_FALSE(Receives({.orb = true}));
	EXPECT_FALSE(Receives({.shield = true}));
	EXPECT_FALSE(Receives({.templePart = true}));
	EXPECT_FALSE(Receives({.handFxPart = true}));
}

TEST(ShadowReceiver, TheFoodInTheHandAndTheMagicFoodDoNotReceive)
{
	EXPECT_FALSE(Receives({.pot = PotInfo::HandFood}));
	EXPECT_FALSE(Receives({.pot = PotInfo::MagicFood}));
}

static_assert(Receives(Object {}) && !Receives(Object {.creature = true}));
