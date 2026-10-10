/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "DialogueControlSystem.h"

using namespace openblack::ecs::systems;

bool DialogueControlSystem::IsControlled(uint32_t wideScreenOwner) const
{
	return _owner != 0 || wideScreenOwner != 0;
}

void DialogueControlSystem::Request(uint32_t task, uint32_t wideScreenOwner)
{
	if (IsControlled(wideScreenOwner))
	{
		return;
	}
	_owner = task;
	if (_hooks.taken)
	{
		_hooks.taken();
	}
}

bool DialogueControlSystem::Release(uint32_t task, bool helpScript)
{
	if (_owner != task)
	{
		return false;
	}
	_owner = 0;
	if (_hooks.released)
	{
		_hooks.released(helpScript);
	}
	return true;
}

void DialogueControlSystem::SendSpiritsHome(bool helpScript)
{
	if (_hooks.sendSpiritsHome)
	{
		_hooks.sendSpiritsHome(helpScript);
	}
}
