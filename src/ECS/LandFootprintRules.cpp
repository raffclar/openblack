/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandFootprintRules.h"

#include <cmath>

#include <algorithm>
#include <unordered_set>

#include "Common/X87.h"
#include "Common/X87Angle.h"

namespace openblack::ecs::land_footprints
{

namespace
{
/// Two pi as the game keeps it in a float, added to a negative angle
constexpr float k_TwoPi = 6.2831854820251465f;
/// A thirtieth as the game keeps it in a float: the river's meshes are 30 units long
constexpr float k_PerMeshLength = 0.0333333351f;
} // namespace

std::vector<entt::entity> ThingsOverBlock(glm::ivec2 block, const CellList& fixed, const CellList& mobile,
                                          const std::function<bool(entt::entity)>& hasFootprint)
{
	const auto first = (block & 0x1F) * k_BlockCells - k_CellMargin;
	const auto low = glm::max(first, glm::ivec2(0));
	const auto high = glm::min(first + k_BlockCells + 2 * k_CellMargin, glm::ivec2(k_MapCells - 1));
	std::vector<entt::entity> things;
	std::unordered_set<entt::entity> painted;
	const auto take = [&](std::span<const entt::entity> list) {
		for (const auto entity : list)
		{
			if (hasFootprint(entity) && painted.insert(entity).second)
			{
				things.push_back(entity);
			}
		}
	};
	for (int32_t x = low.x; x <= high.x; ++x)
	{
		for (int32_t z = low.y; z <= high.y; ++z)
		{
			take(fixed({x, z}));
			take(mobile({x, z}));
		}
	}
	return things;
}

footprint_paint::Placement PlacementOf(const glm::vec3& position, const glm::mat3& scaledTurn)
{
	return {
	    .xFromX = scaledTurn[0].x,
	    .xFromZ = scaledTurn[2].x,
	    .zFromX = scaledTurn[0].z,
	    .zFromZ = scaledTurn[2].z,
	    .origin = {position.x, position.z},
	};
}

footprint_paint::Placement RiverStretch(const glm::vec3& from, const glm::vec3& to)
{
	// The game turns the meshes by the angle of the stretch across the ground, from 0 to two pi
	const float dx = to.x - from.x;
	const float dy = to.y - from.y;
	const float dz = to.z - from.z;
	const double wide = x87::Atan2(dz, dx);
	const float angle = wide < 0.0 ? x87::Add(wide, k_TwoPi) : x87::RoundToFloat(wide);
	const auto turn = x87::GameUprightTurn(angle);
	// It then stretches the meshes' x by the length of the stretch, over 30
	const float stretch = std::sqrt((dz * dz + dy * dy) + dx * dx) * k_PerMeshLength;
	return {
	    .xFromX = stretch * turn[0].x,
	    .xFromZ = turn[2].x,
	    .zFromX = stretch * turn[0].z,
	    .zFromZ = turn[2].z,
	    .origin = {from.x, from.z},
	};
}

std::pair<glm::vec2, glm::vec2> GroundBounds(std::span<const footprint_paint::Triangle> triangles,
                                             const footprint_paint::Placement& placement)
{
	glm::vec2 low {0.0f};
	glm::vec2 high {0.0f};
	bool first = true;
	for (const auto& triangle : triangles)
	{
		for (const auto& corner : triangle.corners)
		{
			const glm::vec2 ground {placement.xFromZ * corner.y + placement.xFromX * corner.x + placement.origin.x,
			                        placement.zFromZ * corner.y + placement.zFromX * corner.x + placement.origin.y};
			if (first)
			{
				low = ground;
				high = ground;
				first = false;
				continue;
			}
			low = glm::min(low, ground);
			high = glm::max(high, ground);
		}
	}
	return {low, high};
}

bool ReachesBlock(const std::pair<glm::vec2, glm::vec2>& bounds, glm::vec2 blockCorner)
{
	const auto far = blockCorner + footprint_paint::k_BlockUnits;
	return blockCorner.x <= bounds.second.x && bounds.first.x <= far.x && blockCorner.y <= bounds.second.y &&
	       bounds.first.y <= far.y;
}

std::vector<RiverPrint> RiverPrintOrder(std::span<const size_t> pointsOfEachRiver)
{
	std::vector<RiverPrint> prints;
	for (size_t river = 0; river < pointsOfEachRiver.size(); ++river)
	{
		for (size_t stretch = pointsOfEachRiver[river]; stretch-- > 1;)
		{
			prints.push_back({.river = river, .from = stretch - 1, .channel = false});
			prints.push_back({.river = river, .from = stretch - 1, .channel = true});
		}
	}
	return prints;
}

} // namespace openblack::ecs::land_footprints
