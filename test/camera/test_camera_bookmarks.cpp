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

#include "Camera/CameraBookmarks.h"

using namespace openblack::camera_bookmarks;

namespace
{

/// The slots of the number keys that are set, from a list
std::array<bool, k_SteppedSlots> SetSlots(std::initializer_list<size_t> slots)
{
	std::array<bool, k_SteppedSlots> set {};
	for (const auto slot : slots)
	{
		set.at(slot) = true;
	}
	return set;
}

} // namespace

TEST(CameraBookmarks, SevenKeysAndTheirMarkers)
{
	EXPECT_EQ(k_KeySlots, 7U);
	const auto place = MarkerPlace({10.0f, 5.0f, -3.0f});
	EXPECT_EQ(place.x, 10.0f);
	EXPECT_EQ(place.y, 7.0f);
	EXPECT_EQ(place.z, -3.0f);
}

TEST(CameraBookmarks, TheThrobClockGoesRoundEveryTwoSeconds)
{
	EXPECT_EQ(StepPulseClock(0, 16), 16);
	EXPECT_EQ(StepPulseClock(1990, 10), 2000);
	EXPECT_EQ(StepPulseClock(1990, 11), 1);
	EXPECT_EQ(StepPulseClock(0, 4500), 500);
	// Widest at the turn of the clock, narrowest a half second on, the height a fifth of a second behind
	EXPECT_NEAR(MarkerHalfSize(0).x, 1.2f, 1e-6f);
	EXPECT_NEAR(MarkerHalfSize(500).x, 0.2f, 1e-6f);
	EXPECT_NEAR(MarkerHalfSize(300).y, 0.2f, 1e-6f);
	EXPECT_NEAR(MarkerHalfSize(1800).y, 1.2f, 1e-6f);
}

TEST(CameraBookmarks, CtrlTabStepsToTheNextSetSlot)
{
	EXPECT_EQ(NextSet(0, SetSlots({2, 5})), 2U);
	EXPECT_EQ(NextSet(2, SetSlots({2, 5})), 5U);
	EXPECT_EQ(NextSet(5, SetSlots({2, 5})), 2U);
	// The search goes round to the current slot when no other slot is set
	EXPECT_EQ(NextSet(3, SetSlots({3})), 3U);
	// After the go-back key the steps start from its slot, past the end of the key slots
	EXPECT_EQ(NextSet(k_BackSlot, SetSlots({4})), 4U);
	EXPECT_EQ(NextSet(0, SetSlots({})), std::nullopt);
}

TEST(CameraBookmarks, ShiftTabStepsToTheLastSetSlot)
{
	EXPECT_EQ(PreviousSet(5, SetSlots({2, 5})), 2U);
	EXPECT_EQ(PreviousSet(2, SetSlots({2, 5})), 5U);
	EXPECT_EQ(PreviousSet(k_BackSlot, SetSlots({1, 6})), 6U);
	// The search never comes back to the current slot
	EXPECT_EQ(PreviousSet(3, SetSlots({3})), std::nullopt);
}

TEST(CameraBookmarks, ShiftAndANumberFollowsABookmarkOnAThing)
{
	EXPECT_EQ(ShiftKey(false, std::nullopt, 2), FollowKey::Jump);
	EXPECT_EQ(ShiftKey(false, 2U, 2), FollowKey::Jump);
	EXPECT_EQ(ShiftKey(true, std::nullopt, 2), FollowKey::Follow);
	EXPECT_EQ(ShiftKey(true, 4U, 2), FollowKey::Follow);
	EXPECT_EQ(ShiftKey(true, 2U, 2), FollowKey::Stop);
}

TEST(CameraBookmarks, TabFliesToTheTownBeforeTheLastTownOfTheKey)
{
	const std::vector<TownStop> towns = {{.id = 4, .hasPlace = true}, {.id = 7, .hasPlace = true}, {.id = 2, .hasPlace = true}};
	// The first press takes the town before the last town
	EXPECT_EQ(NextTown(towns, std::nullopt), 1U);
	// Each press takes the town before the town of the last press, and the first town goes round to the last
	EXPECT_EQ(NextTown(towns, 7U), 0U);
	EXPECT_EQ(NextTown(towns, 4U), 2U);
	EXPECT_EQ(NextTown(towns, 2U), 1U);
	// A town that is no longer the player's starts the search again from the last town
	EXPECT_EQ(NextTown(towns, 9U), 1U);
	// One town flies to itself
	EXPECT_EQ(NextTown(std::vector<TownStop> {{.id = 3, .hasPlace = true}}, 3U), 0U);
	EXPECT_EQ(NextTown(std::vector<TownStop> {}, std::nullopt), std::nullopt);
}

TEST(CameraBookmarks, TabPassesOverATownWithNoPlace)
{
	const std::vector<TownStop> towns = {
	    {.id = 4, .hasPlace = true}, {.id = 7, .hasPlace = false}, {.id = 2, .hasPlace = true}};
	// The town before the town of the last press has no place. No other town is the town of the last press, so the
	// second search starts at the last town and takes the first town.
	EXPECT_EQ(NextTown(towns, 2U), 0U);
	EXPECT_EQ(NextTown(towns, std::nullopt), 0U);
	// Before the first town, the last town has no place: the second search takes the first town again
	const std::vector<TownStop> onlyFirst = {
	    {.id = 4, .hasPlace = true}, {.id = 7, .hasPlace = false}, {.id = 2, .hasPlace = false}};
	EXPECT_EQ(NextTown(onlyFirst, 7U), 0U);
	EXPECT_EQ(NextTown(onlyFirst, 4U), 0U);
	const std::vector<TownStop> none = {{.id = 4, .hasPlace = false}, {.id = 7, .hasPlace = false}};
	EXPECT_EQ(NextTown(none, 4U), std::nullopt);
}

TEST(CameraBookmarks, TabFliesFromAFixedPlaceAboveAndBehindTheTown)
{
	const auto from = TownViewFrom({100.0f, 10.0f, 200.0f});
	EXPECT_EQ(from.x, 100.0f - k_TownViewX);
	EXPECT_EQ(from.y, 10.0f + k_TownViewY);
	EXPECT_EQ(from.z, 200.0f + k_TownViewZ);
	EXPECT_EQ(k_TownViewX, 24.45f);
	EXPECT_EQ(k_TownViewZ, 47.4f);
}
