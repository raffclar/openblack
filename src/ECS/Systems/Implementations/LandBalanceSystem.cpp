/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LandBalanceSystem.h"

#include <cstdio>
#include <cstdlib>

#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "LandBalance.h"
#include "Locator.h"

using namespace openblack::ecs::systems;
using openblack::ecs::components::MapScriptGlobals;

namespace
{
static_assert(std::tuple_size_v<decltype(MapScriptGlobals::landBalance)> == openblack::land_balance::k_Count);

/// The map script's globals, which hold the land balance's values; stops with a message when there are none (before
/// the game or after it has gone)
MapScriptGlobals& Globals()
{
	if (!openblack::Locator::mapScriptSystem::has_value())
	{
		std::fputs("land_balance: no map script globals in the locator (Locator::mapScriptSystem)\n", stderr);
		std::abort();
	}
	return openblack::Locator::mapScriptSystem::value().Globals();
}
} // namespace

void LandBalanceSystem::Reset()
{
	auto& globals = Globals();
	globals.landBalance.fill(1.0f);
	globals.lostTownScale = 1.0f;
}

void LandBalanceSystem::Set(int index, float value)
{
	auto& values = Globals().landBalance;
	if (index >= 0 && static_cast<std::size_t>(index) < values.size())
	{
		values.at(static_cast<std::size_t>(index)) = value;
	}
}

float LandBalanceSystem::Get(std::size_t index) const
{
	const auto& values = Globals().landBalance;
	return index < values.size() ? values.at(index) : 1.0f;
}

void LandBalanceSystem::SetLostTownScale(float scale)
{
	Globals().lostTownScale = scale;
}

float LandBalanceSystem::LostTownScale() const
{
	return Globals().lostTownScale;
}
