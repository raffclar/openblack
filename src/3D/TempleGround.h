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

#include <functional>
#include <vector>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

/// The land under a temple's heart. When the game makes a heart, it levels the cell corners near the heart to the
/// altitude of the corner below the heart. The corners in 35 m of that corner become flat. From 35 m to 70 m the land
/// goes back to its own altitude in a straight line. Each corner changes one time, from the altitude that it has then.
namespace openblack::temple_ground
{

/// The corners on each side of the centre corner that the heart can change, along x and along z
constexpr int k_Reach = 8;
/// Corners nearer than this distance in metres take the altitude of the centre corner
constexpr float k_FlatRadius = 35.0f;
/// Corners farther than this distance in metres keep their own altitude
constexpr float k_EdgeRadius = 70.0f;
/// The share of its own altitude that a corner gets for each metre past the flat radius: 1 / 35 as a float
constexpr float k_SharePerMetre = 0.0285714287f;

/// A corner and the altitude that the heart gives it
struct CornerAltitude
{
	glm::ivec2 corner;
	uint8_t altitude;
};

/// The share of its own altitude that a corner keeps, from 0 to 1, by its distance from the centre corner. dx and dz
/// are the counts of cells between the two corners.
[[nodiscard]] float OwnShare(int dx, int dz);

/// The altitude of a corner after the heart levels it: the own share of its altitude plus the remaining share of the
/// centre altitude, as floats, truncated towards 0
[[nodiscard]] uint8_t LevelledAltitude(uint8_t own, float ownShare, uint8_t centre);

/// The corner below a heart at this map position: its metres times 0.1, each truncated towards 0
[[nodiscard]] glm::ivec2 CentreCorner(const map_coords::MapCoords& place);

/// The new altitudes of the 17 by 17 corners round the centre, x first, then z. altitudeAt gives the altitude of a
/// corner now, or 0 where the map has no block. The list holds corners off the map too: the caller ignores them.
[[nodiscard]] std::vector<CornerAltitude> Levelled(glm::ivec2 centre, const std::function<uint8_t(glm::ivec2)>& altitudeAt);

} // namespace openblack::temple_ground
