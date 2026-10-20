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

#include <array>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/GroundShadow.h"
#include "Graphics/TreeShadows.h"

/// The moving shadow of a thing that the physics moves: a tree, a pot, a villager, a rock or another thing that flies
/// or falls. The game casts it from a light straight above the thing, flattens it onto the level of the thing, and lays
/// it on the land only, never on objects (see GroundShadow).
///
/// Things whose shadows go through their textures' alpha (trees, big forests, dead trees, pots and crops) fill the
/// shadow's texture through that alpha, then soften it. Each triangle of the nearest detail, without the parts that
/// bones move, goes onto a grid of 32 by 32 texels. Each texel that the triangle covers takes the top three bits of the
/// alpha at the texture's point there. Then each texel inside the clear ring takes the average of its own level and the
/// levels of the next texel across, the next texel down and the texel between them. Thus a solid leaf gives at most 7
/// of the 15 levels.
namespace openblack::graphics::shadow_texture
{

/// How far above a moving body its light is
inline constexpr float k_LightHeight = 15000.0f;
inline constexpr uint8_t k_Texels = GroundShadow::k_Texels;
/// The highest and the lowest texel that a corner can fall on, across and down
inline constexpr float k_FirstTexel = 1.0f;
inline constexpr float k_LastTexel = 31.0f;
/// The texture's coordinates go over this many places of the alpha mask
inline constexpr float k_MaskPlaces = 63.0f;

/// The light that the shadow of a moving body comes from: straight above the place where it is drawn
[[nodiscard]] glm::vec3 LightAbove(const glm::vec3& origin);

/// A corner of a triangle on the grid of texels: its column, its row, and its texture's coordinates on the mask's
/// places, all as whole numbers
struct Corner
{
	int32_t column;
	int32_t row;
	int32_t u;
	int32_t v;
};

/// The point where a vertex of the body falls on the grid. `local` is the vertex in the model, which `model` moves into
/// the world. The vertex goes along the ray from the light onto the level, also when it is below the level. Then it goes
/// across the footprint onto the texels, and stays on the texels from k_FirstTexel to k_LastTexel.
[[nodiscard]] Corner CornerOf(const glm::vec3& local, const glm::vec2& uv, const glm::mat4& model,
                              const GroundShadow::Footprint& footprint, const GroundShadow::Cast& cast);

/// Where an edge of a triangle crosses a row of texels: its column, and its texture's coordinates in 16.16 fixed point
struct Crossing
{
	int32_t column {0};
	int32_t u {0};
	int32_t v {0};
};

/// The texels of a shadow while its triangles fill it, and where the edges of the last triangle crossed each row. The
/// game keeps the crossings from one triangle to the next.
struct Canvas
{
	/// The level of each texel, in the top four bits of 16, by rows
	std::array<uint16_t, k_Texels * k_Texels> texels {};
	/// The crossings of the edges that go down the rows, and of the edges that go up the rows
	std::array<Crossing, k_Texels> down {};
	std::array<Crossing, k_Texels> up {};
};

/// Fills a triangle into the canvas as the game does:
/// 1. It keeps each corner on the grid and puts the corners in a fixed order from the top.
/// 2. It steps each edge across the rows that it spans, in 16.16 fixed point.
/// 3. It fills each row from the first crossing to the second crossing.
/// Each texel that the row fills takes the top three bits of the mask at the texture's point there. A point outside
/// the mask gives nothing.
void Fill(Canvas& canvas, const tree_shadows::AlphaMask& mask, std::array<Corner, 3> corners);

/// The levels of the shadow's texture, by rows. A texel inside the clear ring takes the average of the 2 by 2 texels
/// from itself across and down, rounded down. Then the shadow's opacity scales the level down, as GroundShadow::Level.
[[nodiscard]] std::array<uint8_t, k_Texels * k_Texels> Levels(const Canvas& canvas, uint8_t alpha);

} // namespace openblack::graphics::shadow_texture
