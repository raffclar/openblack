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
#include <span>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FootprintPaint.h"

/// Which footprints the game paints into a land block, in which order, and where they lie. Pure rules, tested on their
/// own.
///
/// The game paints a block's footprints when it paints the block's texture. It looks for the things in the map's cells
/// over the block and in four more cells beyond each side, row by row along x and in each row along z. In each cell it
/// takes the things that stay put, then the things that move, and it paints each thing one time only. After the things
/// it paints the rivers. The rivers come in the order the land made them, and the last stretch of each river comes
/// first.
namespace openblack::ecs::land_footprints
{

/// The cells beyond each side of a block in which the game looks for things whose footprints the block takes
inline constexpr int32_t k_CellMargin = 4;
/// Cells along a side of a block
inline constexpr int32_t k_BlockCells = 16;
/// Cells along a side of the map
inline constexpr int32_t k_MapCells = 512;

/// The things in one of a cell's lists, in the order the cell keeps them
using CellList = std::function<std::span<const entt::entity>(glm::ivec2 cell)>;

/// The things whose footprints the game paints into a block, in the order that it paints them. `block` is the block's
/// column and row along x and z. `hasFootprint` tells which things have a footprint.
[[nodiscard]] std::vector<entt::entity> ThingsOverBlock(glm::ivec2 block, const CellList& fixed, const CellList& mobile,
                                                        const std::function<bool(entt::entity)>& hasFootprint);

/// Where a placed thing's footprint lies: from its position and its turn with its scale in it, in the form of
/// x87::GameUprightTurn (each column is a row of the game's matrix)
[[nodiscard]] footprint_paint::Placement PlacementOf(const glm::vec3& position, const glm::mat3& scaledTurn);

/// Where a river's meshes lie on the stretch from one of its points to the next. The meshes turn to point from the
/// first point to the second, and stretch along their x by the distance between the points, over 30.
[[nodiscard]] footprint_paint::Placement RiverStretch(const glm::vec3& from, const glm::vec3& to);

/// The lowest and the highest corner, along x and z, of a footprint's triangles on the ground, as the game finds them
[[nodiscard]] std::pair<glm::vec2, glm::vec2> GroundBounds(std::span<const footprint_paint::Triangle> triangles,
                                                           const footprint_paint::Placement& placement);

/// Whether the bounds of a river's footprint reach a block whose lowest corner is `blockCorner`, as the game tests it.
/// A footprint that only touches the block's edge reaches it too.
[[nodiscard]] bool ReachesBlock(const std::pair<glm::vec2, glm::vec2>& bounds, glm::vec2 blockCorner);

/// A stretch of a river whose footprint the game paints
struct RiverPrint
{
	/// Which river, in the order the land made them
	size_t river {0};
	/// The point the stretch starts at
	size_t from {0};
	/// The channel, which lowers the land's alpha; else the bed, which mixes its colour into the land
	bool channel {false};

	constexpr bool operator==(const RiverPrint&) const = default;
};

/// The rivers' footprints in the order that the game paints them, from the number of points of each river in the order
/// the land made them. The first river comes first. Each river's last stretch comes first, its bed and then its channel.
[[nodiscard]] std::vector<RiverPrint> RiverPrintOrder(std::span<const size_t> pointsOfEachRiver);

} // namespace openblack::ecs::land_footprints
