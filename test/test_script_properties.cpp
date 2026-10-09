/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <limits>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "CHLApi.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

using namespace openblack;
using namespace openblack::script::property_rules;

namespace
{
constexpr auto k_Thing = static_cast<entt::entity>(7);
constexpr auto k_Other = static_cast<entt::entity>(9);
} // namespace

TEST(ScriptProperties, ATownIsDestroyedWhenNoneOfItsBuildingsStands)
{
	EXPECT_TRUE(TownCompletelyDestroyed({}));
	const std::array standing {TownBuilding {.life = 0.5f, .field = false, .built = 1.0f}};
	EXPECT_FALSE(TownCompletelyDestroyed(standing));
	// Dead, a field, or no more than a tenth built
	const std::array fallen {TownBuilding {.life = 0.0f, .field = false, .built = 1.0f},
	                         TownBuilding {.life = 1.0f, .field = true, .built = 1.0f},
	                         TownBuilding {.life = 1.0f, .field = false, .built = 0.1f}};
	EXPECT_TRUE(TownCompletelyDestroyed(fallen));
	const std::array begun {TownBuilding {.life = 1.0f, .field = false, .built = 0.11f}};
	EXPECT_FALSE(TownCompletelyDestroyed(begun));
}

TEST(ScriptProperties, PlayersAreNumberedFromOne)
{
	EXPECT_EQ(PlayerProperty(PlayerNames::PLAYER_ONE, false), 1.0f);
	EXPECT_EQ(PlayerProperty(PlayerNames::PLAYER_THREE, false), 3.0f);
	EXPECT_EQ(PlayerProperty(PlayerNames::NEUTRAL, false), 0.0f);
	EXPECT_EQ(PlayerProperty(std::nullopt, false), 0.0f);
	// A wholly destroyed town answers 1 whoever it belonged to
	EXPECT_EQ(PlayerProperty(PlayerNames::PLAYER_TWO, true), 1.0f);
	EXPECT_EQ(PlayerProperty(PlayerNames::NEUTRAL, true), 1.0f);
}

TEST(ScriptProperties, ACreatureHasInItsHandWhatItCarriesOrEats)
{
	EXPECT_TRUE(InCreatureHand(k_Thing, k_Thing, std::nullopt));
	EXPECT_TRUE(InCreatureHand(k_Thing, entt::null, k_Thing));
	EXPECT_FALSE(InCreatureHand(k_Thing, k_Other, k_Other));
	EXPECT_FALSE(InCreatureHand(k_Thing, entt::null, std::nullopt));
	EXPECT_FALSE(InCreatureHand(entt::null, entt::null, std::nullopt));
}

TEST(ScriptProperties, WarmthAndEnergyAreKeptInTheirRanges)
{
	EXPECT_EQ(SetNeed(CreatureNeed::Warmth, -3.0f), -1.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Warmth, 2.0f), 1.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Warmth, -0.5f), -0.5f);
	EXPECT_EQ(SetNeed(CreatureNeed::Energy, -0.5f), 0.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Energy, 1.5f), 1.0f);
	// Not a number becomes the top of the range
	EXPECT_EQ(SetNeed(CreatureNeed::Energy, std::numeric_limits<float>::quiet_NaN()), 1.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Exhaustion, 3.0f), 3.0f);
	EXPECT_EQ(SetNeed(CreatureNeed::Poo, -2.0f), -2.0f);
}

TEST(ScriptProperties, PropertyNumbersMatchTheScripts)
{
	using script::ObjectPropertyType;
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::InHand), 9);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::Speed), 11);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::Player), 21);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::BuiltPercentage), 22);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::YPos), 24);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::CreatureWarmth), 26);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::CreatureExhaustion), 31);
	EXPECT_EQ(static_cast<int>(ObjectPropertyType::CreatureDehydration), 32);
}

TEST(ScriptProperties, NativesTakeAndGiveWhatTheGameDoes)
{
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	struct Expected
	{
		size_t index;
		const char* name;
		int32_t in;
		uint32_t out;
	};
	// Each native's place in the table, and how many values it takes from the stack and gives back
	for (const auto& [index, name, in, out] :
	     {Expected {11, "GAME_THING_FIELD_OF_VIEW", 1, 1}, Expected {12, "POS_FIELD_OF_VIEW", 3, 1},
	      Expected {21, "GET_PROPERTY", 2, 1}, Expected {22, "SET_PROPERTY", 3, 0}, Expected {26, "CALL", 6, 1},
	      Expected {51, "CALL_NEAR", 7, 1}, Expected {76, "IN_CREATURE_HAND", 2, 1}})
	{
		ASSERT_LT(index, table.size());
		EXPECT_EQ(table[index].name, std::string(name));
		EXPECT_EQ(table[index].stackIn, in) << name;
		EXPECT_EQ(table[index].stackOut, out) << name;
	}
}
