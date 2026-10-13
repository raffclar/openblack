/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandReach.h"

#include <glm/common.hpp>

#include "Common/GUtilsDistance.h"

namespace openblack::hand_reach
{

std::optional<glm::vec3> Middle(std::span<const glm::vec3> bonePlaces)
{
	if (bonePlaces.size() < 2)
	{
		return std::nullopt;
	}
	glm::vec3 low = bonePlaces[1];
	glm::vec3 high = bonePlaces[1];
	for (const auto& place : bonePlaces.subspan(2))
	{
		low = glm::min(low, place);
		high = glm::max(high, place);
	}
	return (low + high) * 0.5f;
}

namespace
{
/// A 16.16 value moved by some metres, the way the search finds its cells: into metres, moved, and back, truncated
int32_t Moved(int32_t fixed, float metres)
{
	constexpr float k_MetresPerCell = 10.0f;
	constexpr float k_FixedPerCell = 65536.0f;
	// The whole value is scaled once before it is rounded to a float
	const auto inMetres = static_cast<float>(static_cast<double>(fixed) * k_MetresPerCell) / k_FixedPerCell;
	return map_coords::FtoL((inMetres + metres) * k_FixedPerCell / k_MetresPerCell);
}
} // namespace

Cells CellsAround(const map_coords::MapCoords& point)
{
	// The last row across z is read unsigned, the others signed
	return {.first = {map_coords::SignedCellOf(Moved(point.x, -k_Reach)), map_coords::SignedCellOf(Moved(point.z, -k_Reach))},
	        .last = {map_coords::SignedCellOf(Moved(point.x, k_Reach)), map_coords::CellOf(Moved(point.z, k_Reach))}};
}

std::optional<size_t> Nearest(const map_coords::MapCoords& behindHand, const map_coords::MapCoords& cursor,
                              std::span<const map_coords::MapCoords> things)
{
	std::optional<size_t> nearest;
	float nearestDistance = k_Reach;
	for (size_t i = 0; i < things.size(); ++i)
	{
		const float distance = gutils::GetDistanceInMetres(behindHand, things[i]);
		if (distance < nearestDistance)
		{
			nearest = i;
			nearestDistance = distance;
		}
	}
	if (nearest.has_value() && nearestDistance <= gutils::GetDistanceInMetres(cursor, behindHand))
	{
		return nearest;
	}
	return std::nullopt;
}

} // namespace openblack::hand_reach
