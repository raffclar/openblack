/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTempleParts.h"

#include <algorithm>
#include <iterator>

using namespace openblack;

bool creature_temple_parts::AltarIn(const ecs::map_cells::Outline& outline, glm::ivec2 cell)
{
	const auto cells = ecs::map_cells::CellsCovered(outline);
	return std::ranges::find(cells, cell) != cells.end();
}

std::vector<entt::entity> creature_temple_parts::ThingsSeenInCell(std::span<const entt::entity> altars,
                                                                  std::span<const entt::entity> inCell)
{
	std::vector<entt::entity> things(altars.begin(), altars.end());
	std::ranges::copy(inCell, std::back_inserter(things));
	return things;
}
