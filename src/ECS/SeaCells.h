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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
namespace lnd
{
struct LNDCell;
}
} // namespace openblack

/// The sea and land predicates of the landscape cells (the map position's tests, the map cell's collide, the sound
/// map's surface type), exactly as the original reads them, the map border included: one place for the physics,
/// the hand, the sounds and the AI instead of each module's own rule.
///
/// All read the landscape cell of block [x >> 4][z >> 4], index (x & 15) * 17 + (z & 15), with x, z the high words
/// of the 16.16 MapCoords (world / 10): a cell off 0..511 (openblack: 0..GetCellsPerSide() - 1) or without a block
/// is "no cell". Its altitude byte (openblack: GetCellAltitude, the same with 8 altitude bits) and its LND
/// properties (0x10 hasWater, 0x20 coastLine) are what they test.
///
/// The overloads with a LandIslandInterface are the pure predicates (unit tests); the others use the Locator's island
/// and answer like an empty map when none is loaded (IsWater true, the rest false).
namespace openblack::ecs::sea_cells
{

/// The cell of a world point (map_coords::CellOf: the unsigned high words of x * 6553.6f truncated toward zero, so
/// negative -> off the map)
[[nodiscard]] glm::ivec2 CellOf(glm::vec3 point);
/// round(x * 0.1), round(z * 0.1): the cell rounded to the nearest, as the sound events read it
[[nodiscard]] glm::ivec2 RoundedCellOf(glm::vec3 point);

/// The landscape cell, nullptr off the map or where no block is
[[nodiscard]] const lnd::LNDCell* CellAt(const LandIslandInterface& island, glm::ivec2 cell);
/// The cell's altitude in height units (0.67); 0 for no cell (as IsDryLand reads it)
[[nodiscard]] uint16_t AltitudeAt(const LandIslandInterface& island, glm::ivec2 cell);

/// The cell is inside the game map (its cells per side, an unsigned compare; openblack: GetCellsPerSide()), a block
/// or not
[[nodiscard]] bool InBounds(const LandIslandInterface& island, glm::ivec2 cell);
/// properties & 0x10; no cell = 1
[[nodiscard]] bool IsWater(const LandIslandInterface& island, glm::ivec2 cell);
/// !(properties & 0x10); no cell = 0
[[nodiscard]] bool IsLand(const LandIslandInterface& island, glm::ivec2 cell);
/// altitude >= 4, the water bit is not read; no cell = 0
[[nodiscard]] bool IsDryLand(const LandIslandInterface& island, glm::ivec2 cell);
/// !(properties & 0x10) && (properties & 0x20) (a land cell on the coast line); no cell = 0
[[nodiscard]] bool IsCoastal(const LandIslandInterface& island, glm::ivec2 cell);

/// The map cell's collide bits, the CollideType values
constexpr uint32_t k_CollideWater = 0x01; ///< IsWater (the water bit, or no landscape cell)
constexpr uint32_t k_CollideLand = 0x02;  ///< not water
constexpr uint32_t k_CollideField = 0x04; ///< an OBJECT_TYPE 0x12 (Field) in the map cell
/// only ecs::map_cells::CollideWithFixed; the original's map cell collide branches to the fixed-object test on this bit
/// of a result that never has it: a dead branch
constexpr uint32_t k_CollideFixed = 0x08;
constexpr uint32_t k_CollideEdge = 0x10; ///< the map cell is off the game map
constexpr uint32_t k_CollideTree = 0x20; ///< an OBJECT_TYPE 6 (ForestTree) in the map cell
/// The landscape part of the map cell's collide: 0x10 off the game map, else 1 on water (or no landscape cell) or 2.
/// The object bits come from the map cell's fixed list: 0x04 Field and 0x20 tree in ecs::map_cells::Collide, 0x08
/// only in ecs::map_cells::CollideWithFixed.
[[nodiscard]] uint32_t CollideLandscape(const LandIslandInterface& island, glm::ivec2 cell);

/// 6 (DEEP_WATER) for no cell, 7 (SHALLOW_WATER) where !IsLand, else the surfaceSound of the cell's material
/// (info.dat), 3 when it is not 1..8
[[nodiscard]] int32_t GetSurfaceType(const LandIslandInterface& island, glm::vec3 point);

/// The script's GET_LAND_HEIGHT: the cell (int)(x * 0.1), (int)(z * 0.1) (truncated towards 0, so -10 < x < 0 is
/// still cell 0); off 0..511, without a block or at altitude 0 -> -10.0 ("the sea"), else the land height of the
/// point (openblack: GetHeightAt)
[[nodiscard]] float ScriptLandHeight(const LandIslandInterface& island, glm::vec3 point);

// With the Locator's island
[[nodiscard]] bool InBounds(glm::vec3 point);
[[nodiscard]] bool IsWater(glm::vec3 point);
[[nodiscard]] bool IsLand(glm::vec3 point);
[[nodiscard]] bool IsDryLand(glm::vec3 point);
[[nodiscard]] bool IsCoastal(glm::vec3 point);
[[nodiscard]] uint32_t CollideLandscape(glm::vec3 point);
[[nodiscard]] int32_t GetSurfaceType(glm::vec3 point);

} // namespace openblack::ecs::sea_cells
