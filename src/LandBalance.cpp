/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandBalance.h"

#include <cstdio>
#include <cstdlib>

#include "ECS/Systems/LandBalanceSystemInterface.h"
#include "Locator.h"

namespace openblack::land_balance
{
namespace
{
/// The game's land balance; stops with a message when there is none (before the game or after it has gone)
ecs::systems::LandBalanceSystemInterface& Balance()
{
	if (!Locator::landBalanceSystem::has_value())
	{
		std::fputs("land_balance: no land balance in the locator (Locator::landBalanceSystem)\n", stderr);
		std::abort();
	}
	return Locator::landBalanceSystem::value();
}
} // namespace

void Reset()
{
	Balance().Reset();
}

void Set(int index, float value)
{
	Balance().Set(index, value);
}

float Get(size_t index)
{
	return Balance().Get(index);
}

void SetLostTownScale(float scale)
{
	Balance().SetLostTownScale(scale);
}

float LostTownScale()
{
	return Balance().LostTownScale();
}

} // namespace openblack::land_balance
