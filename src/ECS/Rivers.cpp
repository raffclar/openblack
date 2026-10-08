/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Rivers.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs
{

void CreateRiverFootprints()
{
	using namespace components;
	auto& registry = Locator::entitiesRegistry::value();

	std::vector<entt::entity> old;
	registry.Each<const StreamFootprint>(
	    [&old](entt::entity entity, const StreamFootprint& /*unused*/) { old.push_back(entity); });
	for (const auto entity : old)
	{
		registry.Destroy(entity);
	}

	// the two meshes are 30 units long along their local x
	constexpr float k_MeshLength = 30.0f;
	std::vector<std::pair<Transform, bool>> footprints;
	registry.Each<const Stream>([&footprints](const Stream& stream) {
		for (size_t i = 0; i + 1 < stream.points.size(); ++i)
		{
			const auto& from = stream.points[i];
			const auto& to = stream.points[i + 1];
			const float angle = std::atan2(to.z - from.z, to.x - from.x);
			const float stretch = glm::distance(from, to) / k_MeshLength;
			// AngleY(angle) turns local x onto (cos, 0, sin) of the segment, as the original places objects
			// ((inferred) the stretch of local x only: the original never scales one row alone)
			const Transform transform {from, affine::AngleY(angle), glm::vec3(stretch, 1.0f, 1.0f)};
			footprints.emplace_back(transform, false);
			footprints.emplace_back(transform, true);
		}
	});
	for (const auto& [transform, channel] : footprints)
	{
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, transform);
		registry.Assign<StreamFootprint>(entity, channel);
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Rivers: {} footprints", footprints.size());
}

} // namespace openblack::ecs
