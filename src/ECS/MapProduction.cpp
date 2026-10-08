/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS
#include "MapProduction.h"

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "MapCells.h"

using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace map_coords = openblack::map_coords;

const std::unordered_set<entt::entity>& MapProduction::GetFixedInGridCell(const CellId& cellId) const
{
	return _fixedGrid.At(cellId.x + cellId.y * k_GridSize.x);
}

const std::unordered_set<entt::entity>& MapProduction::GetFixedInGridCell(const glm::vec3& pos) const
{
	const auto cellId = GetGridCell(pos);
	return GetFixedInGridCell(cellId);
}

void MapProduction::Rebuild()
{
	Clear();
	Build();
	// the ordered cell lists (ECS/MapCells) take in what the owners without hooks did since the last rebuild
	map_cells::Sync();
}

void MapProduction::Clear()
{
	// only the cells the last Build filled
	_fixedGrid.Clear();
}

void MapProduction::Build()
{
	// the fixed objects only: the wall hug's obstacles. The mobile ones are read from the map cells (ECS/MapCells)
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Fixed, const Transform>(
	    [this](entt::entity entity, const Fixed& fixed, const Transform& transform) {
		    // TODO(bwrsandman): This is only in the case of a square bb underling the bounding circle (x/z) <= 1.4
		    const float radius = fixed.boundingRadius * glm::compMax(transform.scale) + 1.0f;
		    // the corners' signed high words (as a JustMapXZ) and only the cells inside the map (InBounds): a corner off the
		    // map does not wrap to cell 0xFFFF. (inferred) openblack's own grid: the original's map object insertion is not
		    // ported
		    const auto low = glm::ivec2(map_coords::SignedCellOf(map_coords::ToFixed(fixed.boundingCenter.x - radius)),
		                                map_coords::SignedCellOf(map_coords::ToFixed(fixed.boundingCenter.y - radius)));
		    const auto high = glm::ivec2(map_coords::SignedCellOf(map_coords::ToFixed(fixed.boundingCenter.x + radius)),
		                                 map_coords::SignedCellOf(map_coords::ToFixed(fixed.boundingCenter.y + radius)));

		    for (int32_t x = low.x; x <= high.x; ++x)
		    {
			    for (int32_t y = low.y; y <= high.y; ++y)
			    {
				    if (!map_coords::InBounds(glm::ivec2(x, y)))
				    {
					    continue;
				    }
				    const auto cellId = MapProduction::CellId(x, y);
				    if (glm::distance2(GetCellCenter(cellId), fixed.boundingCenter) < radius * radius)
				    {
					    _fixedGrid.Insert(cellId.x + cellId.y * k_GridSize.x, entity);
				    }
			    }
		    }
	    },
	    entt::exclude<Unavailable>);
}
