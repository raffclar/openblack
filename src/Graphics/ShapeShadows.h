/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/TreeShadows.h"

namespace openblack::graphics
{

/// The shadows of buildings, rocks and the island's other solid things, which Black & White bakes into the land's
/// textures as it paints each land block, together with the trees' (see tree_shadows), from its fixed far sun. They are
/// cast by the shapes of the things' triangles alone, never through their textures.
///
/// Every triangle of every level of detail (not the physics' own parts, nor the parts that only show in some states, such
/// as a building's scaffolding) is flattened from the sun onto the flat plane through the thing's origin: a point's
/// height above the origin (nothing below it) sends it along the line from the sun, as far again as
/// height / (sun height - height) of the way, the sun's place taken as it is in the world. Each texel of a block holds
/// eight samples: two rows of four. The flattened triangle is scanned into the block's 512 rows (two a texel, along x)
/// and 1024 columns (four a texel, along z). A row is the flattened point's place taken down to a whole row; each edge
/// is followed from row to row in fixed point, and the samples of a row from the edges on one side up to, not
/// including, the edges on the other are covered. Triangles facing away from the sun are left out, unless the
/// triangle is drawn from both sides or the thing is the temple, whose every triangle casts.
///
/// The samples are joined with the trees' alpha bits in the same byte a texel, so overlapping shadows get no darker,
/// and darken the land the same way (see tree_shadows::Shade): a fully covered texel is half as bright.
namespace shape_shadows
{

/// The sample rows along x and columns along z of a block
inline constexpr int k_Rows = 2 * tree_shadows::k_BlockTexels;
inline constexpr int k_Columns = 4 * tree_shadows::k_BlockTexels;

/// Where a point of a thing falls on the flat plane through its origin, along x and z in the world. offset is the point
/// less the thing's origin, turned and scaled as it stands; origin is the thing's origin along x and z.
[[nodiscard]] glm::vec2 Cast(glm::vec3 offset, glm::vec2 origin);

/// A flattened point in a block's samples: its column, along z, and its row, along x, not yet taken to whole ones
struct Sample
{
	float column;
	float row;
};

/// Where a flattened point falls in the samples of the block with its corner at block
[[nodiscard]] Sample SampleOf(glm::vec2 cast, glm::vec2 block);

/// Scans a flattened triangle into a block's samples, covering them in each texel's bits. A triangle cast from both
/// sides is turned around to face the sun; any other facing away from it casts nothing.
void Fill(tree_shadows::Coverage& coverage, const std::array<Sample, 3>& corners, bool bothSides);

} // namespace shape_shadows

} // namespace openblack::graphics
