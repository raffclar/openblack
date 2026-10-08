/*******************************************************************************
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
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

// The buildings' "needs" signs (4 show needs info rows)

namespace openblack::ecs::show_needs
{
/// The workshop's row
inline constexpr uint8_t k_WorkshopInfo = 3;

/// A new needs sign at `at` for `owner` with that info row: an object (the next creation index) at `at`, its Zoomer
/// and desire 0, then the info's mesh, scale 1. Without a Mesh (not drawn) until its frame draws it. `at` is the owner's
/// world position: the owner's map coordinates (x, z, altitude) are copied as they are, so the sign stands where the
/// owner does and nothing asks the ground
entt::entity Create(const glm::vec3& at, entt::entity owner, uint8_t infoIndex);
/// Sets the desire
void SetDesire(entt::entity visuals, float desire);
/// Deletes the sign (when its owner is deleted)
void Delete(entt::entity visuals, bool now);
/// The frame's part of the owners' draw that draws the sign (a functional workshop not on fire draws it), in the logic
/// updater before PrepareDraw: the Zoomer, the Transform, the Mesh on / off. `frameSeconds` is the frame's game time in
/// seconds, `eye` the camera point
void UpdateFrame(float frameSeconds, const glm::vec3& eye);
} // namespace openblack::ecs::show_needs
