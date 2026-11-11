/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureListOrder.h"

#include <algorithm>
#include <iterator>

#include "ECS/Components/Creature.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/Registry.h"
#include "LivingTurnOrder.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::vector<entt::entity> ecs::creature_list::NewestFirst(const Registry& registry)
{
	std::vector<living_turn::Living<entt::entity>> list;
	registry.Each<const Creature>([&registry, &list](entt::entity entity, const Creature& /*creature*/) {
		const auto* place = registry.TryGet<const LivingListPlace>(entity);
		list.push_back({.id = entity, .made = place != nullptr ? place->made : 0, .kind = living_turn::Kind::Creature});
	});
	living_turn::NewestFirst(list);
	std::vector<entt::entity> creatures;
	creatures.reserve(list.size());
	std::ranges::transform(list, std::back_inserter(creatures), &living_turn::Living<entt::entity>::id);
	return creatures;
}
