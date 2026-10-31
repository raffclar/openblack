/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <vector>

#include <LNDFile.h>

namespace openblack
{
/// Everything a land is built from: its blocks, where each lies on the map, and what paints them. It is read from a
/// landscape file or generated, as the flat testbed is.
struct LandData
{
	/// The map is 32 by 32 blocks of 16 by 16 cells
	static constexpr int k_BlocksPerSide = 32;
	/// The most blocks that the byte table of a landscape file can name
	static constexpr size_t k_MostBlocksInTable = 255;

	/// For each block of the map at [x * 32 + z], one more than its index in blocks, or 0 where there is no land. A
	/// landscape file holds a byte for each, so has at most 255 blocks; the whole map takes 1024.
	std::array<uint16_t, k_BlocksPerSide * k_BlocksPerSide> blockIndexLookup {};
	std::vector<lnd::LNDBlock> blocks;
	std::vector<lnd::LNDCountry> countries;
	std::vector<lnd::LNDMaterial> materials;
	/// The noise and bump maps, 256 by 256 texels each, [x * 256 + z]
	std::vector<uint8_t> noise;
	std::vector<uint8_t> bump;
	/// The land's coarse texture pages, as the landscape file keeps them: each block's coarse texture is a 64 texel
	/// square of one of them (see LNDBlock::lowResTexture, fuLrs and fvLrs)
	std::vector<lnd::LNDLowResolutionTexture> coarsePages;

	/// The land of a landscape file. The table of the file names each block by a byte, so a land of more than 255 blocks
	/// takes its table from the place that each block keeps (its block x and z).
	[[nodiscard]] static LandData FromLnd(const lnd::LNDFile& lnd);

	/// The bytes of a landscape file that holds this land. The file has the layout of the game's landscape files. The
	/// byte of each block in the table is 0 for a block after the 255th: such a land reads back from the places of the
	/// blocks.
	[[nodiscard]] std::vector<uint8_t> LndBytes() const;
};
} // namespace openblack
