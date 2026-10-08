/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The bit level of the Bink 1 video decoder: video::bink::BitReader, the Huffman trees and the bundles
// (src/Video/BinkBitReader.h, BinkHuffman.h, BinkBundles.h), with hand-written bitstreams.

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Video/BinkBitReader.h"
#include "Video/BinkBundles.h"
#include "Video/BinkHuffman.h"
#include "Video/BinkTables.h"
#include "support/BinkBitWriter.h"

using namespace openblack::video::bink;
using openblack::test::BinkBitWriter;

namespace
{
/// Every tree of a plane's bundles as tree 0 (plain 4-bit symbols): 7 bundles with a tree, plus the 16 trees of the
/// colours' high nibble
void PlainTrees(BinkBitWriter& w)
{
	for (int i = 0; i < 7 + 16; ++i)
	{
		w.Put(0, 4);
	}
}

/// The count bits of every bundle of a plane 16 pixels (2 blocks) wide
constexpr uint32_t k_CountBits = 10;

/// A chunk of `count` copies of `value` (block types and runs)
void Repeat(BinkBitWriter& w, uint32_t count, uint32_t value)
{
	w.Put(count, k_CountBits).Bit(true).Put(value, 4);
}

/// The chunk count 0: the bundle has ended for this plane
void End(BinkBitWriter& w)
{
	w.Put(0, k_CountBits);
}
} // namespace

TEST(BinkBitReader, ReadsEachByteFromItsLowestBit)
{
	const std::vector<uint8_t> data = {0b1011'0010, 0xFF, 0x00};
	BitReader reader(data);
	EXPECT_EQ(reader.Read(1), 0u);
	EXPECT_EQ(reader.Read(3), 0b001u);
	EXPECT_EQ(reader.Read(4), 0b1011u);
	EXPECT_EQ(reader.Peek(8), 0xFFu);
	EXPECT_EQ(reader.Read(8), 0xFFu);
	EXPECT_EQ(reader.Position(), 16u);
	EXPECT_EQ(reader.BitsLeft(), 8);
}

TEST(BinkBitReader, ReadsThirtyTwoBitsAcrossBytes)
{
	const std::vector<uint8_t> data = {0x80, 0x67, 0x45, 0x23, 0x01};
	BitReader reader(data);
	reader.Skip(4);
	// the 40 bits are 0x0123456780: past the first 4, the next 32
	EXPECT_EQ(reader.Read(32), 0x12345678u);
}

TEST(BinkBitReader, Aligns32)
{
	const std::vector<uint8_t> data(8, 0);
	BitReader reader(data);
	reader.Align32();
	EXPECT_EQ(reader.Position(), 0u);
	reader.Skip(5);
	reader.Align32();
	EXPECT_EQ(reader.Position(), 32u);
	reader.Align32();
	EXPECT_EQ(reader.Position(), 32u);
}

TEST(BinkBitReader, ReadsZerosPastTheEnd)
{
	const std::vector<uint8_t> data = {0xFF};
	BitReader reader(data);
	EXPECT_FALSE(reader.Overrun());
	EXPECT_EQ(reader.Read(4), 0xFu);
	EXPECT_EQ(reader.Read(8), 0xFu);
	EXPECT_TRUE(reader.Overrun());
	EXPECT_EQ(reader.Read(32), 0u);
	EXPECT_EQ(reader.BitsLeft(), -36);
}

TEST(BinkHuffman, EveryTreeCodesEverySymbol)
{
	for (uint8_t index = 0; index < 16; ++index)
	{
		uint32_t kraft = 0; // the sum of 2^(7 - length): a complete prefix code fills all 128
		for (uint8_t leaf = 0; leaf < 16; ++leaf)
		{
			const uint32_t length = k_TreeLengths[index][leaf];
			kraft += 1u << (k_MaxCodeLength - length);
			BinkBitWriter w;
			w.Put(k_TreeCodes[index][leaf], length).Put(0x55, 8);
			BitReader reader(w.Bytes());
			const Tree tree {index, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
			EXPECT_EQ(ReadSymbol(reader, tree), leaf) << "tree " << int {index};
			EXPECT_EQ(reader.Position(), length) << "tree " << int {index};
			EXPECT_EQ(reader.Read(8), 0x55u);
		}
		EXPECT_EQ(kraft, 128u) << "tree " << int {index};
	}
}

TEST(BinkHuffman, SymbolsFollowTheTreeOrder)
{
	Tree tree {0, {}};
	for (uint8_t leaf = 0; leaf < 16; ++leaf)
	{
		tree.symbols[leaf] = static_cast<uint8_t>(15 - leaf);
	}
	BinkBitWriter w;
	w.Put(3, 4);
	BitReader reader(w.Bytes());
	EXPECT_EQ(ReadSymbol(reader, tree), 12);
}

TEST(BinkHuffman, TreeZeroIsPlainValues)
{
	BinkBitWriter w;
	w.Put(0, 4).Put(0xF, 4);
	BitReader reader(w.Bytes());
	const auto tree = ReadTree(reader);
	ASSERT_TRUE(tree.has_value());
	EXPECT_EQ(tree->index, 0);
	EXPECT_EQ(reader.Position(), 4u);
	for (uint8_t i = 0; i < 16; ++i)
	{
		EXPECT_EQ(tree->symbols[i], i);
	}
}

TEST(BinkHuffman, TreeWithAListedOrder)
{
	// Tree 3, listed: 3 symbols (7, 2, 9), then the others in increasing order
	BinkBitWriter w;
	w.Put(3, 4).Bit(true).Put(2, 3).Put(7, 4).Put(2, 4).Put(9, 4);
	BitReader reader(w.Bytes());
	const auto tree = ReadTree(reader);
	ASSERT_TRUE(tree.has_value());
	EXPECT_EQ(tree->index, 3);
	const std::array<uint8_t, 16> expected = {7, 2, 9, 0, 1, 3, 4, 5, 6, 8, 10, 11, 12, 13, 14, 15};
	EXPECT_EQ(tree->symbols, expected);
	EXPECT_EQ(reader.Position(), w.Bits());
}

TEST(BinkHuffman, TreeWithAMergedOrder)
{
	// Tree 5, merged, two passes: the first swaps every pair (8 merges of one bit each), the second takes the second
	// pair first in each group of four (4 merges, bits 1 1)
	BinkBitWriter w;
	w.Put(5, 4).Bit(false).Put(1, 2);
	for (int i = 0; i < 8; ++i)
	{
		w.Bit(true);
	}
	for (int i = 0; i < 4; ++i)
	{
		w.Bit(true).Bit(true);
	}
	BitReader reader(w.Bytes());
	const auto tree = ReadTree(reader);
	ASSERT_TRUE(tree.has_value());
	const std::array<uint8_t, 16> expected = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12};
	EXPECT_EQ(tree->symbols, expected);
	EXPECT_EQ(reader.Position(), w.Bits());
}

TEST(BinkHuffman, NoTreeWithoutFourBits)
{
	const std::vector<uint8_t> none;
	BitReader reader(none);
	EXPECT_FALSE(ReadTree(reader).has_value());
}

TEST(BinkBundles, BlockTypesWithRunCodes)
{
	BinkBitWriter w;
	PlainTrees(w);
	// Block types: 6 coded, 3 then code 12 (the last type 4 more times) then 5
	w.Put(6, k_CountBits).Bit(false).Put(3, 4).Put(12, 4).Put(5, 4);
	for (int i = 0; i < 8; ++i)
	{
		End(w);
	}
	BitReader reader(w.Bytes());
	Bundles bundles(4);
	ASSERT_TRUE(bundles.StartPlane(reader, 16, 2));
	ASSERT_TRUE(bundles.ReadRow(reader));
	EXPECT_EQ(reader.Position(), w.Bits());
	for (const int expected : {3, 3, 3, 3, 3, 5})
	{
		EXPECT_EQ(bundles.Next(Source::BlockTypes), expected);
	}
}

TEST(BinkBundles, AChunkWaitsUntilItsValuesAreTaken)
{
	BinkBitWriter w;
	PlainTrees(w);
	Repeat(w, 2, 6);
	for (int i = 0; i < 8; ++i)
	{
		End(w);
	}
	Repeat(w, 2, 9);
	BitReader reader(w.Bytes());
	Bundles bundles(4);
	ASSERT_TRUE(bundles.StartPlane(reader, 16, 2));
	ASSERT_TRUE(bundles.ReadRow(reader));
	const size_t afterFirst = reader.Position();
	// Nothing taken yet: the next row reads nothing; ended bundles never read again
	ASSERT_TRUE(bundles.ReadRow(reader));
	EXPECT_EQ(reader.Position(), afterFirst);
	EXPECT_EQ(bundles.Next(Source::BlockTypes), 6);
	EXPECT_EQ(bundles.Next(Source::BlockTypes), 6);
	ASSERT_TRUE(bundles.ReadRow(reader));
	EXPECT_EQ(reader.Position(), w.Bits());
	EXPECT_EQ(bundles.Next(Source::BlockTypes), 9);
}

TEST(BinkBundles, ColoursTakeTheirHighNibbleTree)
{
	BinkBitWriter w;
	// Every tree plain, except the high nibble tree used after a high nibble of 0xA: tree 0 with its symbols reversed
	for (int i = 0; i < 2; ++i)
	{
		w.Put(0, 4); // block and sub-block types
	}
	for (int i = 0; i < 16; ++i)
	{
		if (i == 0xA)
		{
			// listed order: 8 symbols 15..8, the rest 0..7
			w.Put(1, 4).Bit(true).Put(7, 3);
			for (uint32_t s = 15; s >= 8; --s)
			{
				w.Put(s, 4);
			}
		}
		else
		{
			w.Put(0, 4);
		}
	}
	for (int i = 0; i < 5; ++i)
	{
		w.Put(0, 4); // colours, patterns, offsets, runs
	}
	End(w);
	End(w);
	// Colours: 2 coded, 0xA5 (high nibble with tree 0, low 5), then 0x?3 with the high nibble from tree [0xA]: tree 1's
	// first leaf (code 0, 1 bit) is its listed symbol 15
	w.Put(2, k_CountBits).Bit(false).Put(0xA, 4).Put(5, 4).Put(0, 1).Put(3, 4);
	for (int i = 0; i < 6; ++i)
	{
		End(w);
	}
	BitReader reader(w.Bytes());
	Bundles bundles(4);
	ASSERT_TRUE(bundles.StartPlane(reader, 16, 2));
	ASSERT_TRUE(bundles.ReadRow(reader));
	EXPECT_EQ(reader.Position(), w.Bits());
	EXPECT_EQ(bundles.Next(Source::Colours), 0xA5);
	EXPECT_EQ(bundles.Next(Source::Colours), 0xF3);
}

TEST(BinkBundles, PatternsAndOffsets)
{
	BinkBitWriter w;
	PlainTrees(w);
	End(w);
	End(w);
	End(w);
	// Patterns: low nibble then high
	w.Put(1, k_CountBits).Put(0x4, 4).Put(0xC, 4);
	// X offsets coded: 5 with sign (-5), 0 (no sign bit), 15 without sign
	w.Put(3, k_CountBits).Bit(false).Put(5, 4).Bit(true).Put(0, 4).Put(15, 4).Bit(false);
	// Y offsets: 2 copies of -7
	w.Put(2, k_CountBits).Bit(true).Put(7, 4).Bit(true);
	End(w);
	End(w);
	End(w);
	BitReader reader(w.Bytes());
	Bundles bundles(4);
	ASSERT_TRUE(bundles.StartPlane(reader, 16, 2));
	ASSERT_TRUE(bundles.ReadRow(reader));
	EXPECT_EQ(reader.Position(), w.Bits());
	EXPECT_EQ(bundles.Next(Source::Patterns), 0xC4);
	EXPECT_EQ(bundles.Next(Source::XOffsets), -5);
	EXPECT_EQ(bundles.Next(Source::XOffsets), 0);
	EXPECT_EQ(bundles.Next(Source::XOffsets), 15);
	EXPECT_EQ(bundles.Next(Source::YOffsets), -7);
	EXPECT_EQ(bundles.Next(Source::YOffsets), -7);
}

TEST(BinkBundles, DcsAreDeltas)
{
	BinkBitWriter w;
	PlainTrees(w);
	for (int i = 0; i < 6; ++i)
	{
		End(w);
	}
	// Intra DCs: 3 values, the first 1000 in 11 bits, then a group of 2 with 3-bit deltas: +2 and 0
	w.Put(3, k_CountBits).Put(1000, 11).Put(3, 4).Put(2, 3).Bit(false).Put(0, 3);
	// Inter DCs: 2 values, the first -5 (10 bits and a sign), then a group of 1 with no deltas (bit size 0)
	w.Put(2, k_CountBits).Put(5, 10).Bit(true).Put(0, 4);
	// Runs: 3 coded values
	w.Put(3, k_CountBits).Bit(false).Put(1, 4).Put(0, 4).Put(14, 4);
	BitReader reader(w.Bytes());
	Bundles bundles(4);
	ASSERT_TRUE(bundles.StartPlane(reader, 16, 2));
	ASSERT_TRUE(bundles.ReadRow(reader));
	EXPECT_EQ(reader.Position(), w.Bits());
	EXPECT_EQ(bundles.Next(Source::IntraDc), 1000);
	EXPECT_EQ(bundles.Next(Source::IntraDc), 1002);
	EXPECT_EQ(bundles.Next(Source::IntraDc), 1002);
	EXPECT_EQ(bundles.Next(Source::InterDc), -5);
	EXPECT_EQ(bundles.Next(Source::InterDc), -5);
	EXPECT_EQ(bundles.Next(Source::Runs), 1);
	EXPECT_EQ(bundles.Next(Source::Runs), 0);
	EXPECT_EQ(bundles.Next(Source::Runs), 14);
}

TEST(BinkBundles, RefusesMoreValuesThanItsRoom)
{
	// One block has room for 64 block types
	BinkBitWriter w;
	PlainTrees(w);
	Repeat(w, 65, 1);
	BitReader reader(w.Bytes());
	Bundles bundles(1);
	ASSERT_TRUE(bundles.StartPlane(reader, 16, 2));
	EXPECT_FALSE(bundles.ReadRow(reader));
}

TEST(BinkBundles, RefusesARunPastTheChunk)
{
	BinkBitWriter w;
	PlainTrees(w);
	// 3 values, but code 13 asks for 8 copies
	w.Put(3, k_CountBits).Bit(false).Put(2, 4).Put(13, 4);
	BitReader reader(w.Bytes());
	Bundles bundles(4);
	ASSERT_TRUE(bundles.StartPlane(reader, 16, 2));
	EXPECT_FALSE(bundles.ReadRow(reader));
}

TEST(BinkBundles, StartPlaneNeedsItsTrees)
{
	BinkBitWriter w;
	w.Put(0, 4).Put(0, 4);
	BitReader reader(w.Bytes());
	Bundles bundles(4);
	EXPECT_FALSE(bundles.StartPlane(reader, 16, 2));
}
