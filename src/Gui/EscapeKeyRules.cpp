/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EscapeKeyRules.h"

namespace openblack::escape_key
{

bool MenuMayOpen(std::optional<std::chrono::milliseconds> sinceMenuClosed) noexcept
{
	// The full 300 ms must have gone by
	return !sinceMenuClosed.has_value() || *sinceMenuClosed > k_MenuReopenGuard ||
	       *sinceMenuClosed < std::chrono::milliseconds::zero();
}

Response Respond(const Situation& situation) noexcept
{
	// On the Gods' Playground, Escape never opens the menu nor stops the help scripts. Outside a cut scene and with the
	// screen clear, it ends a running lesson, or else asks whether to leave
	if (situation.inGodsPlayground)
	{
		if (situation.scriptHoldsCinemaBars || !situation.screenClear)
		{
			return {};
		}
		return {
		    .outcome = situation.playgroundLessonRunning ? Outcome::EndPlaygroundLesson : Outcome::AskLeavePlayground,
		    .stopHelpScripts = false,
		    .clearScreenFade = true,
		};
	}
	if (situation.shiftOrCtrl || situation.scriptHoldsCinemaBars)
	{
		return {};
	}
	// A help script holding the dialogue stops, even when the menu then stays shut
	return {
	    .outcome = MenuMayOpen(situation.sinceMenuClosed) ? Outcome::OpenMenu : Outcome::Nothing,
	    .stopHelpScripts = situation.helpScriptHoldsDialogue,
	    .clearScreenFade = false,
	};
}

std::string_view WhyMenuStaysShut(const Situation& situation) noexcept
{
	if (situation.inGodsPlayground)
	{
		return "on the Gods' Playground, Escape does not open the menu";
	}
	if (situation.shiftOrCtrl)
	{
		return "Shift or Ctrl is held, and Escape does nothing then";
	}
	if (situation.scriptHoldsCinemaBars)
	{
		return "a script has the cinema bars in for its cut scene, and Escape does nothing then; load a scenario or "
		       "wait for the bars to go";
	}
	if (!MenuMayOpen(situation.sinceMenuClosed))
	{
		return "the menu closed less than 300 ms ago; try again after that time";
	}
	return {};
}

} // namespace openblack::escape_key
