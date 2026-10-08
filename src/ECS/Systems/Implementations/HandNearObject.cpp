/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The object near the action's point when the click hit none and the point it searches around, the land behind the
// hand seen from the camera. Wiki: docs/bw1-notes/hand-and-interface.md, "Object under the cursor".

#define LOCATOR_IMPLEMENTATIONS

#include <limits>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void HandSystem::UpdatePointBehindHand() noexcept
{
	// Once a drawn frame, in the landscape draw: the box (min / max) of the hand's bones 1..n-1 (element 1 first, the
	// root left out), the camera's ray through its centre onto the land (with the plane y = 0 within 7500 m when the
	// land is missed): hit or not, and the point (x, 0, z)
	_pointBehindHand.reset();
	const auto* bones = GetBoneMatrices();
	if (bones == nullptr || bones->empty() || !Locator::camera::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const glm::mat4 model = GetHandMatrix();
	glm::vec3 lo(std::numeric_limits<float>::max());
	glm::vec3 hi(std::numeric_limits<float>::lowest());
	for (size_t i = 1; i < bones->size(); ++i)
	{
		const glm::vec3 joint = glm::vec3(model * (*bones)[i][3]);
		lo = glm::min(lo, joint);
		hi = glm::max(hi, joint);
	}
	const glm::vec3 centre = (lo + hi) * 0.5f;
	const glm::vec3 camera = Locator::camera::value().GetOrigin();
	glm::vec2 hit;
	if (Locator::terrainSystem::value().RayCast(camera, centre, hit, camera))
	{
		_pointBehindHand = glm::vec3(hit.x, 0.0f, hit.y);
	}
}

std::optional<entt::entity> HandSystem::FindObjectNearMapCoord(glm::vec3 at) const noexcept
{
	namespace map_coords = openblack::map_coords;
	// nothing without the point behind the hand or with it at (0, 0, 0)
	if (!_pointBehindHand)
	{
		return std::nullopt;
	}
	// MapCoords c of that point: x, z = m x 6553.6 truncated toward zero, altitude = 0 - the ground height (so 0 only on a cell
	// at height 0)
	const glm::vec3 c = *_pointBehindHand;
	if (map_coords::ToFixed(c.x) == 0 && map_coords::ToFixed(c.z) == 0 &&
	    Locator::terrainSystem::value().GetHeightAt(glm::vec2(c.x, c.z)) == 0.0f)
	{
		return std::nullopt;
	}
	// c on the water: the shoal near the action's point (xz distance^2 < 4), then the fish farm of that shoal.
	// (approximate) ecs::FindFishFarmAt walks the farms, not the shoal list then the farm: they only differ where
	// shoals overlap
	// (inferred) !IsLand stands for the original's water test
	if (!hand_detail::IsLand(c))
	{
		if (const auto farm = ecs::FindFishFarmAt(at); farm)
		{
			return farm;
		}
	}
	// the cells of the square +-5 m around c's MapCoords value (x 10 / 65536; (c -+ 5) x 65536 / 10 truncated toward zero,
	// signed high words), x outer, z inner, the fixed list then the mobile one; every object but the fragments, at its 2D
	// distance from c (GetDistanceInMetres), the nearest under 5 (strict, the first of a tie)
	const auto& registry = Locator::entitiesRegistry::value();
	const float cx = map_coords::Quantise(c.x);
	const float cz = map_coords::Quantise(c.z);
	const int16_t minX = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cx - 5.0f));
	const int16_t maxX = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cx + 5.0f));
	const int16_t minZ = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cz - 5.0f));
	const int16_t maxZ = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cz + 5.0f));
	std::optional<entt::entity> best;
	float bestDistance = 5.0f;
	for (int32_t i = minX; i <= maxX; ++i)
	{
		for (int32_t j = minZ; j <= maxZ; ++j)
		{
			const glm::ivec2 cell(i, j);
			if (!map_coords::InBounds(cell))
			{
				continue;
			}
			ecs::map_cells::ForEachInCell(cell, [&](entt::entity object) {
				if (!ecs::IsAvailable(object) || registry.AllOf<Fragment>(object))
				{
					return true;
				}
				const auto* transform = registry.TryGet<const Transform>(object);
				if (transform == nullptr)
				{
					return true;
				}
				const float distance = gutils::GetDistanceInMetres(c, transform->position);
				if (distance < bestDistance)
				{
					bestDistance = distance;
					best = object;
				}
				return true;
			});
		}
	}
	// the distance from the action's point to c: the nearest counts when it is not farther (<=)
	if (best && bestDistance <= gutils::GetDistanceInMetres(at, c))
	{
		return best;
	}
	return std::nullopt;
}
