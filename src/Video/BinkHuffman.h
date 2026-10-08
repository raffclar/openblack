/*******************************************************************************
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

#include "BinkBitReader.h"

/// The Huffman coding of Bink 1 video: every value is a 4-bit symbol coded with one of 16 fixed trees, and each use of a
/// tree carries its own order of the 16 symbols over the tree's leaves.
namespace openblack::video::bink
{

struct Tree
{
	/// Which of the 16 fixed trees (BinkTables.h k_TreeCodes); 0 is plain 4-bit values
	uint8_t index {0};
	/// The symbol of each leaf
	std::array<uint8_t, 16> symbols {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
};

/// Reads a tree header: the tree's index, then the symbols of its leaves, either as a short list (the leaves not listed
/// take the remaining symbols in increasing order) or as the bits of a merge sort. Nullopt when fewer than 4 bits are
/// left
[[nodiscard]] std::optional<Tree> ReadTree(BitReader& reader) noexcept;

/// Reads one symbol (0 to 15) coded with `tree`
[[nodiscard]] uint8_t ReadSymbol(BitReader& reader, const Tree& tree) noexcept;

} // namespace openblack::video::bink
