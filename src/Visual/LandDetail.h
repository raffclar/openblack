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

/// A detail line as the game keeps it from frame to frame: a direction along the line and a point on it, x and z. The
/// morph of the corners and the cross-fade of the texture find each corner's side of the line from it.
struct GroundLine
{
	/// The direction along the line. It is of unit length, except after a frame with the camera straight up or down.
	glm::vec2 along {1.0f, 0.0f};
	/// A point on the line
	glm::vec2 point {0.0f};

	bool operator==(const GroundLine&) const = default;
};

/// The line where the plane depth units along the camera's view meets level ground at groundHeight, in the game's order
/// of float steps. When the camera looks straight up or down, the plane meets the ground nowhere. Then, as in the game,
/// the line keeps the point of the line before, and its direction is the view's square along the ground, of the
/// view's length along it.
[[nodiscard]] GroundLine GroundLineAcross(glm::vec3 cameraPosition, glm::vec3 cameraForward, float depth, float groundHeight,
                                          const GroundLine& before);

/// How far a point is before the line, towards the camera; negative beyond it. x and z are the point's place less the
/// line's point.
[[nodiscard]] float SideOf(const GroundLine& line, float x, float z);

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

/// A corner's altitude as it is drawn while it morphs, by its side of the mesh line (see SideOf): its own before the
/// band, the target beyond it, and between them blended in 256ths, rounded down to a whole altitude. The band is width
/// units wide, half of it on each side of the line.
[[nodiscard]] uint8_t MorphedAltitude(uint8_t own, uint8_t target, float side, float width);

/// The side of the mesh line (see SideOf) of the corner (x, z) of a block that morphs. The game finds each corner's
/// place from the block's corner with the least x and z, in steps of the coarser mesh, and these float sums do the
/// same.
[[nodiscard]] float MorphCornerSide(const GroundLine& line, Morph morph, glm::vec2 blockCorner, size_t x, size_t z);

/// The altitudes at which the corners of a block that morphs are drawn, [x * 17 + z], from the corners' own altitudes
/// and the morph's line and band width
[[nodiscard]] std::array<uint8_t, k_CornerCount> MorphedCorners(std::span<const uint8_t, k_CornerCount> altitudes, Morph morph,
                                                                const GroundLine& line, float width, glm::vec2 blockCorner);

/// How opaque the coarse texture is over the full one at a corner, 0 to 255, by its side of the texture line (see
/// SideOf): clear before the band, opaque beyond it. The band is width units wide, band of them on each side of the
/// line.
[[nodiscard]] uint8_t CrossFadeAlpha(float side, float width, float band);

/// How opaque the coarse texture of a block that cross-fades is at each of its corners, [x * 17 + z]
[[nodiscard]] std::array<uint8_t, k_CornerCount> CrossFadeCorners(const GroundLine& line, float width, float band,
                                                                  glm::vec2 blockCorner);

/// The land's vertex shader reads the blocks' corners as they are drawn in a frame from one texture: 32 by 32 squares,
/// one for each block in the order of the blocks, of 17 by 17 texels, the corner's x along the width and z along the
/// height. Each texel holds the drawn altitude and the coarse texture's opacity, a byte each. There is a square for
/// each block of the whole map, because a land made by the game can have more blocks than a landscape file.
inline constexpr size_t k_CornerSquaresPerSide = 32;
inline constexpr size_t k_CornerTextureSide = k_CornerSquaresPerSide * k_CornersPerSide;

/// The texel of the corner texture that holds the corner (x, z) of a block
[[nodiscard]] constexpr size_t CornerTexel(size_t block, size_t x, size_t z)
{
	const size_t column = (block % k_CornerSquaresPerSide) * k_CornersPerSide + x;
	const size_t row = (block / k_CornerSquaresPerSide) * k_CornersPerSide + z;
	return row * k_CornerTextureSide + column;
}

/// Where a block of the land is: its corner of least x and z, and its column and row on the map's 32 by 32 grid
struct BlockPlace
{
	glm::vec2 corner {0.0f};
	glm::ivec2 at {0};

	bool operator==(const BlockPlace&) const = default;
};

/// What the detail of the blocks in a frame comes from: the camera and the detail settings
struct FrameInputs
{
	glm::vec3 origin {0.0f};
	glm::vec3 forward {0.0f, 0.0f, 1.0f};
	Lines lines;
	bool coarseLand {false};

	bool operator==(const FrameInputs&) const = default;
};

/// How each block of the land is drawn in a frame, in the order of the blocks given
struct FrameDetail
{
	Lines lines;
	ViewLine meshNear;
	ViewLine meshFar;
	ViewLine texture;
	/// The lines as the game keeps them, which the morph and the cross-fade use (see GroundLineAcross)
	GroundLine meshNearGround;
	GroundLine meshFarGround;
	GroundLine textureGround;
	std::vector<BlockDetail> blocks;
	/// The sides of each block whose neighbour has a coarser mesh (see CoarserSides)
	std::vector<uint8_t> coarserSides;
	/// The indices of the blocks at x - 1, z - 1, z + 1 and x + 1 of each block, -1 where there is none
	std::vector<std::array<int16_t, 4>> neighbours;
};

/// The mesh, morph, texture and coarser sides of each block in a frame. The lines cross the ground ahead of the camera.
/// The texture line meets the ground no higher than k_TextureGroundCap. The lines that the game keeps start from those
/// of the frame before, when there is one.
[[nodiscard]] FrameDetail ClassifyFrame(const FrameInputs& inputs, std::span<const BlockPlace> blocks,
                                        const FrameDetail* before = nullptr);

/// The altitude at which the corner (x, z) of a block is drawn in a frame. This is the corner's own altitude. While the
/// block morphs, the altitude goes towards the coarser mesh by the corner's place in the band of the morph's line. The
/// corner altitudes are [x * 17 + z], and blockCorner is the corner of the block with the least x and z.
[[nodiscard]] uint8_t DrawnCornerAltitude(std::span<const uint8_t, k_CornerCount> altitudes, size_t x, size_t z, Morph morph,
                                          const FrameDetail& frame, glm::vec2 blockCorner);

/// The detail of the last frame. A frame with the same inputs and the same blocks uses it again. Thus all the users in a
/// frame share one classification.
class FrameMemo
{
public:
	[[nodiscard]] const FrameDetail& For(const FrameInputs& inputs, std::span<const BlockPlace> blocks);
	/// How many times the memo classified a frame
	[[nodiscard]] uint64_t Classifications() const { return _classifications; }

private:
	std::optional<FrameInputs> _inputs;
	std::vector<BlockPlace> _blocks;
	/// Whether _frame holds a classification, whose kept lines the next one starts from
	bool _classified {false};
	FrameDetail _frame;
	uint64_t _classifications {0};
};

/// How a block's coarse texture takes its texels from the block's texture. The coarse texture is a quarter of the
/// block's texture along each side. A texel round its edge is shared with the neighbouring blocks, and the game samples
/// the texels between them from the block's texture.
struct CoarseSampling
{
	/// Texels along a side of the block's texture, and of the page that holds the coarse textures
	uint16_t blockTexels;
	/// The coarse texels sampled along a side, between the shared texels
	uint8_t samples;
	/// The step from one sample to the next, in 65536ths of a texel of the block's texture
	uint32_t step;
	/// The coarse texels a block covers along a side when it is drawn
	float drawnTexels;
};

/// The full textures: 62 samples of a 256 texel texture into a 64 texel coarse texture, drawn 62 texels across
inline constexpr CoarseSampling k_FullCoarseSampling {.blockTexels = 256, .samples = 62, .step = 0x10842, .drawnTexels = 62.0f};
/// The low resolution textures: 30 samples of a 128 texel texture into a 32 texel coarse texture, drawn 31 texels across
inline constexpr CoarseSampling k_LowCoarseSampling {.blockTexels = 128, .samples = 30, .step = 0x11111, .drawnTexels = 31.0f};

/// The coarse sampling for a block texture of this many texels along a side
[[nodiscard]] constexpr const CoarseSampling& CoarseSamplingFor(int blockTexels)
{
	return blockTexels == k_LowCoarseSampling.blockTexels ? k_LowCoarseSampling : k_FullCoarseSampling;
}

/// The texel of the block's texture that a coarse texel (1 to the samples) takes its colour from, along one side
[[nodiscard]] uint16_t CoarseSourceTexel(uint8_t coarseTexel, const CoarseSampling& sampling = k_FullCoarseSampling);

/// Where a point a fraction across its block (0 to 1) falls on its coarse texture, in coarse texels
[[nodiscard]] float CoarseTexelPosition(float fraction, const CoarseSampling& sampling = k_FullCoarseSampling);

} // namespace openblack::graphics::land_detail
