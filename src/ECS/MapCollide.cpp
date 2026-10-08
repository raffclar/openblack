/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapCollide.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Locator.h"
#include "MapCells.h"
#include "Resources/ResourcesInterface.h"
#include "SeaCells.h"

using namespace openblack;
using namespace openblack::ecs::map_collide;

namespace
{
/// A long shape (with child circles) is used above this ratio of the half sizes
constexpr float k_LongRatio = 1.4f;

/// x' = x cos a - z sin a, z' = x sin a + z cos a (the sign checked on the maps saved by the game)
glm::vec2 Rotate(glm::vec2 v, float angle)
{
	const float c = std::cos(angle);
	const float s = std::sin(angle);
	return {v.x * c - v.y * s, v.x * s + v.y * c};
}

bool LogRejections()
{
	static const bool log = [] {
		const char* env = std::getenv("OPENBLACK_LOG_ISOK");
		return env != nullptr && env[0] != '\0' && env[0] != '0';
	}();
	return log;
}
} // namespace

bool openblack::ecs::map_collide::FromMesh(entt::id_type meshResource, glm::vec2 position, float yAngle, float scale,
                                           Shape& shape)
{
	if (!Locator::resources::has_value())
	{
		return false;
	}
	const auto mesh = Locator::resources::value().GetMeshes().Handle(meshResource);
	if (!mesh)
	{
		return false;
	}
	// the mesh's bounding box: centre and half size over every vertex of every submesh
	const auto& bb = mesh->GetBoundingBox();
	shape.centre = position + Rotate(scale * glm::xz(bb.Center()), yAngle);
	const float ex = std::max(1.0f, scale * bb.Size().x * 0.5f);
	const float ez = std::max(1.0f, scale * bb.Size().z * 0.5f);
	const float longHalf = std::max(ex, ez);
	const float shortHalf = std::min(ex, ez);
	shape.children.clear();
	shape.childRadius = 0.0f;
	if (longHalf / shortHalf <= k_LongRatio)
	{
		shape.radius = longHalf;
		return true;
	}
	// the outer circle; then int(long / short) + 1 circles of the short half size in a row
	// along the long axis, turned with the object
	shape.radius = std::sqrt(ex * ex + ez * ez);
	shape.childRadius = shortHalf;
	const int count = static_cast<int>(longHalf / shortHalf) + 1;
	const float step = 2.0f * longHalf / static_cast<float>(count);
	for (int i = 0; i < count; ++i)
	{
		const float t = (static_cast<float>(i) + 0.5f) * step - longHalf;
		const glm::vec2 local = ex > ez ? glm::vec2(t, 0.0f) : glm::vec2(0.0f, t);
		shape.children.push_back(shape.centre + Rotate(local, yAngle));
	}
	return true;
}

bool openblack::ecs::map_collide::Collide(glm::vec2 point, float radius, const Shape& shape)
{
	const auto hits = [point, radius](glm::vec2 centre, float other) {
		const auto delta = point - centre;
		return glm::dot(delta, delta) <= (radius + other) * (radius + other);
	};
	if (!hits(shape.centre, shape.radius))
	{
		return false;
	}
	if (shape.children.empty())
	{
		return true;
	}
	return std::any_of(shape.children.begin(), shape.children.end(),
	                   [&](glm::vec2 child) { return hits(child, shape.childRadius); });
}

bool openblack::ecs::map_collide::IsOkToCreateAtPos(glm::vec3 position, std::string_view command)
{
	// the collision with fixed objects on the live map cells (ecs::map_cells: everything in the map now, not only
	// what the land script made), bit 8 clear -> yes. Off the map it is all bits set, which has the bit. MapCoords
	// x, z only (FromMetres: the altitude is not read)
	const auto collide = map_cells::CollideWithFixed(map_coords::FromMetres(glm::vec2(position.x, position.z)));
	if ((collide & sea_cells::k_CollideFixed) == 0)
	{
		return true;
	}
	// set: whether it is water (ecs::sea_cells, the cell of the 16.16 MapCoords): bit 0x10 of the land cell
	// (hasWater); no landscape cell (off the map, or an empty block) counts as water
	if (sea_cells::IsWater(position))
	{
		return true;
	}
	if (LogRejections())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "isok: {} at ({:.2f}, {:.2f}) rejected (collide 0x{:X})", command,
		                   position.x, position.z, collide);
	}
	return false;
}
