/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <iterator>

#include <gtest/gtest.h>

#include "Graphics/TempleRoomOrder.h"

using openblack::graphics::temple_room_order::k_Order;
using openblack::graphics::temple_room_order::Step;
using openblack::graphics::temple_room_order::StepOf;

namespace
{

/// The place of a step in the order of a frame
std::ptrdiff_t PlaceOf(Step step)
{
	return std::distance(k_Order.begin(), std::ranges::find(k_Order, step));
}

} // namespace

TEST(TempleRoomOrder, TheDoorsOfTheMainRoomComeAfterTheWindowsOfTheLibrary)
{
	// From the library, the main room's mesh is only its doors. The library's walls and their clear windows are one
	// solid mesh. Its dome has only blended primitives.
	const auto mainRoomDoors = StepOf(true, false);
	const auto libraryWalls = StepOf(false, false);
	const auto libraryDome = StepOf(false, true);
	EXPECT_GT(PlaceOf(mainRoomDoors), PlaceOf(libraryWalls));
	EXPECT_GT(PlaceOf(mainRoomDoors), PlaceOf(libraryDome));
}

TEST(TempleRoomOrder, TheMainRoomWhileADoorSwingsComesAfterTheWholeSideRoom)
{
	// While a door swings, the game draws the whole main room, its floor and dome too, after the side room
	for (const bool sideRoomBlended : {false, true})
	{
		for (const bool mainRoomBlended : {false, true})
		{
			EXPECT_GT(PlaceOf(StepOf(true, mainRoomBlended)), PlaceOf(StepOf(false, sideRoomBlended)));
		}
	}
}

TEST(TempleRoomOrder, BlendedMeshesComeAfterTheSolidMeshesOfTheirRoom)
{
	EXPECT_GT(PlaceOf(StepOf(false, true)), PlaceOf(StepOf(false, false)));
	EXPECT_GT(PlaceOf(StepOf(true, true)), PlaceOf(StepOf(true, false)));
}

TEST(TempleRoomOrder, EachStepHasOnePlace)
{
	for (const auto step : {Step::CurrentRoomSolid, Step::CurrentRoomBlended, Step::OtherRoomsSolid, Step::OtherRoomsBlended})
	{
		EXPECT_EQ(std::ranges::count(k_Order, step), 1);
	}
}
