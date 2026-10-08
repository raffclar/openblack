/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The 8x8 blocks of the Bink 1 video decoder (src/Video/BinkBlocks.h): the DCT coefficients and residues read from
// hand-written bitstreams, the inverse DCT, and the block copies, fills and scaling.

#include <cstdint>

#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "Video/BinkBitReader.h"
#include "Video/BinkBlocks.h"
#include "Video/BinkTables.h"
#include "support/BinkBitWriter.h"

using namespace openblack::video::bink;
using openblack::test::BinkBitWriter;

TEST(BinkBlocks, NoCoefficients)
{
	BinkBitWriter w;
	w.Put(0, 4).Put(5, 4); // no bit planes, quantiser 5
	BitReader reader(w.Bytes());
	Coefficients block {};
	CodedCoefficients coded;
	const auto quantiser = ReadDctCoefficients(reader, block, coded);
	ASSERT_TRUE(quantiser.has_value());
	EXPECT_EQ(*quantiser, 5u);
	EXPECT_EQ(coded.count, 0u);
	EXPECT_EQ(reader.Position(), w.Bits());
}

TEST(BinkBlocks, CoefficientsOfTheLowestPlane)
{
	// One bit plane (0): the three groups of 16 stay quiet, the candidates 1 and 3 become +1 and -1
	BinkBitWriter w;
	w.Put(1, 4);
	w.Bit(false).Bit(false).Bit(false); // groups at 4, 24, 44
	w.Bit(true).Bit(false);             // coefficient 1: +1
	w.Bit(false);                       // coefficient 2
	w.Bit(true).Bit(true);              // coefficient 3: -1
	w.Put(7, 4);                        // quantiser
	BitReader reader(w.Bytes());
	Coefficients block {};
	CodedCoefficients coded;
	const auto quantiser = ReadDctCoefficients(reader, block, coded);
	ASSERT_TRUE(quantiser.has_value());
	EXPECT_EQ(*quantiser, 7u);
	ASSERT_EQ(coded.count, 2u);
	EXPECT_EQ(coded.positions[0], 1);
	EXPECT_EQ(coded.positions[1], 3);
	EXPECT_EQ(block[k_CoefficientScan[1]], 1);
	EXPECT_EQ(block[k_CoefficientScan[3]], -1);
	EXPECT_EQ(std::count_if(block.begin(), block.end(), [](int32_t v) { return v != 0; }), 2);
	EXPECT_EQ(reader.Position(), w.Bits());
}

TEST(BinkBlocks, CoefficientsOfSplitGroups)
{
	BinkBitWriter w;
	w.Put(2, 4); // bit planes 1 and 0
	// Plane 1: the group of 16 at 4 splits; its first four: 4 = 3, 5 later, 6 = -2, 7 later; then it stays as the
	// group at 8, quiet, and the rest are quiet
	w.Bit(true);
	w.Bit(false).Put(1, 1).Bit(false); // 4: 2 | 1, positive
	w.Bit(true);                       // 5: a candidate
	w.Bit(false).Put(0, 1).Bit(true);  // 6: 2 | 0, negative
	w.Bit(true);                       // 7: a candidate
	w.Bit(false);                      // the group at 8
	for (int i = 0; i < 5; ++i)
	{
		w.Bit(false); // 24, 44, and the candidates 1, 2, 3
	}
	// Plane 0: candidates 7 (+1) and 5 (quiet); the group at 8 splits into 8, 12, 16, 20, of which 20 is read
	w.Bit(true).Bit(false); // 7: +1
	w.Bit(false);           // 5
	w.Bit(true);            // split
	w.Bit(false);           // 8, now a group of 4
	for (int i = 0; i < 5; ++i)
	{
		w.Bit(false); // 24, 44, 1, 2, 3
	}
	w.Bit(false).Bit(false); // 12, 16
	w.Bit(true);             // 20
	w.Bit(true);             // 20: a candidate
	w.Bit(false).Bit(true);  // 21: -1
	w.Bit(true);             // 22: a candidate
	w.Bit(false).Bit(false); // 23: +1; the new candidates 22 and 20 wait for a next plane
	w.Put(15, 4);
	BitReader reader(w.Bytes());
	Coefficients block {};
	CodedCoefficients coded;
	const auto quantiser = ReadDctCoefficients(reader, block, coded);
	ASSERT_TRUE(quantiser.has_value());
	EXPECT_EQ(*quantiser, 15u);
	const std::array<uint8_t, 5> positions = {4, 6, 7, 21, 23};
	ASSERT_EQ(coded.count, positions.size());
	for (size_t i = 0; i < positions.size(); ++i)
	{
		EXPECT_EQ(coded.positions[i], positions[i]) << i;
	}
	EXPECT_EQ(block[k_CoefficientScan[4]], 3);
	EXPECT_EQ(block[k_CoefficientScan[6]], -2);
	EXPECT_EQ(block[k_CoefficientScan[7]], 1);
	EXPECT_EQ(block[k_CoefficientScan[21]], -1);
	EXPECT_EQ(block[k_CoefficientScan[23]], 1);
	EXPECT_EQ(reader.Position(), w.Bits());
}

TEST(BinkBlocks, CoefficientsNeedFourBits)
{
	const std::vector<uint8_t> none;
	BitReader reader(none);
	Coefficients block {};
	CodedCoefficients coded;
	EXPECT_FALSE(ReadDctCoefficients(reader, block, coded).has_value());
}

TEST(BinkBlocks, DequantiseScalesTheCodedCoefficients)
{
	Coefficients block {};
	block[0] = 3;
	block[k_CoefficientScan[5]] = -2;
	block[k_CoefficientScan[9]] = 7; // not coded: left alone
	CodedCoefficients coded;
	coded.positions[0] = 5;
	coded.count = 1;
	const auto& q = k_IntraQuant[4];
	Dequantise(block, q, coded);
	EXPECT_EQ(block[0], static_cast<int32_t>(3 * q[0]) >> 11);
	EXPECT_EQ(block[k_CoefficientScan[5]], static_cast<int32_t>(static_cast<uint32_t>(-2) * q[5]) >> 11);
	EXPECT_EQ(block[k_CoefficientScan[9]], 7);
}

TEST(BinkBlocks, InverseDctOfAFlatBlock)
{
	Coefficients block {};
	block[0] = 100 * 256;
	InverseDct(block);
	for (const int32_t v : block)
	{
		EXPECT_EQ(v, 100);
	}
	block = {};
	block[0] = -256; // rounds to -1, which a put wraps to 255
	InverseDct(block);
	EXPECT_EQ(block[63], -1);
}

TEST(BinkBlocks, InverseDctOfAKnownBlock)
{
	Coefficients block {};
	block[0] = 16384;
	block[1] = 3200;
	block[8] = -4800;
	block[9] = 800;
	block[27] = -2000;
	block[63] = 112;
	InverseDct(block);
	// The format's integer transform, computed separately
	const Coefficients expected = {
	    53, 59, 65, 51, 39, 25, 31, 37, //
	    64, 64, 49, 56, 40, 47, 32, 32, //
	    79, 58, 59, 41, 66, 48, 49, 28, //
	    76, 76, 52, 70, 50, 68, 44, 44, //
	    77, 73, 90, 63, 73, 45, 63, 59, //
	    74, 91, 84, 92, 57, 66, 58, 75, //
	    89, 85, 93, 77, 83, 67, 75, 71, //
	    99, 90, 77, 82, 84, 89, 75, 66,
	};
	EXPECT_EQ(block, expected);
}

TEST(BinkBlocks, Residue)
{
	BinkBitWriter w;
	w.Put(0, 3);                        // one bit plane, mask 1
	w.Bit(false).Bit(false).Bit(false); // groups at 4, 24, 44
	w.Bit(true);                        // the group at 0
	w.Bit(false).Bit(false);            // 0: +1
	w.Bit(true);                        // 1: a candidate
	w.Bit(false).Bit(true);             // 2: -1
	w.Bit(false).Bit(false);            // 3: +1; the candidate 1 waits for a next plane
	BitReader reader(w.Bytes());
	const auto block = ReadResidue(reader, 10);
	EXPECT_EQ(block[k_CoefficientScan[0]], 1);
	EXPECT_EQ(block[k_CoefficientScan[2]], -1);
	EXPECT_EQ(block[k_CoefficientScan[3]], 1);
	EXPECT_EQ(std::count_if(block.begin(), block.end(), [](int16_t v) { return v != 0; }), 3);
	EXPECT_EQ(reader.Position(), w.Bits());
}

TEST(BinkBlocks, ResidueStopsAtItsMaskCount)
{
	BinkBitWriter w;
	w.Put(1, 3); // mask 2, then 1
	w.Bit(false).Bit(false).Bit(false);
	w.Bit(true);
	w.Bit(false).Bit(false); // 0: +2
	w.Bit(true);             // 1: a candidate
	w.Bit(false).Bit(true);  // 2: -2, the last one allowed
	BitReader reader(w.Bytes());
	const auto block = ReadResidue(reader, 1);
	EXPECT_EQ(block[k_CoefficientScan[0]], 2);
	EXPECT_EQ(block[k_CoefficientScan[2]], -2);
	EXPECT_EQ(block[k_CoefficientScan[3]], 0);
	EXPECT_EQ(reader.Position(), w.Bits());
}

TEST(BinkBlocks, ResidueRefinesWhatItFound)
{
	BinkBitWriter w;
	w.Put(1, 3); // mask 2, then 1
	w.Bit(false).Bit(false).Bit(false);
	w.Bit(true);
	w.Bit(false).Bit(true);          // 0: -2
	w.Bit(true).Bit(true).Bit(true); // 1, 2 and 3: candidates
	// Mask 1: the -2 grows to -3; then the candidates 3, 2, 1 and the groups at 4, 24, 44 stay quiet
	w.Bit(true);
	for (int i = 0; i < 6; ++i)
	{
		w.Bit(false);
	}
	BitReader reader(w.Bytes());
	const auto block = ReadResidue(reader, 10);
	EXPECT_EQ(block[k_CoefficientScan[0]], -3);
	EXPECT_EQ(reader.Position(), w.Bits());
}

namespace
{
struct TestPlane
{
	std::vector<uint8_t> pixels = std::vector<uint8_t>(32 * 32);
	Plane plane {pixels, 32};
};
} // namespace

TEST(BinkBlocks, PutAndAddWrapAround)
{
	TestPlane p;
	Coefficients block {};
	block[0] = -1;
	block[1] = 256;
	block[63] = 300;
	PutBlock(p.plane, 32 + 8, block);
	EXPECT_EQ(p.pixels[32 + 8], 255);
	EXPECT_EQ(p.pixels[32 + 9], 0);
	EXPECT_EQ(p.pixels[32 + 7 * 32 + 15], 44);
	std::array<int16_t, 64> residue {};
	residue[0] = 10;
	AddBlock(p.plane, 32 + 8, residue);
	EXPECT_EQ(p.pixels[32 + 8], 9);
	std::array<int32_t, 64> difference {};
	difference[1] = -1;
	AddBlock(p.plane, 32 + 8, difference);
	EXPECT_EQ(p.pixels[32 + 9], 255);
}

TEST(BinkBlocks, CopyBlockGoesRowByRow)
{
	TestPlane p;
	std::iota(p.pixels.begin(), p.pixels.end(), uint8_t {0});
	// Overlapping, one row down: each row is read before it is written, the rows above already copied
	CopyBlock(p.plane, 32, p.plane, 0);
	for (size_t row = 1; row <= 8; ++row)
	{
		for (size_t x = 0; x < 8; ++x)
		{
			EXPECT_EQ(p.pixels[row * 32 + x], static_cast<uint8_t>(x)) << row << "," << x;
		}
	}
	EXPECT_EQ(p.pixels[9 * 32], static_cast<uint8_t>(9 * 32));
}

TEST(BinkBlocks, FillAndScale)
{
	TestPlane p;
	FillBlock(p.plane, 0, 7, 16);
	EXPECT_EQ(p.pixels[15 * 32 + 15], 7);
	EXPECT_EQ(p.pixels[15 * 32 + 16], 0);
	EXPECT_EQ(p.pixels[16 * 32], 0);
	std::array<uint8_t, 64> block {};
	std::iota(block.begin(), block.end(), uint8_t {100});
	ScaleBlock(p.plane, 16, block);
	EXPECT_EQ(p.pixels[16], 100);
	EXPECT_EQ(p.pixels[17], 100);
	EXPECT_EQ(p.pixels[32 + 17], 100);
	EXPECT_EQ(p.pixels[18], 101);
	EXPECT_EQ(p.pixels[15 * 32 + 31], 163);
	EXPECT_EQ(p.pixels[0], 7);
}
