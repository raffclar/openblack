/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Online/StartRules.h"

using namespace openblack;
using namespace openblack::online::start_rules;

TEST(StartRules, TheLandChecksumAddsTheBytesAndThePlacesOfTheLineFeeds)
{
	EXPECT_EQ(LandChecksum({}), 0u);
	// "A\n": the low sum is 65 + 10 + 1 (the place of the line feed), the high sum 65 * 9 + 21 + 10 * 9 + 3
	const std::vector<uint8_t> file = {'A', '\n'};
	EXPECT_EQ(LandChecksum(file), (uint64_t {699} << 32) | 76u);
	// Only the low 6 bits of a line feed's place count: a line feed at place 64 adds 0
	std::vector<uint8_t> long_(65, 0);
	long_[64] = '\n';
	EXPECT_EQ(LandChecksum(long_), (uint64_t {93} << 32) | 10u);
	// A byte divided by 3 rounds down
	const std::vector<uint8_t> high = {255};
	EXPECT_EQ(LandChecksum(high), (uint64_t {255 * 9 + 85} << 32) | 255u);
}

TEST(StartRules, TheSumsWrapAt32Bits)
{
	// 2,000,000 bytes of 255: the high sum is 2,000,000 * 2380, more than 32 bits hold
	const std::vector<uint8_t> file(2'000'000, 255);
	const auto checksum = LandChecksum(file);
	EXPECT_EQ(checksum >> 32, static_cast<uint32_t>(2'000'000ull * 2380));
	EXPECT_EQ(checksum & 0xFFFFFFFFu, static_cast<uint32_t>(2'000'000ull * 255));
}

TEST(StartRules, OnlyPlainPathsInTheScriptsFolderAreLands)
{
	EXPECT_TRUE(IsLandPath("Land1.txt"));
	EXPECT_TRUE(IsLandPath("Playgrounds/TwoGods.txt"));
	EXPECT_TRUE(IsLandPath("a..b/c.txt"));
	EXPECT_FALSE(IsLandPath(""));
	EXPECT_FALSE(IsLandPath("/etc/passwd"));
	EXPECT_FALSE(IsLandPath("\\Windows\\win.ini"));
	EXPECT_FALSE(IsLandPath("C:/Windows/win.ini"));
	EXPECT_FALSE(IsLandPath("../Profiles/x.txt"));
	EXPECT_FALSE(IsLandPath("Playgrounds/../../x.txt"));
	EXPECT_FALSE(IsLandPath("Playgrounds\\..\\..\\x.txt"));
	EXPECT_FALSE(IsLandPath(".."));
}

TEST(StartRules, ThePlayerRepliesWithTheChecksumOfItsLandFile)
{
	const game_socket::StartQuery query {.land = "Land1.txt", .testbed = false, .checksum = 5};
	const std::vector<uint8_t> file = {'A', '\n'};
	const auto have = Reply(query, file);
	EXPECT_TRUE(have.have);
	EXPECT_EQ(have.checksum, LandChecksum(file));
	const auto missing = Reply(query, std::nullopt);
	EXPECT_FALSE(missing.have);
	// The testbed has no file: each player has it
	const auto testbed = Reply({.land = {}, .testbed = true, .checksum = 0}, std::nullopt);
	EXPECT_TRUE(testbed.have);
	EXPECT_EQ(testbed.checksum, 0u);
}

TEST(StartRules, TheServerJudgesEachReply)
{
	EXPECT_EQ(Judge(7, {.have = true, .checksum = 7}), Verdict::Same);
	EXPECT_EQ(Judge(7, {.have = true, .checksum = 8}), Verdict::Different);
	EXPECT_EQ(Judge(7, {.have = false, .checksum = 7}), Verdict::Missing);
}
