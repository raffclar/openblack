/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

#include "Map.h"
#include "MapGridCells.h"

namespace openblack::ecs
{

class MapProduction final: public MapInterface
{
	[[nodiscard]] const std::unordered_set<entt::entity>& GetFixedInGridCell(const CellId& cellId) const override;
	[[nodiscard]] const std::unordered_set<entt::entity>& GetFixedInGridCell(const glm::vec3& pos) const override;

	void Rebuild() override;

private:
	void Clear() override;
	void Build() override;

	MapGridCells<k_GridSize.x * k_GridSize.y> _fixedGrid;
};

} // namespace openblack::ecs
