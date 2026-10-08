/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TreeSystem.h"

#include <utility>

using namespace openblack::ecs::systems;

uint8_t TreeSystem::Brightness() const
{
	return _brightness;
}

void TreeSystem::SetBrightness(uint8_t brightness)
{
	_brightness = brightness;
}

bool TreeSystem::AnyBent() const
{
	return _anyBent;
}

void TreeSystem::SetAnyBent(bool bent)
{
	_anyBent = bent;
}

void TreeSystem::AddDeletedListener(DeletedListener listener)
{
	_deletedListeners.push_back(std::move(listener));
}

const std::vector<TreeSystem::DeletedListener>& TreeSystem::DeletedListeners() const
{
	return _deletedListeners;
}
