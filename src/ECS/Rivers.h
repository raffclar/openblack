/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs
{

/// Run after the map script: for every river segment p[i] -> p[i + 1], places the two river footprints
/// (StreamFootprint) at p[i], turned to the segment's direction (atan2(dz, dx)) and stretched along their local x
/// by the 3D segment length / 30 (the meshes are 30 units long). Replaces the footprints of an earlier call.
void CreateRiverFootprints();

} // namespace openblack::ecs
