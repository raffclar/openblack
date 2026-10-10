/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Story/NewGameChoice.h"

namespace openblack::ecs::systems
{

/// The returning player's answer to the start-of-game question, which the story's scripts ask about. Cleared as each
/// new game starts, before the question is asked.
class TutorialSkipSystemInterface
{
public:
	virtual ~TutorialSkipSystemInterface() = default;

	[[nodiscard]] virtual const new_game_choice::TutorialSkip& Get() const = 0;
	virtual void Set(new_game_choice::TutorialSkip skip) = 0;
};

} // namespace openblack::ecs::systems
