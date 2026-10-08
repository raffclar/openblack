/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/MeshBoxProviderInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// The bounding boxes of the meshes in the resource cache
class MeshBoxProvider final: public MeshBoxProviderInterface
{
public:
	[[nodiscard]] std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type meshId) const override;
};
} // namespace openblack::ecs::systems
