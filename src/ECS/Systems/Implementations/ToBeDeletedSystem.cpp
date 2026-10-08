/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ToBeDeletedSystem.h"

#include <algorithm>

#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void ToBeDeletedSystem::Push(entt::entity entity)
{
	_deadList.insert(_deadList.begin(), DeadThing {entity, false});
}

void ToBeDeletedSystem::ProcessDead(std::size_t index, bool drain)
{
	auto& thing = _deadList[index];
	if (!thing.passed && !drain)
	{
		thing.passed = true;
		return;
	}
	const auto entity = thing.entity;
	_deadList.erase(_deadList.begin() + static_cast<std::ptrdiff_t>(index));
	// the final delete: (approximate) openblack has no per-class delete, the entity goes
	if (auto& registry = Locator::entitiesRegistry::value(); registry.Valid(entity))
	{
		registry.Destroy(entity);
		registry.SetDirty();
	}
}

void ToBeDeletedSystem::Process(bool drain)
{
	do
	{
		// from the head, the next taken before each one; things a delete marks go to the head, unseen this pass
		std::vector<entt::entity> pass;
		pass.reserve(_deadList.size());
		for (const auto& thing : _deadList)
		{
			pass.push_back(thing.entity);
		}
		for (const auto entity : pass)
		{
			const auto it = std::find_if(_deadList.begin(), _deadList.end(),
			                             [entity](const DeadThing& thing) { return thing.entity == entity; });
			if (it != _deadList.end())
			{
				ProcessDead(static_cast<std::size_t>(it - _deadList.begin()), drain);
			}
		}
	} while (drain && !_deadList.empty());
}

void ToBeDeletedSystem::SetDeferred(bool deferred)
{
	_deferred = deferred;
}

bool ToBeDeletedSystem::Deferred() const
{
	return _deferred;
}
