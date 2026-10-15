/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Creature/CreatureEmergencies.h"

using namespace openblack;
using creature_emergencies::Building;
using creature_emergencies::OwnFire;
using creature_emergencies::TownFireFacts;

namespace
{
TownFireFacts CaringCreature()
{
	return {.knowsWater = true,
	        .seen = 4.0f,
	        .needed = 5.0f,
	        .canPay = true,
	        .compassionActivated = true,
	        .compassionSuppressed = false,
	        .compassion = 0.5f,
	        .anger = 0.2f};
}
} // namespace

TEST(CreatureEmergencies, AnyGateStopsTheEmergencies)
{
	EXPECT_TRUE(creature_emergencies::LooksAtEmergencies({}));
	EXPECT_FALSE(creature_emergencies::LooksAtEmergencies({.emergencyRunning = true}));
	EXPECT_FALSE(creature_emergencies::LooksAtEmergencies({.scripted = true}));
	EXPECT_FALSE(creature_emergencies::LooksAtEmergencies({.planForced = true}));
	EXPECT_FALSE(creature_emergencies::LooksAtEmergencies({.fighting = true}));
}

TEST(CreatureEmergencies, LifeBelowFourTenthsIsHurt)
{
	EXPECT_TRUE(creature_emergencies::IsHurt(0.39f));
	EXPECT_FALSE(creature_emergencies::IsHurt(0.4f));
	EXPECT_FALSE(creature_emergencies::IsHurt(1.0f));
}

TEST(CreatureEmergencies, OnFireItCastsWaterOnlyWithEnoughSightingsAndEnergy)
{
	EXPECT_EQ(creature_emergencies::ChooseOwnFire(4.0f, 5.0f, true), OwnFire::CastWater);
	// More sightings than it needs also count
	EXPECT_EQ(creature_emergencies::ChooseOwnFire(9.0f, 5.0f, true), OwnFire::CastWater);
	// Exactly six tenths is not enough
	EXPECT_EQ(creature_emergencies::ChooseOwnFire(3.0f, 5.0f, true), OwnFire::DrinkFromTheSea);
	EXPECT_EQ(creature_emergencies::ChooseOwnFire(4.0f, 5.0f, false), OwnFire::DrinkFromTheSea);
	// No sightings needed and none seen gives no number: it drinks
	EXPECT_EQ(creature_emergencies::ChooseOwnFire(0.0f, 0.0f, true), OwnFire::DrinkFromTheSea);
}

TEST(CreatureEmergencies, ATownFireNeedsTheMiracleAndCompassionAboveAnger)
{
	EXPECT_TRUE(creature_emergencies::PutsOutTownFire(CaringCreature()));
	auto facts = CaringCreature();
	facts.knowsWater = false;
	EXPECT_FALSE(creature_emergencies::PutsOutTownFire(facts));
	facts = CaringCreature();
	facts.seen = 3.0f;
	EXPECT_FALSE(creature_emergencies::PutsOutTownFire(facts));
	facts = CaringCreature();
	facts.seen = 50.0f;
	EXPECT_TRUE(creature_emergencies::PutsOutTownFire(facts));
	facts = CaringCreature();
	facts.canPay = false;
	EXPECT_FALSE(creature_emergencies::PutsOutTownFire(facts));
	facts = CaringCreature();
	facts.compassionActivated = false;
	EXPECT_FALSE(creature_emergencies::PutsOutTownFire(facts));
	facts = CaringCreature();
	facts.compassionSuppressed = true;
	EXPECT_FALSE(creature_emergencies::PutsOutTownFire(facts));
	// Anger equal to compassion is not less
	facts = CaringCreature();
	facts.anger = facts.compassion;
	EXPECT_FALSE(creature_emergencies::PutsOutTownFire(facts));
	facts = CaringCreature();
	facts.seen = 0.0f;
	facts.needed = 0.0f;
	EXPECT_FALSE(creature_emergencies::PutsOutTownFire(facts));
}

TEST(CreatureEmergencies, ABurningPitOrTownCentreComesFirstElseTheLastBurning)
{
	const std::array none {Building {}, Building {.comesFirst = true}};
	EXPECT_FALSE(creature_emergencies::PickBurningBuilding(none).has_value());
	const std::array plain {Building {.onFire = true}, Building {}, Building {.onFire = true}, Building {}};
	EXPECT_EQ(creature_emergencies::PickBurningBuilding(plain), 2u);
	const std::array first {Building {.onFire = true}, Building {.onFire = true, .comesFirst = true}, Building {.onFire = true},
	                        Building {.onFire = true, .comesFirst = true}};
	EXPECT_EQ(creature_emergencies::PickBurningBuilding(first), 1u);
}

TEST(CreatureEmergencies, OnlyACreatureOfNoPlayerOrOfTheComputerKeepsTheRunSpeed)
{
	EXPECT_TRUE(creature_emergencies::KeepsRunSpeed(false, false));
	EXPECT_TRUE(creature_emergencies::KeepsRunSpeed(true, true));
	EXPECT_FALSE(creature_emergencies::KeepsRunSpeed(true, false));
}
