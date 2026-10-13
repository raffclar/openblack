/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/VillagerHidingRules.h"

using namespace openblack;
namespace hiding = openblack::ecs::villager_hiding;

namespace
{
/// Random numbers handed out in order, recording each limit asked for
struct ScriptedRandom
{
	std::vector<uint32_t> values;
	std::vector<uint32_t> limits;
	size_t next {0};
	uint32_t operator()(uint32_t limit)
	{
		limits.push_back(limit);
		return values.at(next++);
	}
};
} // namespace

TEST(VillagerHiding, OnlyWorkingBuildingsWithRoomInsideHideAVillager)
{
	for (const auto type :
	     {AbodeType::Field, AbodeType::FootballPitch, AbodeType::Wonder, AbodeType::TownCentre, AbodeType::Graveyard})
	{
		EXPECT_FALSE(hiding::CanBeHiddenIn(type, true));
	}
	for (const auto type : {AbodeType::LivingQuarters, AbodeType::Windmill, AbodeType::Totem, AbodeType::StoragePit,
	                        AbodeType::Creche, AbodeType::Workshop, AbodeType::Citadel, AbodeType::SpellDispenser})
	{
		EXPECT_TRUE(hiding::CanBeHiddenIn(type, true));
		EXPECT_FALSE(hiding::CanBeHiddenIn(type, false));
	}
}

TEST(VillagerHiding, ItRunsToTheNearestShelterWithinAHundredMetres)
{
	const std::array<hiding::Shelter, 4> shelters {{
	    {.distance = 5.0f, .canBeHiddenIn = false},
	    {.distance = 40.0f, .canBeHiddenIn = true},
	    {.distance = 20.0f, .canBeHiddenIn = true},
	    {.distance = 20.0f, .canBeHiddenIn = true},
	}};
	// The nearer of two equally near is the first in the town's list
	EXPECT_EQ(hiding::NearestShelter(shelters), 2u);

	const std::array<hiding::Shelter, 1> far {{{.distance = 100.0f, .canBeHiddenIn = true}}};
	EXPECT_FALSE(hiding::NearestShelter(far).has_value());
	const std::array<hiding::Shelter, 1> near {{{.distance = 99.9f, .canBeHiddenIn = true}}};
	EXPECT_EQ(hiding::NearestShelter(near), 0u);
	EXPECT_FALSE(hiding::NearestShelter({}).has_value());
}

TEST(VillagerHiding, InsideItCountsTurnsAgainstAFreshRollEachTurn)
{
	ScriptedRandom random {.values = {0, 149}};
	auto roll = [&](uint32_t limit) { return random(limit); };
	// 151 turns beat a roll of 0 (150 + 0): it comes out, its count starting again
	auto step = hiding::HideStep({.shelterStands = true, .atDoor = true, .threatNear = false, .turnsInside = 150}, roll);
	EXPECT_EQ(step.what, hiding::Hiding::ComeOut);
	EXPECT_EQ(step.turnsInside, 0u);
	// 151 turns don't beat 150 + 149
	step = hiding::HideStep({.shelterStands = true, .atDoor = true, .threatNear = false, .turnsInside = 150}, roll);
	EXPECT_EQ(step.what, hiding::Hiding::Stay);
	EXPECT_EQ(step.turnsInside, 151u);
	EXPECT_EQ(random.limits, (std::vector<uint32_t> {150, 150}));
}

TEST(VillagerHiding, ItGivesUpWalksBackOrWaitsWithoutRolling)
{
	ScriptedRandom random;
	auto roll = [&](uint32_t limit) { return random(limit); };
	EXPECT_EQ(hiding::HideStep({.shelterStands = false, .atDoor = true, .threatNear = false, .turnsInside = 3}, roll).what,
	          hiding::Hiding::GiveUp);
	EXPECT_EQ(hiding::HideStep({.shelterStands = true, .atDoor = false, .threatNear = true, .turnsInside = 3}, roll).what,
	          hiding::Hiding::WalkToDoor);
	const auto wait = hiding::HideStep({.shelterStands = true, .atDoor = true, .threatNear = true, .turnsInside = 3}, roll);
	EXPECT_EQ(wait.what, hiding::Hiding::StayWhileThreatNear);
	EXPECT_EQ(wait.turnsInside, 3u);
	EXPECT_TRUE(random.limits.empty());
}

TEST(VillagerHiding, OutsideItLooksAboutUntilItsTurnsBeatAFreshRoll)
{
	ScriptedRandom random {.values = {24, 0}};
	auto roll = [&](uint32_t limit) { return random(limit); };
	auto step = hiding::LookAbout(25, roll);
	EXPECT_FALSE(step.safe);
	EXPECT_EQ(step.turns, 26u);
	step = hiding::LookAbout(25, roll);
	EXPECT_TRUE(step.safe);
	EXPECT_EQ(random.limits, (std::vector<uint32_t> {25, 25}));
}

TEST(VillagerHiding, APanickingVillagerLooksBackAtTheQuickestTurn)
{
	EXPECT_EQ(hiding::k_PanicLookStep, 0x100);
}
