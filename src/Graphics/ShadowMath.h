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
#include <bit>
#include <functional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AffineMatrix.h"

/// The projected shadows of the original (wiki: docs/bw1-notes/rendering.md, projected shadows), CPU only and without
/// bgfx or ECS, so the tests can run it: the fade, the alpha and the light of the generic and complex updates, the
/// projection of the caster's vertices, the 4 x 2 subsample rasterizer, the resolve into 32 x 32 texels of n / 15, the
/// chroma blur, the baked fade, and the tests the land draw uses.
///
/// The game runs the x87 FPU at 24 bits of precision, so every operation here is float, one rounding per operation in
/// the original's order, no double and no FMA; float to int conversion truncates towards zero.
namespace openblack::graphics::shadow_math
{

inline constexpr float k_FadeFull = 50.0f; ///< Full shadow up to 50 radii
inline constexpr float k_FadeGone = 80.0f; ///< None from 80 radii
inline constexpr float k_FadeMax = 255.0f;
inline constexpr float k_BlockFar = 100000.0f;      ///< A block's camera distance at or past it -> 0
inline constexpr float k_NeighbourStep = 60.0f;     ///< The 3 x 3 test's offsets
inline constexpr float k_CellScale = 0.1f;          ///< World -> cells
inline constexpr float k_InvByte = 0.003921568859f; ///< 1 / 255 as a float
inline constexpr float k_VerticalLight = 15000.0f;  ///< The generic light above the caster
inline constexpr float k_HandLight = 200.0f;        ///< The hand's light above it
inline constexpr float k_CreatureRadii = 3.0f;      ///< The creature's light at most 3 R away
inline constexpr double k_CreatureNear = 0.1;       ///< (a double)
inline constexpr float k_BlockSize = 160.0f;        ///< A land block's side
inline constexpr float k_ChromaSide = 32.0f;        ///< The chroma render's side
inline constexpr float k_SixtyFourth = 0.015625f;   ///< 1 / 64
inline constexpr int k_CellLimit = 0x1FF;           ///< Cells 0..0x1FF
/// The box before the first vertex: a huge float, positive for the minimums and negative for the maximums
inline constexpr float k_BoxEmpty = std::bit_cast<float>(0x60AD78ECu);
/// A 32 x 32 texture; the grid of 128 x 64 is 4 x 2 subsamples per texel
inline constexpr int k_Texels = 32;

// ---- Fade, alpha and light ------------------------------------------------------------------------------------------

/// The two values of a land block that the fade reads: whether it is in the camera's view, and its distance to the
/// camera
struct BlockState
{
	bool exists {false}; ///< The block is in the index and loaded
	bool visible {false};
	float distance {0.0f};
};
/// The block of the cells (16 per block side, 32 blocks a row)
using BlockAt = std::function<BlockState(int blockX, int blockZ)>;

/// 0 when the block under the caster is at 100000 or more; 0 when none of the 9 blocks at (-60, 0, +60) in x and z
/// exists, is visible and is nearer than 100000 (cells outside 0..cellLimit skip the first test and count as no block);
/// else q = |(x, ground, z) - camera| / (scale * radius) gives 255 below 50, 255 - (q - 50) 255 / (80 - 50) up to 80
/// and 0 past it. `ground` is the altitude at the caster's x, z, `scale` the object's scale, `meshRadius` its mesh's
/// half diagonal (ecs::object::MeshHalfDiagonal).
[[nodiscard]] float Fade(glm::vec3 position, float ground, glm::vec3 camera, float scale, float meshRadius,
                         const BlockAt& blocks, int cellLimit = k_CellLimit);
/// base fade (1/255), truncated toward zero; 0 means not drawn
[[nodiscard]] int AlphaGeneric(float fade, int base);
/// A fade below 255 as the generic one, else 255
[[nodiscard]] int AlphaComplex(float fade, int base);
/// With the sun, the fixed (-500000, 500000, -500000); else the caster's position + (0, 15000, 0)
[[nodiscard]] glm::vec3 LightGeneric(glm::vec3 position, bool useSun);
/// The hand's light: the position + (0, 200, 0)
[[nodiscard]] glm::vec3 LightHand(glm::vec3 position);
/// The creature: d = light - position; R = mesh half diagonal x scale x 3; when the horizontal |d| < R: below 0.1
/// (a double) dx and dz + 1, then d (3D) scaled to length R unless it is 0; then dy raised to the horizontal |d| when
/// dy / horizontal < 1 (45 degrees at least)
[[nodiscard]] glm::vec3 LightCreature(glm::vec3 position, glm::vec3 light, float meshRadius, float scale);

// ---- Silhouette -----------------------------------------------------------------------------------------------------

/// What the vertices are projected with: the light, dir = caster - light and the caster's own y (its matrix ty) that
/// each vertex height is taken from
struct Projection
{
	glm::vec3 light {0.0f};
	glm::vec3 dir {0.0f};
	float baseY {0.0f};
};
/// Projection of the caster at `position` (its matrix translation) from `light`: dir = position - light, one float
/// subtraction per axis
[[nodiscard]] Projection MakeProjection(glm::vec3 position, glm::vec3 light);

/// The shadow's box (x0, x1, z0, z1) and the least k, reset to +-k_BoxEmpty before the first vertex
struct Box
{
	float x0 {k_BoxEmpty};
	float x1 {-k_BoxEmpty};
	float z0 {k_BoxEmpty};
	float z1 {-k_BoxEmpty};
	float kMin {k_BoxEmpty};
};

/// One vertex: W = M v with M's ty lowered by baseY (the skinned path does the same to its copy of the bone matrix),
/// h = max(0, W.y), k = W.z d.z + W.x d.x (the box's kMin), t = -Ly / (h - Ly) with Ly the light's ABSOLUTE y,
/// P = L.xz + (W.xz - L.xz) t; the box grows by P with no margin. `matrix` is the vertex's world matrix (the caster's,
/// or the bone's), glm column-major (matrix[c][r]).
glm::vec2 Project(const Projection& projection, const glm::mat4& matrix, glm::vec3 local, Box& box);

/// The rasterizer's grid of one shadow: 4 x 2 subsamples per texel, one byte per texel (cleared for every shadow):
/// bits 0..3 the even subrow, 4..7 the odd one, bit x & 3 for the subsample x. The original always has 32 texels;
/// another size is openblack's own (kept for the hand's look, see shadow_list).
struct Coverage
{
	explicit Coverage(int side = k_Texels);
	void Clear();
	[[nodiscard]] int GridX() const { return texels * 4; }
	[[nodiscard]] int GridZ() const { return texels * 2; }
	int texels;
	std::vector<uint8_t> bytes; ///< texels rows (z) of texels bytes (x)
};

/// Every point into the grid, px = (x - x0) (128 / (x1 - x0)) and pz = (z - z0) (64 / (z1 - z0)) (the factors stored
/// as floats first), each clamped to 0 below 0 and to 127 / 63 above
void ToGrid(const Box& box, std::span<glm::vec2> points, int texels = k_Texels);

/// One primitive: triangles of 16-bit indices into `grid`. One-sided ones (no double-sided material and not a mist)
/// are kept when (r0 - r2)(x1 - x2) >= (r1 - r2)(x0 - x2), rows r = z truncated toward zero, and walked 0 -> 2, 2 -> 1, 1 -> 0;
/// two-sided ones are walked 0 -> 2 -> 1 when (r0 - r1)(x2 - x1) < (r2 - r1) (x0 - x1), else 0 -> 1 -> 2.
/// `halfRows`: the even subrows are not written.
void RasterTriangles(std::span<const glm::vec2> grid, std::span<const uint16_t> indices, bool bothFaces, bool halfRows,
                     Coverage& coverage);

/// One shadow's texels, the alpha nibbles n of the ARGB4444 texture (black, alpha n / 15), texels x texels by rows
using Texels = std::vector<uint8_t>;
/// Rows and columns 1..texels - 2 take popcount(coverage); the outer ring is never written and keeps the texture's
/// initial 0
void Resolve(const Coverage& coverage, Texels& texels);
/// The chroma casters' blur: for rows and columns 1..30, texel(r, c) |= ((t(r, c) + t(r, c + 1) + t(r + 1, c) +
/// t(r + 1, c + 1)) / 4) & 0xF000 of the 16-bit texels `rendered` the caster's 32 x 32 shadow texture draw produced;
/// an OR of nibbles, not a max
void ChromaFilter(std::span<const uint16_t> rendered, Texels& texels);
/// When the alpha is not 255: rows and columns 1..texels - 2 become ((n << 12) a / 255) & 0xF000, that is
/// n' = floor(n a / 255) (the signed division by 255)
void BakeAlpha(Texels& texels, int alpha);

// ---- The chroma casters ---------------------------------------------------------------------------------------------

/// The texture's 64 x 64 shadow map, cached with the texture: byte (r, c) = the high byte of the 16-bit texel
/// (r h / 64, c w / 64), both truncated toward zero, & 0xF0, the ARGB4444 alpha nibble
using AlphaMap = std::array<uint8_t, 64 * 64>;
[[nodiscard]] AlphaMap MakeAlphaMap(std::span<const uint16_t> texels, int width, int height);

/// One chroma vertex: W = M v; t = (base y - Ly) / (W.y - Ly) (no clamp of h);
/// x = ((W.x - Lx) t + Lx - x0) (32 / (x1 - x0)) clamped to [1, 31], z the same; u, v = the vertex uv x 63.
/// Returns (x, z, u, v)
[[nodiscard]] glm::vec4 ChromaVertex(const Projection& projection, const Box& box, const glm::mat4& matrix, glm::vec3 local,
                                     glm::vec2 uv);
/// One chroma triangle into the target (side x side 16-bit texels): the three vertices truncated toward zero, clamped to the
/// target, sorted, the edges (16.16 x, u, v per row, rows inclusive) and the spans (inclusive, u, v stepped by an integer
/// division): each texel ORs (map[(v >> 16) 64 + (u >> 16)] & 0xE0) << 7, i.e. alpha nibble (a >> 1)
void ChromaTriangle(const std::array<glm::vec4, 3>& vertices, const AlphaMap& map, std::span<uint16_t> target, int side);

// ---- Land and objects -----------------------------------------------------------------------------------------------

/// t' = (base y - Ly) / (H - Ly), H = the altitude under the caster when there is one, and the base y without one (the
/// hand and the creature: t' = 1). One H per shadow, before the vertex loop
[[nodiscard]] float LandProjectionFactor(float baseY, float lightY, float ground);
/// bx 160 <= x1, (bx + 1) 160 >= x0 and the same in z
[[nodiscard]] bool TouchesBlock(const Box& box, int blockX, int blockZ);
/// 1.41419995, not the float nearest sqrt 2: the morphable receiver's box factor
inline constexpr float k_MorphableBoxFactor = std::bit_cast<float>(0x3FB50481u);
/// The morphable draw's own receiver test (in place of the bounding box test): the reach
/// R = (scale x mesh half diagonal) + max(x1 - x0, z1 - z0) x 1.4142 (the x side kept only when the z one is less),
/// the mesh centre through the object's matrix (x = ((cy m3 + cz m6) + cx m0) + m9, z = ((cy m5 + cx m2) + cz m8) +
/// m11; rows m0..m11 = glm columns), dx = x - bx and dz = z - bz with the box centre (x0 + x1) x 0.5 and
/// (z0 + z1) x 0.5, all stored as floats; the shadow is drawn when dx dx + dz dz < R R, strictly
[[nodiscard]] bool ReachesMorphable(const Box& box, glm::vec3 meshCentre, const glm::mat4& model, float scale,
                                    float halfDiagonal);
/// The 8 corners of the block (graphics::haze::BlockCorners) through the world-to-clipping matrix (X, Y and the depth
/// Z); outcodes Z < near, X > Z, else -Z > X, Y > Z, else -Z > Y. Not visible when one outcode holds for all 8.
[[nodiscard]] bool BlockVisible(const std::array<glm::vec3, 8>& corners, const affine::AffineMatrix& worldToClipping,
                                float nearW);

} // namespace openblack::graphics::shadow_math
