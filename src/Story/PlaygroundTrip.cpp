/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlaygroundTrip.h"

namespace openblack::playground_trip
{

bool MayAskToGo(const GoSituation& situation) noexcept
{
	return !situation.skirmish && !situation.networkGame && !situation.inTemple && situation.land != k_PlaygroundLand &&
	       !situation.boxUp && !situation.scriptHoldsCinemaBars;
}

bool MenuLetsAsk(const MenuSituation& menu) noexcept
{
	if (menu.questionUp)
	{
		return false;
	}
	switch (menu.box)
	{
	case MenuBox::Main:
		// Escape does not close the main menu, but F2 can ask over it
		return true;
	case MenuBox::Options:
	case MenuBox::Advanced:
		// Escape cannot close these pages until the game tells the player about the change of the detail
		return !menu.detailUntold;
	case MenuBox::Players:
		// Escape cannot close the page while the game must start again
		return !menu.restartPending;
	case MenuBox::Controls:
	case MenuBox::Statistics:
	case MenuBox::Conditions:
		return true;
	}
	return false;
}

bool OtherBoxRefuses(input::box_keyboard::Boxes boxes) noexcept
{
	boxes.menu = false;
	boxes.landInfoBox = false;
	return input::box_keyboard::WantsKeys(boxes);
}

bool PausesWhenBack(MenuBox box) noexcept
{
	return box == MenuBox::Main;
}

void StartLesson(Lessons& lessons) noexcept
{
	lessons.running = true;
}

bool AskEnd(Lessons& lessons) noexcept
{
	if (!lessons.running)
	{
		return false;
	}
	lessons.endAsked = true;
	return true;
}

bool TakeEnd(Lessons& lessons) noexcept
{
	if (!lessons.endAsked)
	{
		return false;
	}
	lessons = {};
	return true;
}

void Leave(Lessons& lessons) noexcept
{
	// The saved game that loads next has no lesson and no end that Escape asked for
	lessons = {};
}

} // namespace openblack::playground_trip
