/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FireSoundSystem.h"

using namespace openblack::ecs::systems;

FireSoundSystem::Slots& FireSoundSystem::GetSlots()
{
	return _slots;
}

float FireSoundSystem::MaxDistance() const
{
	return _maxDistance;
}

void FireSoundSystem::SetMaxDistance(float distance)
{
	_maxDistance = distance;
}

FireSoundSystem::Owners& FireSoundSystem::GetOwners()
{
	return _owners;
}
