/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LocalPlayer.h"

#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;

PlayerNames creature::LocalPlayer()
{
	if (!Locator::playerSystem::has_value())
	{
		return PlayerNames::PLAYER_ONE;
	}
	return Locator::playerSystem::value().LocalPlayer();
}
