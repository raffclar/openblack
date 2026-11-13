/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownCentreOrder.h"

#include <algorithm>
#include <iterator>

namespace openblack::ecs::town_centre_order
{

void Update(std::vector<entt::entity>& order, std::span<const entt::entity> present)
{
	std::erase_if(order, [present](entt::entity centre) { return std::ranges::find(present, centre) == present.end(); });
	std::vector<entt::entity> added;
	std::ranges::copy_if(present, std::back_inserter(added),
	                     [&order](entt::entity centre) { return std::ranges::find(order, centre) == order.end(); });
	order.insert(order.begin(), added.begin(), added.end());
}

} // namespace openblack::ecs::town_centre_order
