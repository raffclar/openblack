/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <numeric>
#include <span>
#include <string>
#include <vector>

#include <LhpFile.h>
#include <gtest/gtest.h>

using namespace openblack;

namespace
{

lhp::Package TwoBlocks()
{
	return {
	    .stamp = 0x1234,
	    .blocks =
	        {
	            {.category = "CREATURE", .name = "creature", .data = {'a', 'b', 'c'}},
	            {.category = "PICTURES", .name = "_0.jpg", .data = {0xFF, 0xD8}},
	        },
	};
}

std::vector<uint8_t> Bytes(std::initializer_list<int> values)
{
	std::vector<uint8_t> bytes;
	std::ranges::transform(values, std::back_inserter(bytes), [](int value) { return static_cast<uint8_t>(value); });
	return bytes;
}

} // namespace

TEST(LhpFile, WritesTheHeaderAndTheLengthOfTheBlocks)
{
	const auto file = lhp::Write(TwoBlocks());
	// Two records of 172 bytes and 5 bytes of data, filled to 352 bytes
	ASSERT_EQ(file.size(), 36u + 352u + 4u);
	EXPECT_EQ(std::vector<uint8_t>(file.begin(), file.begin() + 36),
	          Bytes({'L', 'H', 'N', 'I', 'L', 'D', 1,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	                 0,   0,   0,   0,   0,   0,   0x34, 0x12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
	EXPECT_EQ(std::vector<uint8_t>(file.end() - 4, file.end()), Bytes({0x5D, 0x01, 0, 0}));
}

TEST(LhpFile, EncryptsAsTheGame)
{
	// Bytes from a separate model of the game's encryption
	const auto file = lhp::Write(TwoBlocks());
	EXPECT_EQ(std::vector<uint8_t>(file.begin() + 36, file.begin() + 52),
	          Bytes({0x2D, 0x78, 0xBC, 0xA5, 0x84, 0xA3, 0x28, 0xF9, 0x0A, 0x76, 0xC6, 0xEF, 0x2B, 0x9F, 0x03, 0x19}));
	EXPECT_EQ(std::vector<uint8_t>(file.end() - 12, file.end() - 4), Bytes({0x97, 0x23, 0xF6, 0x8D, 0x94, 0xA5, 0xD0, 0x99}));
}

TEST(LhpFile, DecryptUndoesEncrypt)
{
	std::vector<uint8_t> bytes(40);
	std::iota(bytes.begin(), bytes.end(), uint8_t {7});
	const auto plain = bytes;
	lhp::Encrypt(bytes);
	EXPECT_NE(bytes, plain);
	lhp::Decrypt(bytes);
	EXPECT_EQ(bytes, plain);
}

TEST(LhpFile, ReadsWhatItWrites)
{
	const auto package = lhp::Read(lhp::Write(TwoBlocks()));
	ASSERT_TRUE(package.has_value());
	EXPECT_EQ(package->stamp, 0x1234);
	ASSERT_EQ(package->blocks.size(), 2u);
	EXPECT_EQ(package->blocks[0].category, "CREATURE");
	EXPECT_EQ(package->blocks[0].name, "creature");
	EXPECT_EQ(package->blocks[0].data, Bytes({'a', 'b', 'c'}));
	EXPECT_EQ(package->blocks[1].category, "PICTURES");
	EXPECT_EQ(package->blocks[1].name, "_0.jpg");
	EXPECT_EQ(package->blocks[1].data, Bytes({0xFF, 0xD8}));
	EXPECT_EQ(lhp::Write(*package), lhp::Write(TwoBlocks()));
}

TEST(LhpFile, LinksTheBlocksByTheirOffsets)
{
	auto file = lhp::Write(TwoBlocks());
	auto parts = std::span(file).subspan(36, 352);
	lhp::Decrypt(parts);
	const auto word = [&](size_t at) {
		return parts[at] | (parts[at + 1] << 8) | (parts[at + 2] << 16) | (parts[at + 3] << 24);
	};
	// The first record: the next block, the length and the data
	EXPECT_EQ(word(160), 175);
	EXPECT_EQ(word(164), 3);
	EXPECT_EQ(word(168), 172);
	// The last record has no next block
	EXPECT_EQ(word(175 + 160), 0);
	EXPECT_EQ(word(175 + 168), 175 + 172);
}

TEST(LhpFile, CutsLongTexts)
{
	auto package = TwoBlocks();
	package.blocks[0].category = "CATEGORY_TOO_LONG";
	package.blocks[0].name = std::string(200, 'n');
	const auto read = lhp::Read(lhp::Write(package));
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->blocks[0].category, "CATEGORY_");
	EXPECT_EQ(read->blocks[0].name, std::string(149, 'n'));
}

TEST(LhpFile, RefusesOtherFiles)
{
	auto file = lhp::Write(TwoBlocks());
	EXPECT_FALSE(lhp::Read(std::span(file).first(20)).has_value());
	auto shortened = file;
	shortened.pop_back();
	EXPECT_FALSE(lhp::Read(shortened).has_value());
	file[0] = 'X';
	EXPECT_FALSE(lhp::Read(file).has_value());
}

TEST(LhpFile, AnEmptyPackageHasNoBlocks)
{
	const auto file = lhp::Write({});
	EXPECT_EQ(file.size(), 40u);
	const auto read = lhp::Read(file);
	ASSERT_TRUE(read.has_value());
	EXPECT_TRUE(read->blocks.empty());
}
