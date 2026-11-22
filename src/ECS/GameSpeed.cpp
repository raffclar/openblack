/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameSpeed.h"

#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::game_speed
{

void Set(float speed)
{
	auto& clock = Locator::time::value();
	if (!clock.IsNetworkGame())
	{
		clock.SetSpeed(speed);
		return;
	}
	if (Locator::networkGame::has_value())
	{
		Locator::networkGame::value().SetGameSpeed(speed);
	}
}

float Get()
{
	const auto& clock = Locator::time::value();
	if (clock.IsNetworkGame() && Locator::networkGame::has_value())
	{
		return Locator::networkGame::value().GetGameSpeed();
	}
	return clock.GetSpeed();
}

} // namespace openblack::ecs::game_speed
