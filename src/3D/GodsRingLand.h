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

#include "3D/FlatLand.h"
#include "3D/LandData.h"

/// The land of the network map for 32 gods: a round, flat island in the middle of the map, with the sea around it. The
/// island has the checkerboard grass of the testbed, so that distances show. The code makes the land, as it makes the
/// testbed's land: no file gives it.
namespace openblack::gods_ring_land
{
/// The cells of the map along each side, their size and the middle of the map, as on the testbed
constexpr int k_CellsPerSide = flat_land::k_CellsPerSide;
constexpr float k_CellSize = flat_land::k_CellSize;
constexpr glm::vec2 k_MapMiddle = flat_land::k_MapMiddle;

/// The radius of the flat part of the island, from the middle of the map, in units of the map
constexpr float k_PlaneRadius = 2350.0f;
/// The shore goes down from the plane to the sea bed in these steps, one cell each, as the testbed's lake shore. The
/// sea bed starts at this radius.
constexpr float k_SeaRadius = k_PlaneRadius + (static_cast<float>(flat_land::k_ShoreCells) * k_CellSize);

/// The ambient sound type of the open sea
constexpr uint8_t k_SeaSoundFlags = 1 << 2;

/// The altitude of the map's corner (x, z)
[[nodiscard]] uint8_t Altitude(int x, int z);

/// What a cell of the map is, by the altitudes of its corners, as on the testbed
[[nodiscard]] flat_land::CellKind KindOf(int x, int z);

/// The cell (x, z) of the map
[[nodiscard]] lnd::LNDCell CellAt(int x, int z);

/// The island. A block of the map that has only open sea has no land.
[[nodiscard]] LandData Build();
} // namespace openblack::gods_ring_land
