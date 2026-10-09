/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PrimaryCreature.h"

#include <algorithm>

void openblack::primary_creature::Acquire(std::vector<entt::entity>& acquired, entt::entity creature)
{
	if (std::ranges::find(acquired, creature) == acquired.end())
	{
		acquired.push_back(creature);
	}
}
