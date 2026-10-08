/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TownCellObjectsInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The map cells' object lists (ecs::map_cells) and the objects' 2D radius (ecs::object)
class TownCellObjects final: public TownCellObjectsInterface
{
public:
	[[nodiscard]] std::vector<entt::entity> ObjectsInCell(glm::ivec2 cell) const override;
	[[nodiscard]] float Get2DRadius(entt::entity object) const override;
};
} // namespace openblack::ecs::systems
