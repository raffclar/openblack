/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Components/FishFarm.h"
#include "ECS/ListOrder.h"
#include "ECS/Registry.h"

/// The order of the fish farms of a town. The game puts each new fish farm at the front of the list of its town. A farm
/// that goes does not change the order of the others. Thus the list keeps the newest farm first.
namespace openblack::ecs::fish_farm_order
{

/// The fish farms of this town, the newest first
[[nodiscard]] inline std::vector<entt::entity> FarmsOfTown(const Registry& registry, entt::entity town)
{
	auto farms = list_order::Walk<components::FishFarm>(registry);
	std::erase_if(farms,
	              [&registry, town](entt::entity farm) { return registry.Get<const components::FishFarm>(farm).town != town; });
	return farms;
}

} // namespace openblack::ecs::fish_farm_order
