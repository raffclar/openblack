/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>

#include <gtest/gtest.h>

#include "CHLApi.h"
#include "Hand/HandClickRules.h"

using namespace openblack;
using openblack::ecs::components::HandClicked;

namespace
{
constexpr uint32_t k_TurnMs = 100;
constexpr auto k_Thing = static_cast<entt::entity>(7);
constexpr auto k_Other = static_cast<entt::entity>(9);

map_coords::MapCoords At(float x, float z)
{
	return {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z)};
}
} // namespace

TEST(HandClick, AClickIsForgottenOnceMoreThanFifteenSecondsHaveGoneBy)
{
	EXPECT_FALSE(hand_click::IsForgotten(149, 0, k_TurnMs));
	// 150 turns of 100 ms comes to a hair over 15 seconds in single precision
	EXPECT_TRUE(hand_click::IsForgotten(150, 0, k_TurnMs));
	EXPECT_FALSE(hand_click::IsForgotten(1149, 1000, k_TurnMs));
	// A click from a turn yet to come is not forgotten
	EXPECT_FALSE(hand_click::IsForgotten(10, 20, k_TurnMs));
}

TEST(HandClick, ThingAndPlaceAreForgottenEachOnItsOwn)
{
	HandClicked clicked;
	hand_click::ClickPlace(clicked, At(20.0f, 30.0f), 0);
	hand_click::ClickThing(clicked, k_Thing, 100);
	hand_click::Forget(clicked, 160, k_TurnMs);
	EXPECT_EQ(clicked.thing, k_Thing);
	EXPECT_EQ(clicked.place, map_coords::MapCoords {});
	// The turn stays as it was
	EXPECT_EQ(clicked.placeTurn, 0u);
	hand_click::Forget(clicked, 250, k_TurnMs);
	EXPECT_TRUE(clicked.thing == entt::null);
}

TEST(HandClick, ARewardIsNotMarkedWhileTheLeashIsLooseInTheHand)
{
	EXPECT_TRUE(hand_click::MarksThing(false, false));
	EXPECT_TRUE(hand_click::MarksThing(true, false));
	EXPECT_TRUE(hand_click::MarksThing(false, true));
	EXPECT_FALSE(hand_click::MarksThing(true, true));
}

TEST(HandClick, LettingGoOverAThingClicksItOverTheLandClicksThePlace)
{
	HandClicked clicked;
	hand_click::ClickReleased(clicked, k_Thing, true, At(1.0f, 2.0f), 5);
	EXPECT_EQ(clicked.thing, k_Thing);
	EXPECT_EQ(clicked.thingTurn, 5u);
	// The place is left as it was
	EXPECT_EQ(clicked.place, map_coords::MapCoords {});
	// A thing that doesn't mark is the land under it, and forgets the thing clicked before
	hand_click::ClickReleased(clicked, k_Other, false, At(1.0f, 2.0f), 6);
	EXPECT_TRUE(clicked.thing == entt::null);
	EXPECT_EQ(clicked.thingTurn, 5u);
	EXPECT_EQ(clicked.place, At(1.0f, 2.0f));
	EXPECT_EQ(clicked.placeTurn, 6u);
	hand_click::ClickThing(clicked, k_Thing, 7);
	hand_click::ClickReleased(clicked, std::nullopt, false, At(3.0f, 4.0f), 8);
	EXPECT_TRUE(clicked.thing == entt::null);
	EXPECT_EQ(clicked.place, At(3.0f, 4.0f));
}

TEST(HandClick, OnlyTheThingClickedIsClicked)
{
	HandClicked clicked;
	EXPECT_FALSE(hand_click::IsThingClicked(clicked, k_Thing));
	EXPECT_FALSE(hand_click::IsThingClicked(clicked, entt::null));
	hand_click::ClickThing(clicked, k_Thing, 1);
	EXPECT_TRUE(hand_click::IsThingClicked(clicked, k_Thing));
	EXPECT_FALSE(hand_click::IsThingClicked(clicked, k_Other));
	hand_click::ClearThing(clicked);
	EXPECT_FALSE(hand_click::IsThingClicked(clicked, k_Thing));
}

TEST(HandClick, APlaceIsClickedWithinTheRadiusAlongTheGround)
{
	HandClicked clicked;
	hand_click::ClickPlace(clicked, At(100.0f, 100.0f), 1);
	EXPECT_TRUE(hand_click::IsPlaceClicked(clicked, At(103.0f, 104.0f), 5.1f));
	EXPECT_FALSE(hand_click::IsPlaceClicked(clicked, At(103.0f, 104.0f), 4.9f));
	EXPECT_FALSE(hand_click::IsPlaceClicked(clicked, At(0.0f, 0.0f), 5.0f));
	// Cleared, the place is the map's corner, which a script asking near it still finds
	hand_click::ClearPlace(clicked);
	EXPECT_TRUE(hand_click::IsPlaceClicked(clicked, At(1.0f, 1.0f), 2.0f));
}

TEST(ClickNatives, TakeAndGiveWhatTheGameDoes)
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
	     {Expected {16, "GAME_THING_CLICKED", 1, 1}, Expected {156, "CLEAR_CLICKED_OBJECT", 0, 0},
	      Expected {157, "CLEAR_CLICKED_POSITION", 0, 0}, Expected {158, "POSITION_CLICKED", 4, 1},
	      Expected {421, "GET_OBJECT_CLICKED", 0, 1}})
	{
		ASSERT_LT(index, table.size());
		EXPECT_EQ(table[index].name, std::string(name));
		EXPECT_EQ(table[index].stackIn, in) << name;
		EXPECT_EQ(table[index].stackOut, out) << name;
	}
}
