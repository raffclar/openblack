/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameSession.h"

#include "Common/PlayerProfiles.h"

namespace openblack::game_session
{

bool SameProfileName(std::u16string_view a, std::u16string_view b)
{
	return player_profiles::SameName(a, b);
}

ExitSave ExitSaveFor(const ExitSaveFacts& facts)
{
	if (facts.gameOver || facts.multiplayer || facts.skirmish || facts.end == End::SavedGame || facts.godsPlayground)
	{
		return ExitSave::None;
	}
	// With no previous profile (an empty name is none), the save goes to the profile played. A restart as the same profile is
	// the only case with a previous profile that is the same and no save.
	if (!facts.previousProfile.has_value() || facts.previousProfile->empty() ||
	    (facts.end != End::Continue && SameProfileName(facts.currentProfile, *facts.previousProfile)))
	{
		return ExitSave::Current;
	}
	if (facts.previousExists && !SameProfileName(facts.currentProfile, *facts.previousProfile))
	{
		return ExitSave::Previous;
	}
	return ExitSave::None;
}

} // namespace openblack::game_session
