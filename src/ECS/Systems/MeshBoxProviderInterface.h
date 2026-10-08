/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/core/fwd.hpp>

#include "3D/AxisAlignedBoundingBox.h"

namespace openblack::ecs::systems
{
/// The bounding boxes of the meshes the object sizes are read from
class MeshBoxProviderInterface
{
public:
	virtual ~MeshBoxProviderInterface() = default;

	/// The mesh's bounding box, or nothing when the mesh is not loaded
	[[nodiscard]] virtual std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type meshId) const = 0;
};
} // namespace openblack::ecs::systems
