/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/MapShapeProviderInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The shape built from the object's loaded mesh (map_collide::FromMesh)
class MapShapeProvider final: public MapShapeProviderInterface
{
public:
	[[nodiscard]] bool MeshShape(entt::entity object, map_collide::Shape& shape, float& reach) const override;
};
} // namespace openblack::ecs::systems
