/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureMindLogSystem.h"

#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/CreatureMind.h"
#include "ECS/Registry.h"

using namespace openblack::ecs::systems;
using openblack::ecs::components::CreatureMindState;

void CreatureMindLogSystem::Watch(entt::entity creature, bool watch)
{
	if (watch)
	{
		_traces.try_emplace(creature);
	}
	else
	{
		_traces.erase(creature);
	}
}

bool CreatureMindLogSystem::IsWatched(entt::entity creature) const
{
	return _traces.contains(creature);
}

void CreatureMindLogSystem::TurnEnded(const Registry& registry, uint32_t turn)
{
	if (_traces.empty())
	{
		return;
	}
	std::vector<entt::entity> gone;
	for (auto& [creature, trace] : _traces)
	{
		const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
		if (mind == nullptr)
		{
			gone.push_back(creature);
			continue;
		}
		// A mind that has not thought yet has no desires to record
		if (mind->desires.has_value())
		{
			trace.Record(turn, *mind->desires, mind->planner, mind->planForced);
		}
	}
	for (const auto creature : gone)
	{
		_traces.erase(creature);
	}
}

const openblack::creature_mind_view::Trace* CreatureMindLogSystem::Find(entt::entity creature) const
{
	const auto found = _traces.find(creature);
	return found != _traces.end() ? &found->second : nullptr;
}

void CreatureMindLogSystem::Clear()
{
	_traces.clear();
}
