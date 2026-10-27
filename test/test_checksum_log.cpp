/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Simulation/ChecksumLog.h"

using namespace openblack;

TEST(ChecksumLog, LineHasTheTurnAndTheChecksum)
{
	EXPECT_EQ(checksum_log::LineOf(12, 0x00ab'cdef'0123'4567ull, {}), R"({"turn":12,"checksum":"00abcdef01234567"})");
}

TEST(ChecksumLog, LineHasThePartsInTheirOrder)
{
	const std::vector<std::pair<std::string, uint64_t>> parts {{"entities", 1}, {"Transform", 0xffull}};
	EXPECT_EQ(
	    checksum_log::LineOf(3, 2, parts),
	    R"({"turn":3,"checksum":"0000000000000002","parts":{"entities":"0000000000000001","Transform":"00000000000000ff"}})");
}

TEST(ChecksumLog, ReadLineGivesBackTheTurnAndTheChecksum)
{
	const auto line = checksum_log::LineOf(1000, 0xfedc'ba98'7654'3210ull, {{"entities", 5}});
	const auto turn = checksum_log::ReadLine(line);
	ASSERT_TRUE(turn.has_value());
	EXPECT_EQ(turn->turn, 1000u);
	EXPECT_EQ(turn->checksum, 0xfedc'ba98'7654'3210ull);
}

TEST(ChecksumLog, ReadLineRefusesOtherLines)
{
	EXPECT_FALSE(checksum_log::ReadLine("").has_value());
	EXPECT_FALSE(checksum_log::ReadLine(R"({"turn":1})").has_value());
	EXPECT_FALSE(checksum_log::ReadLine(R"({"turn":1,"checksum":"12"})").has_value());
	EXPECT_FALSE(checksum_log::ReadLine(R"({"turn":x,"checksum":"0000000000000002"})").has_value());
}

TEST(ChecksumLog, LineHasNoUnregisteredListWhenThereAreNone)
{
	EXPECT_EQ(checksum_log::LineOf(1, 2, {}, {}), R"({"turn":1,"checksum":"0000000000000002"})");
}

TEST(ChecksumLog, LineNamesTheUnregisteredComponents)
{
	EXPECT_EQ(checksum_log::LineOf(1, 2, {}, {"Alpha", "Beta"}),
	          R"({"turn":1,"checksum":"0000000000000002","unregistered":["Alpha","Beta"]})");
	// A tool that only reads the turn and the checksum still reads the line
	const auto turn = checksum_log::ReadLine(checksum_log::LineOf(1, 2, {}, {"Alpha"}));
	ASSERT_TRUE(turn.has_value());
	EXPECT_EQ(turn->checksum, 2u);
}
