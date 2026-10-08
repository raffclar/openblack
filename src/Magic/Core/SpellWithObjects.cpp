/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellWithObjects.h"

#include <algorithm>

#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"
#include "Spell.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
SpellObjects& ListOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* list = registry.TryGet<SpellObjects>(spell); list != nullptr)
	{
		return *list;
	}
	return registry.Assign<SpellObjects>(spell);
}
} // namespace

void spell_objects::Add(entt::entity spell, entt::entity object)
{
	auto& list = ListOf(spell).objects;
	list.insert(list.begin(), object);
}

void spell_objects::Remove(entt::entity spell, entt::entity object)
{
	std::erase(ListOf(spell).objects, object);
}

const std::vector<entt::entity>& spell_objects::Objects(entt::entity spell)
{
	return ListOf(spell).objects;
}

bool spell_objects::ProcessObjectsAndRemoveDeleted(entt::entity spell)
{
	auto& list = ListOf(spell).objects;
	std::erase_if(list, [](entt::entity object) { return !ecs::IsAvailable(object); });
	return !list.empty();
}

int spell_objects::Process(entt::entity spell)
{
	base::CoreProcess(spell);
	if (ProcessObjectsAndRemoveDeleted(spell))
	{
		return 1;
	}
	return Locator::entitiesRegistry::value().Get<Spell>(spell).psys != 0 ? 1 : 5;
}
