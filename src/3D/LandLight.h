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
#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FrameAnim.h"

namespace openblack
{
class LandIslandInterface;
class LandLightTable;
} // namespace openblack

/// The land's light per cell and vertex (wiki: docs/bw1-notes/rendering.md, "Haze and land light: the common API"). In
/// the original every LandBlock holds 17 x 17 cells of 8 bytes; the first dword is the cell's colour read as a D3DCOLOR
/// (bytes 0..2: blue, green, red; the file's r, g, b bytes) and byte 3 its luminosity. The land vertices and the models
/// take table[luminosity] (LandLightTable) as the diffuse and the colour | 0xFF000000 as the specular. Every frame the
/// light and shadow stamps are written into those bytes (before the land and the models are drawn) and taken out at
/// the end. The GPU side is assets/shaders/land_light.sh, which reads the cells of this frame (the renderer uploads
/// Cells()).
///
/// Cells are global, 0..511 along x and z (world / 10; the island's block index is 32 x 32 blocks of 16).
namespace openblack::land_light
{

/// The diffuse and specular an object is drawn with
struct Sample
{
	uint32_t diffuse {0xFFFFFFFFu};  ///< table[luminosity] (D3DCOLOR)
	uint32_t specular {0xFF000000u}; ///< the cells' colour | 0xFF000000
};

/// The land light table the renderer built last (RenderFrameSystemInterface::GetLandLightTable): what the game reads
/// when it colours something by the land's light outside the renderer. Every light 0xFFFFFFFF before the first build
[[nodiscard]] const LandLightTable& CurrentTable() noexcept;

/// table[255]: the light of the cells off the map and of the missing blocks, and of the dove, the citadel heart, the
/// clouds and the sea
[[nodiscard]] uint32_t FullLight(const LandLightTable& table) noexcept;

/// a + ((b - a) w >> 8) per RGB byte, w 0..256: the integer lerp of the model light (the logical shift with the masks
/// gives a + floor((b - a) w / 256)) and of the stamps; alpha 0
[[nodiscard]] uint32_t LerpBytes(uint32_t a, uint32_t b, int w) noexcept;

/// The cells of the land, this frame's (after the stamps) and as loaded
struct Cells
{
	glm::ivec2 firstCell {0};               ///< global cell of index 0 (the island's cell map: extent minimum / 10)
	glm::ivec2 size {0};                    ///< the island's cell map resolution
	std::vector<uint32_t> base;             ///< the dword as loaded (restored when the stamps are cleared)
	std::vector<uint32_t> cells;            ///< this frame's
	std::vector<uint8_t> covered;           ///< the cell is one of a block's 17 x 17
	std::array<uint8_t, 32 * 32> blocks {}; ///< the block [x >> 4][z >> 4] exists
	const LandIslandInterface* island {nullptr};
	uint32_t generation {0};
};
/// The cells read by At / AtCellShift / AtCell and uploaded for the shaders
[[nodiscard]] const Cells& GetCells() noexcept;
/// Replaces them (tests, and a land without blocks)
void SetCells(Cells cells);
/// The cell's dword this frame; false off 0..0x1FF or where no block covers it
[[nodiscard]] bool CellAt(const Cells& cells, glm::ivec2 cell, uint32_t& dword) noexcept;
/// The block [x >> 4][z >> 4] exists, the cell in 0..0x1FF
[[nodiscard]] bool HasBlock(const Cells& cells, glm::ivec2 cell) noexcept;
/// The luminosity bytes of this frame, for the stamps outside this module (the night lights); write them back with
/// SetLuminosity
[[nodiscard]] std::vector<uint8_t> Luminosity();
void SetLuminosity(const std::vector<uint8_t>& luminosity);
/// RGBA8 texels of this frame's cells for the shaders: r, g, b the colour as a D3DCOLOR (byte 2, 1, 0), a = luminosity
[[nodiscard]] std::vector<uint8_t> Texels();

/// The model light at a position: the cell trunc(x 0.1), trunc(z 0.1); off 0..0x1FF or in a missing block the full
/// light and specular 0xFF000000; else the 4 cells (the cell, z + 1, x + 1 and both) lerped with the integer weights
/// trunc(frac 256), along z first and then along x, the colours | 0xFF000000 and the lights table[byte 3]
[[nodiscard]] Sample At(const Cells& cells, const LandLightTable& table, glm::vec2 xz) noexcept;
[[nodiscard]] Sample At(const LandLightTable& table, glm::vec2 xz) noexcept; ///< with GetCells()
/// The same from a map cell, with the weights CellX >> 8 and CellZ >> 8, not the fraction: 0 or 1/256, so trees,
/// scaffolds and town artifacts take their cell almost unmixed. Off the map: the full light
[[nodiscard]] Sample AtCellShift(const Cells& cells, const LandLightTable& table, glm::ivec2 cell) noexcept;
/// The colour part of the altitude and colour query: the cell alone, specular = cell | 0xFF000000, diffuse =
/// table[byte 3]; off the map or in a missing block the full light
[[nodiscard]] Sample AtCell(const Cells& cells, const LandLightTable& table, glm::ivec2 cell) noexcept;

/// How a model takes the land light in vs_object (the per-mesh mode of RenderContext; u_objectLight.w = haze off +
/// 2 x mode)
enum class ObjectMode : uint8_t
{
	Bilinear = 0,  ///< At: the models
	CellShift = 1, ///< AtCellShift: trees, scaffolds, town artifacts
	Cell = 2,      ///< AtCell: worship sites, spell icons, totems
	Full = 3,      ///< FullLight alone: the dove writes the diffuse only ((inferred) the specular left at 0)
};
struct ObjectLight
{
	ObjectMode mode {ObjectMode::Bilinear};
	bool haze {true}; ///< whether the draw applies the haze after the light
};

/// Loads the cells of `island` again when the land changed (`generation`, Clouds::GetLandscapeGeneration), and then
/// puts back the loaded cells (colour 0, luminosity = the saved byte) on the stamped blocks. The land files have the
/// saved byte = the luminosity and colour 0 everywhere (checked on Land1, Land2, Norse), so the loaded cells are the
/// same
void BeginFrame(const LandIslandInterface& island, uint32_t generation);

/// A light or shadow stamp (at most 200 a frame)
struct Stamp
{
	glm::vec3 position {0.0f};       ///< its first texel's world point (after the centring)
	const uint8_t* texels {nullptr}; ///< the frame (pitch x pitch x bpp, rows along x)
	int pitch {0};
	int keepBrighter {0}; ///< mode 1 only: max instead of the saturated sum
	int alpha {0};        ///< trunc(clamp(alpha x 255, 0, 255))
	int mode {0};
};
/// Drops it past 200 stamps; alpha x 255 clamped to 0..255 and truncated; with `centre` the point moves back by
/// (pitch - 1) x 5 in x and z. Mode 1 = light (bpp 3), 2 = shadow (bpp 1); 3..8 only set a bpp and stamp nothing.
/// `texels` must stay valid until this frame's ApplyStamps.
/// Callers: the particle light maps (pos + (10, 0, 10), centred, mode by bpp, keepBrighter 0), the storm clouds
/// (sstorm.raw 40, centred, mode 2), the lightning flash (64, centred, mode 1), the map clouds (sclouds.raw 40, not
/// centred, mode 2), the dance light (not ported)
bool AddStamp(const glm::vec3& position, const uint8_t* texels, int pitch, bool centre, float alpha, int mode,
              int keepBrighter = 0);
/// Every stamp of the list into the cells (in list order)
void ApplyStamps();
/// The list emptied; the cells come back with the next BeginFrame
void ClearStamps() noexcept;
[[nodiscard]] const std::vector<Stamp>& GetStamps() noexcept;
/// (openblack) how many times ClearStamps ran: the producers that must add once a frame compare it
[[nodiscard]] uint32_t StampFrame() noexcept;

/// One stamp against cells of `size` from global cell `firstCell` (`covered` marks the cells of a block; `cells` the
/// dwords). The cell trunc(x 0.1) / trunc(z 0.1) and the weight round(255 - frac 255) (round((ix - f) 255) and ix - 1
/// below 0), & 0xFF; clipped to 0..0x1FF; then for each covered cell (ix + i, iz + j), i, j < pitch - 1: the texels
/// t(i, j), t(i, j + 1), t(i + 1, j), t(i + 1, j + 1) lerped along z by wz and then along x by wx (LerpBytes). Shadow:
/// v = 255 - (255 - r) alpha / 255, at least 0x30, luminosity = min(luminosity, v). Light: per texel byte c,
/// v = r alpha / 255 into colour byte 2 - c (texel R -> D3DCOLOR red), + v capped at 0xFF, or max with keepBrighter.
/// (approximate) the original only stamps the blocks drawn this frame: here every block
void ApplyStamp(const Stamp& stamp, glm::ivec2 firstCell, glm::ivec2 size, const std::vector<uint8_t>& covered,
                std::vector<uint32_t>& cells);

/// Loads a bitmap file under Data (".\Data\..." or "Data/..." names): graphics::frame_anim::LoadBitmapFromFile on its
/// bytes, once per name and arguments; null when it is missing or of another size. The frames' texels:
/// graphics::frame_anim::FrameTexels
[[nodiscard]] std::shared_ptr<const graphics::frame_anim::StackedFrames>
LoadBitmapFile(const std::string& path, int pitch, int bpp, int framesInFile, int framesInUse);
/// The bitmap file `name` (relative to Data) decoded uncached: LightBitmapLoader's read; null when it is missing or
/// of another size
[[nodiscard]] std::shared_ptr<graphics::frame_anim::StackedFrames> ReadBitmapFile(const std::string& name, int pitch, int bpp,
                                                                                  int framesInFile, int framesInUse);

} // namespace openblack::land_light
