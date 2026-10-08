/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' leash commands (Creature/LeashScript.h): which leash call each makes, with which creature and thing,
// and what the queries give, on a recording fake of the leash service

#include <cstdint>

#include <bit>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Creature/LeashKeys.h"
#include "Creature/LeashScript.h"
#include "Enums.h"
#include "support/CreatureFakes.h"

using namespace openblack;
using namespace openblack::test::creature_loop_fakes;
namespace script = openblack::creature_leash::script;

namespace
{
constexpr auto k_Creature = static_cast<entt::entity>(5);
constexpr auto k_Post = static_cast<entt::entity>(9);
constexpr auto k_Other = static_cast<entt::entity>(12);

class LeashScriptTest: public ::testing::Test
{
protected:
	/// Only k_Creature is a creature
	const script::IsCreature isCreature = [](entt::entity entity) { return entity == k_Creature; };

	CallLog log;
	FakeLeash leash {log};
};
} // namespace

TEST_F(LeashScriptTest, AttachToThingTiesTheCreatureToIt)
{
	script::AttachToThing(leash, k_Creature, k_Post, isCreature);
	EXPECT_EQ(log, (CallLog {{.name = "leash.TieTo", .args = {Id(k_Creature), Id(k_Post)}}}));
}

TEST_F(LeashScriptTest, AttachToThingTakesEitherOrder)
{
	script::AttachToThing(leash, k_Post, k_Creature, isCreature);
	EXPECT_EQ(log, (CallLog {{.name = "leash.TieTo", .args = {Id(k_Creature), Id(k_Post)}}}));
}

TEST_F(LeashScriptTest, AttachToThingNeedsACreatureAndAThing)
{
	script::AttachToThing(leash, k_Post, k_Other, isCreature);
	script::AttachToThing(leash, std::nullopt, k_Post, isCreature);
	script::AttachToThing(leash, k_Creature, std::nullopt, isCreature);
	EXPECT_TRUE(log.empty());
}

TEST_F(LeashScriptTest, AttachToHandUntiesATiedLeash)
{
	leash.leashed = true;
	leash.tiedTo = k_Post;
	script::AttachToHand(leash, k_Creature);
	EXPECT_EQ(log, (CallLog {{.name = "leash.UntieToHand", .args = {Id(k_Creature)}}}));
}

TEST_F(LeashScriptTest, AttachToHandPutsOnALeashNotWorn)
{
	script::AttachToHand(leash, k_Creature);
	EXPECT_EQ(log, (CallLog {{.name = "leash.Toggle", .args = {Id(k_Creature)}}}));
}

TEST_F(LeashScriptTest, AttachToHandLeavesALeashAlreadyHeld)
{
	leash.leashed = true;
	script::AttachToHand(leash, k_Creature);
	script::AttachToHand(leash, std::nullopt);
	EXPECT_TRUE(log.empty());
}

TEST_F(LeashScriptTest, DetachTakesTheLeashOff)
{
	script::Detach(leash, k_Creature);
	script::Detach(leash, std::nullopt);
	EXPECT_EQ(log, (CallLog {{.name = "leash.TakeOff", .args = {Id(k_Creature)}}}));
}

TEST_F(LeashScriptTest, IsLeashedAsksTheLeash)
{
	EXPECT_FALSE(script::IsLeashed(leash, k_Creature));
	leash.leashed = true;
	EXPECT_TRUE(script::IsLeashed(leash, k_Creature));
	EXPECT_FALSE(script::IsLeashed(leash, std::nullopt));
}

TEST_F(LeashScriptTest, SetWorksTakesAnyValueButZeroAsSet)
{
	script::SetWorks(leash, k_Creature, 0);
	// the script's value is passed as it is popped: a float 1 is its bits
	script::SetWorks(leash, k_Creature, std::bit_cast<int32_t>(1.0f));
	script::SetWorks(leash, std::nullopt, 1);
	EXPECT_EQ(log, (CallLog {
	                   {.name = "leash.SetWorks", .args = {Id(k_Creature), 0.0f}},
	                   {.name = "leash.SetWorks", .args = {Id(k_Creature), 1.0f}},
	               }));
}

TEST_F(LeashScriptTest, IsLeashedToThingComparesTheTie)
{
	EXPECT_FALSE(script::IsLeashedToThing(leash, k_Creature, k_Post, isCreature)) << "no tie";
	leash.leashed = true;
	leash.tiedTo = k_Post;
	EXPECT_TRUE(script::IsLeashedToThing(leash, k_Creature, k_Post, isCreature));
	EXPECT_TRUE(script::IsLeashedToThing(leash, k_Post, k_Creature, isCreature)) << "either order";
	EXPECT_FALSE(script::IsLeashedToThing(leash, k_Creature, k_Other, isCreature)) << "tied to another thing";
	EXPECT_FALSE(script::IsLeashedToThing(leash, k_Post, k_Other, isCreature)) << "no creature";
	EXPECT_FALSE(script::IsLeashedToThing(leash, k_Creature, std::nullopt, isCreature)) << "no thing";
}

TEST_F(LeashScriptTest, TypeOfNumbersThePickedLeashAsTheScriptsDo)
{
	const std::vector<std::pair<LeashType, int32_t>> cases {
	    {LeashType::None, -1},
	    {LeashType::Evil, 1},
	    {LeashType::Rope, 2},
	    {LeashType::Good, 3},
	};
	for (const auto& [type, number] : cases)
	{
		leash.picked = type;
		EXPECT_EQ(script::TypeOf(leash, k_Creature), number) << number;
	}
}

TEST_F(LeashScriptTest, TypeOfReportsALeashPickedButNotWorn)
{
	leash.leashed = false;
	leash.type = LeashType::None;
	leash.picked = LeashType::Rope;
	EXPECT_EQ(script::TypeOf(leash, k_Creature), 2);
}

TEST_F(LeashScriptTest, TypeOfNoCreatureIsZero)
{
	leash.picked = LeashType::Rope;
	EXPECT_EQ(script::TypeOf(leash, std::nullopt), 0);
}

TEST_F(LeashScriptTest, TogglePressesTheLeashKeyForTheScriptsPlayer)
{
	// script player n is game player n - 1, and 0 is the neutral player
	EXPECT_TRUE(script::Toggle(leash, 1));
	EXPECT_TRUE(script::Toggle(leash, 0));
	const auto leashKey = static_cast<float>(creature_leash::LeashKey::Leash);
	EXPECT_EQ(log, (CallLog {
	                   {.name = "leash.PressKey", .args = {static_cast<float>(PlayerNames::PLAYER_ONE), leashKey}},
	                   {.name = "leash.PressKey", .args = {static_cast<float>(PlayerNames::NEUTRAL), leashKey}},
	               }));
}

TEST_F(LeashScriptTest, ToggleOutOfTheScriptsPlayersDoesNothing)
{
	EXPECT_FALSE(script::Toggle(leash, -1));
	EXPECT_FALSE(script::Toggle(leash, 9));
	EXPECT_TRUE(log.empty());
}
