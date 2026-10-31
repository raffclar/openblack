/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ThingsMadeSystem.h"

#include <utility>

using namespace openblack::ecs::systems;

ThingsMadeSystem::ThingsMadeSystem(std::function<uint32_t&()> count)
    : _count(std::move(count))
{
}

uint32_t ThingsMadeSystem::TakeNext()
{
	// The count wraps as the game's whole numbers do
	return _count()++;
}

uint32_t ThingsMadeSystem::Made() const
{
	return _count();
}
