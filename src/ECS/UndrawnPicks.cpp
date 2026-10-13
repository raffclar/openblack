/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UndrawnPicks.h"

namespace openblack::ecs::undrawn_picks
{

void TestOrder(std::span<const entt::entity> drawn, std::span<const Shape> shapes, std::vector<Turn>& order)
{
	order.clear();
	for (std::size_t i = 0; i < drawn.size(); ++i)
	{
		for (const auto& shape : shapes)
		{
			if (shape.drawnWith == drawn[i])
			{
				order.push_back({.drawnIndex = i, .undrawn = shape.entity});
			}
		}
		order.push_back({.drawnIndex = i, .undrawn = std::nullopt});
	}
}

} // namespace openblack::ecs::undrawn_picks
