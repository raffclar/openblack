/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectCreationIndex.h"

#include <cstdio>
#include <cstdlib>

#include "ECS/Registry.h"
#include "ECS/Systems/ObjectCreationIndexSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::object_index
{
namespace
{
/// The game's creation counter; stops with a message when there is none (before the game or after it has gone)
systems::ObjectCreationIndexSystemInterface& Counter()
{
	if (!Locator::objectCreationIndexSystem::has_value())
	{
		std::fputs("ecs::object_index: no creation counter (Locator::objectCreationIndexSystem)\n", stderr);
		std::abort();
	}
	return Locator::objectCreationIndexSystem::value();
}
} // namespace

void OnLoadMap()
{
	Counter().OnLoadMap();
}

void Assign(entt::entity entity)
{
	const uint32_t value = Counter().Next();
	Locator::entitiesRegistry::value().AssignOrReplace<components::ObjectCreationIndex>(entity, value);
}

void Skip(uint32_t count)
{
	Counter().Skip(count);
}

int64_t Of(entt::entity entity)
{
	const auto* index = Locator::entitiesRegistry::value().TryGet<const components::ObjectCreationIndex>(entity);
	return index != nullptr ? static_cast<int64_t>(index->value) : -1;
}

void AddTownSpell(uint32_t town, const std::string& spell)
{
	Counter().AddTownSpell(town, spell);
}

void OnTownCentre(uint32_t town)
{
	Counter().OnTownCentre(town);
}

} // namespace openblack::ecs::object_index
