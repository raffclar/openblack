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
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The land's level of detail: how far ahead of the camera its blocks keep every corner and their full texture.
///
/// Three lines cross the ground ahead of the camera, square to its view: two mesh lines and a texture line. A block
/// wholly before the first mesh line is drawn at every cell corner; past it at every second corner (the half mesh), and
/// past the second line at every fourth (the quarter mesh). A block a line runs through morphs its corners towards the
/// coarser mesh across the line's band, so the land never pops. Where a coarser block borders a finer one, the finer
/// block's edge strip is stitched to the coarser corners so no crack opens between them. The texture line does the same
/// for the texture: past it the land is drawn with a coarse texture of its block, cross-faded across the line's band.
///
/// The game took the lines from the player's CPU; openblack takes them from the detail level (see For), all of them
/// the game's own values.
namespace openblack::graphics::land_detail
{

/// Where the lines are, in units ahead of the camera, and how wide their bands reach to either side
struct Lines
{
	/// The first mesh line: the half mesh beyond it
	float meshNear {1600.0f};
	/// The second mesh line: the quarter mesh beyond it
	float meshFar {2500.0f};
	/// How far either side of a mesh line its morph reaches
	float meshBand {275.0f};
	/// The texture line: the coarse texture beyond it
	float texture {800.0f};
	/// How far either side of the texture line its cross-fade reaches
	float textureBand {85.0f};

	bool operator==(const Lines&) const = default;
};

/// The lines at each detail level, 0 to 6
inline constexpr std::array<Lines, 7> k_Lines = {{
    {.meshNear = 300.0f, .meshFar = 600.0f, .meshBand = 25.0f, .texture = 300.0f, .textureBand = 20.0f},
    {.meshNear = 300.0f, .meshFar = 600.0f, .meshBand = 25.0f, .texture = 400.0f, .textureBand = 35.0f},
    {.meshNear = 1600.0f, .meshFar = 2500.0f, .meshBand = 275.0f, .texture = 600.0f, .textureBand = 85.0f},
    {.meshNear = 1600.0f, .meshFar = 2500.0f, .meshBand = 275.0f, .texture = 700.0f, .textureBand = 85.0f},
    {.meshNear = 1600.0f, .meshFar = 2500.0f, .meshBand = 275.0f, .texture = 800.0f, .textureBand = 85.0f},
    {.meshNear = 1600.0f, .meshFar = 2500.0f, .meshBand = 275.0f, .texture = 800.0f, .textureBand = 85.0f},
    {.meshNear = 1600.0f, .meshFar = 2500.0f, .meshBand = 275.0f, .texture = 800.0f, .textureBand = 85.0f},
}};

/// The lines of a detail level; any level above the highest is the highest
[[nodiscard]] Lines For(uint8_t level);

/// The texture line meets the ground no higher than this: the camera's height, or this if the camera is higher
inline constexpr float k_TextureGroundCap = 0.67f * 165.0f;

/// A line across the ground ahead of the camera: where the plane square to the camera's view, a given depth ahead of
/// it, meets level ground
struct ViewLine
{
	/// The camera's x and z
	glm::vec2 camera {0.0f};
	/// The direction the camera faces along the ground, of unit length
	glm::vec2 forward {1.0f, 0.0f};
	/// How far the line lies from the camera along forward
	float ahead {0.0f};
	/// False when the camera looks straight up or down, and the plane meets the ground nowhere
	bool valid {false};
};

/// The line where the plane at depth units along the camera's view meets the ground at groundHeight
[[nodiscard]] ViewLine LineAcross(glm::vec3 cameraPosition, glm::vec3 cameraForward, float depth, float groundHeight);

/// How far a point on the ground is before the line, towards the camera; negative beyond it. Every point counts as on
/// the line when the line is not valid.
[[nodiscard]] float Offset(const ViewLine& line, glm::vec2 xz);

/// The mesh a block is drawn with
enum class Mesh : uint8_t
{
	Full,
	Half,
	Quarter,
};

/// Which coarser mesh a block's corners are moving towards
enum class Morph : uint8_t
{
	None,
	/// The full mesh's odd corners towards the half mesh, across the first line's band
	ToHalf,
	/// The half mesh's corners between every fourth towards the quarter mesh, across the second line's band
	ToQuarter,
};

/// The texture a block is drawn with
enum class Texture : uint8_t
{
	Full,
	/// The full texture with the coarse one faded in over it, by each corner's place in the texture line's band
	CrossFade,
	Coarse,
};

struct BlockDetail
{
	Mesh mesh {Mesh::Full};
	Morph morph {Morph::None};
	Texture texture {Texture::Full};

	bool operator==(const BlockDetail&) const = default;
};

/// A block's four corners' offsets from each line (see Offset)
struct CornerOffsets
{
	std::array<float, 4> meshNear {};
	std::array<float, 4> meshFar {};
	std::array<float, 4> texture {};
};

/// How a block is drawn. A block a line's band touches, or that the line runs through, takes the finer mesh and morphs
/// towards the coarser one; the texture likewise fades. With coarseLand every block takes the coarse texture.
[[nodiscard]] BlockDetail Classify(const CornerOffsets& offsets, const Lines& lines, bool coarseLand);

/// The sides of a block whose neighbour is drawn with a coarser mesh, one bit each
enum CoarserSide : uint8_t
{
	CoarserLowX = 1,
	CoarserLowZ = 2,
	CoarserHighZ = 4,
	CoarserHighX = 8,
};

/// The coarser sides of a block of the given mesh among its neighbours at x - 1, z - 1, z + 1 and x + 1 (none where a
/// neighbour is missing)
[[nodiscard]] uint8_t CoarserSides(Mesh mesh, std::span<const std::optional<Mesh>, 4> lowXLowZHighZHighX);

/// What the triangles of a block need of each of its 17 by 17 cells, [x * 17 + z]
struct CellFlags
{
	/// The cell's diagonal runs from (x, z + 1) to (x + 1, z) rather than from (x, z) to (x + 1, z + 1)
	bool split {false};
	/// Open sea: the land isn't drawn there
	bool openSea {false};
};

inline constexpr size_t k_CornersPerSide = 17;
inline constexpr size_t k_CornerCount = k_CornersPerSide * k_CornersPerSide;

/// The triangles of a block, as indices of its 17 by 17 corners ([x * 17 + z]), each turning the same way. Each quad
/// of the mesh's grid is split along its diagonal. The edge strips beside coarser sides are stitched to every second
/// corner of the strip's edge. Open-sea quads are left out only when dropOpenSea (the game does it only for blocks
/// wholly before the first line).
///
/// As the game builds them, a quad's diagonal and open-sea flag come from the cell found by counting one cell per quad
/// and one more per row from the block's first cell, which is the quad's own cell only for the full mesh with no
/// stitched side; the other quads take another cell's flags (vanilla bug, kept).
[[nodiscard]] std::vector<uint16_t> BlockTriangles(Mesh mesh, uint8_t coarserSides, bool dropOpenSea,
                                                   std::span<const CellFlags, k_CornerCount> cells);

/// The altitudes a corner morphs towards: the average of the two corners either side of it on the coarser mesh, as
/// whole altitudes rounded down; a corner of the coarser mesh keeps its own
struct MorphTargets
{
	uint8_t half {0};
	uint8_t quarter {0};
};

/// The morph targets of the corner at (x, z) of a block whose corner altitudes are [x * 17 + z]
[[nodiscard]] MorphTargets TargetsAt(std::span<const uint8_t, k_CornerCount> altitudes, size_t x, size_t z);

/// A corner's altitude as it is drawn while it morphs: its own before the band, the target beyond it, and between them
/// blended in 256ths, rounded down to a whole altitude
[[nodiscard]] uint8_t MorphedAltitude(uint8_t own, uint8_t target, float offset, float band);

/// How opaque the coarse texture is over the full one at a corner, 0 to 255: clear before the band, opaque beyond it
[[nodiscard]] uint8_t CrossFadeAlpha(float offset, float band);

/// The coarse texture of a block is 64 texels square: 62 of them sampled from the block's 256 texel texture, with a
/// texel round the edge shared with the neighbouring blocks. The texel of the block's texture a coarse texel (1 to 62)
/// is sampled from, along one side.
[[nodiscard]] uint16_t CoarseSourceTexel(uint8_t coarseTexel);

/// Where a point a fraction across its block (0 to 1) falls on its coarse texture, in coarse texels
[[nodiscard]] float CoarseTexelPosition(float fraction);

} // namespace openblack::graphics::land_detail
