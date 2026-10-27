/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "JoinOnlineEntry.h"

namespace openblack::join_online_entry
{

Decision Decide(const Situation& situation) noexcept
{
	// The player is in the front end or in a network game already
	if (situation.frontEndOpen || situation.networkGame)
	{
		return {};
	}
	if (situation.boxToAnswer)
	{
		return {.outcome = Outcome::Wait, .holdLand = false, .askQuestionAfter = false};
	}
	return {
	    .outcome = Outcome::Open,
	    .holdLand = situation.scriptHoldsCinemaBars,
	    .askQuestionAfter = situation.startQuestionOpen,
	};
}

bool FrontEndOnStart(const FirstStart& start) noexcept
{
	if (!start.multiplayerSwitch)
	{
		return false;
	}
	return start.hadProfile || (start.browser && start.hasProfile);
}

} // namespace openblack::join_online_entry
