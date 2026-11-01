/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TornadoPileTake.h"

#include <algorithm>

#include "ECS/Systems/ResourceStoreSystemInterface.h"

using namespace openblack::ecs;

uint32_t tornado_pile::Take(systems::ResourceStoreSystemInterface& stores, entt::entity pile, ResourceType resource,
                            uint32_t asked)
{
	const auto amount = std::min(asked, stores.GetResource(pile, resource));
	if (amount == 0)
	{
		return 0;
	}
	[[maybe_unused]] const auto given = stores.RemoveResource(pile, resource, amount);
	return amount;
}
