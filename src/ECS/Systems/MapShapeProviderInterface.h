/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::map_collide
{
struct Shape;
} // namespace openblack::ecs::map_collide

namespace openblack::ecs::systems
{
/// The collide shape the map cells give a fixed object from its mesh
class MapShapeProviderInterface
{
public:
	virtual ~MapShapeProviderInterface() = default;

	/// The shape of the object's mesh at its position, turned and scaled, and the reach of its cells; false when the
	/// object has no mesh to build it from
	[[nodiscard]] virtual bool MeshShape(entt::entity object, map_collide::Shape& shape, float& reach) const = 0;
};
} // namespace openblack::ecs::systems
