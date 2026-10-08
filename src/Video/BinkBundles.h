/*******************************************************************************
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

#include "BinkBitReader.h"
#include "BinkHuffman.h"

/// The bundles of a Bink 1 video plane: the values its blocks use are not coded where each block is, but grouped by kind
/// into nine streams. Each stream reads its next chunk before every row of blocks, and the blocks then take its values
/// in order.
namespace openblack::video::bink
{

enum class Source : uint8_t
{
	BlockTypes,    ///< the type of each 8x8 block
	SubBlockTypes, ///< the type of the 8x8 block a 16x16 block is scaled from
	Colours,       ///< pixel values
	Patterns,      ///< the 8-bit rows of two-colour pattern blocks
	XOffsets,      ///< motion, -15 to 15 pixels
	YOffsets,
	IntraDc, ///< the first DCT coefficient of intra blocks, 0 to 2047 then deltas
	InterDc, ///< the same for inter blocks, signed
	Runs,    ///< run lengths minus one, for run blocks
};
inline constexpr size_t k_SourceCount = 9;

/// The block types of Bink 1 video
enum class BlockType : uint8_t
{
	Skip,    ///< the block of the previous frame
	Scaled,  ///< a 16x16 block: an 8x8 block of another type, each pixel doubled
	Motion,  ///< a block of the previous frame, moved
	Run,     ///< runs of colours along one of 16 orders
	Residue, ///< moved, plus a small coded difference
	Intra,   ///< DCT coded
	Fill,    ///< one colour
	Inter,   ///< moved, plus a DCT coded difference
	Pattern, ///< two colours and a bit per pixel
	Raw,     ///< 64 colours
};

class Bundles
{
public:
	/// Room for the values of a plane of `blocks` 8x8 blocks
	explicit Bundles(size_t blocks);

	/// Before a plane `width` pixels wide (at least 8) and `blockWidth` blocks wide: the bit sizes of the chunk counts,
	/// then the trees. False: the packet ran out
	[[nodiscard]] bool StartPlane(BitReader& reader, uint32_t width, uint32_t blockWidth);
	/// Before each row of blocks: the next chunk of every bundle. False: the data is damaged
	[[nodiscard]] bool ReadRow(BitReader& reader);
	/// The next value of a bundle (0 past its room)
	[[nodiscard]] int32_t Next(Source source) noexcept;

private:
	struct Bundle
	{
		uint32_t countBits {0}; ///< the size of a chunk's count
		Tree tree;
		std::vector<int16_t> values;
		size_t decoded {0}; ///< values read from the packet so far
		size_t taken {0};   ///< values the blocks have taken
		bool ended {false}; ///< a chunk of 0 values: nothing more in this plane
	};

	/// The count of the next chunk, or 0 when the bundle has nothing to read now (it ended, or the blocks have not taken
	/// what it already has)
	[[nodiscard]] uint32_t ChunkSize(BitReader& reader, Bundle& bundle) noexcept;
	[[nodiscard]] bool ReadBlockTypes(BitReader& reader, Bundle& bundle);
	[[nodiscard]] bool ReadColours(BitReader& reader, Bundle& bundle);
	[[nodiscard]] bool ReadPatterns(BitReader& reader, Bundle& bundle);
	[[nodiscard]] bool ReadOffsets(BitReader& reader, Bundle& bundle);
	[[nodiscard]] bool ReadDcs(BitReader& reader, Bundle& bundle, bool hasSign);
	[[nodiscard]] bool ReadRuns(BitReader& reader, Bundle& bundle);
	[[nodiscard]] uint8_t ReadColour(BitReader& reader, const Bundle& bundle) noexcept;

	std::array<Bundle, k_SourceCount> _bundles;
	/// Colours: the high nibble is coded with one of 16 trees, chosen by the high nibble before it
	std::array<Tree, 16> _highNibbleTrees;
	uint8_t _lastHighNibble {0};
};

} // namespace openblack::video::bink
