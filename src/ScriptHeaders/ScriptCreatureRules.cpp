/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCreatureRules.h"

#include <algorithm>

#include "3D/MapCoords.h"

namespace openblack::script::creature_rules
{

int32_t LeashTypeForScripts(std::span<const LeashPostState> posts)
{
	if (posts.empty())
	{
		return 0;
	}
	const auto picked = std::ranges::find_if(posts, [](const LeashPostState& post) { return post.selected; });
	return picked != posts.end() ? static_cast<int32_t>(picked->type) : -1;
}

LeashedPair WhichIsLeashed(bool firstIsCreature, bool secondIsCreature)
{
	if (firstIsCreature)
	{
		return LeashedPair::FirstIsCreature;
	}
	return secondIsCreature ? LeashedPair::SecondIsCreature : LeashedPair::Neither;
}

Confinement ConfinementFor(const glm::vec3& point, float radius)
{
	return {
	    .centre = {map_coords::Quantise(point.x), 0.0f, map_coords::Quantise(point.z)},
	    .radius = radius,
	};
}

} // namespace openblack::script::creature_rules
