/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
namespace components
{
/// A prop in a villager's hand (ECS/CarriedProps.h): drawn like any mesh, moved every frame
struct CarriedProp
{
	entt::entity owner {entt::null};
	int32_t type {0};
};
} // namespace components

/// The object a villager carries, drawn in its hand like the original (docs/bw1-notes/animation.md): the carried
/// object's mesh (one mesh per CARRIED_OBJECT) linked to bone 15 of the villager's pose, the grip at the end of its
/// -X arm (rows -X, -Z, -Y of the bone, no offset). Not while
/// the villager is hidden or in the hand. Runs after the poses are computed.
void UpdateCarriedProps();

/// the mesh (AllMeshes.h index) of a CARRIED_OBJECT, 0 for none (0, 1 and out of
/// range). The villager's dropped log is made with it.
[[nodiscard]] uint32_t CarriedObjectMesh(int32_t carriedObject);

} // namespace openblack::ecs
