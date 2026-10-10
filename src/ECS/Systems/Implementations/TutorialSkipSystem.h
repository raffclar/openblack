/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TutorialSkipSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class TutorialSkipSystem final: public TutorialSkipSystemInterface
{
public:
	[[nodiscard]] const new_game_choice::TutorialSkip& Get() const final { return _skip; }
	void Set(new_game_choice::TutorialSkip skip) final { _skip = skip; }

private:
	new_game_choice::TutorialSkip _skip;
};

} // namespace openblack::ecs::systems
