/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/StateHashWatch.h"

using namespace openblack::state_hash;

namespace
{
Snapshot Make(uint32_t turn, uint64_t hash, std::vector<std::pair<std::string, uint64_t>> parts = {})
{
	return {.turn = turn, .hash = hash, .parts = std::move(parts)};
}
} // namespace

TEST(StateHashWatch, TheHashIsSixteenHexDigits)
{
	EXPECT_EQ(HexOf(0xabcULL), "0000000000000abc");
	EXPECT_EQ(HexOf(0xffffffffffffffffULL), "ffffffffffffffff");
}

TEST(StateHashWatch, NothingPinnedIsNotAMatch)
{
	EXPECT_EQ(Compare(std::nullopt, Make(1, 5)), Match::NothingPinned);
}

TEST(StateHashWatch, TheSameHashMatchesAtAnotherTurn)
{
	EXPECT_EQ(Compare(Make(10, 5), Make(20, 5)), Match::Same);
	EXPECT_EQ(Compare(Make(10, 5), Make(10, 6)), Match::Different);
}

TEST(StateHashWatch, TheDifferencesAreThePartsThatChangedOrAreOnlyInOne)
{
	const auto pinned = Make(1, 1, {{"entities", 7}, {"Transform", 8}, {"Gone", 9}});
	const auto current = Make(2, 2, {{"entities", 7}, {"Transform", 80}, {"New", 10}});
	const auto differences = Differences(pinned, current);
	ASSERT_EQ(differences.size(), 3u);
	EXPECT_EQ(differences[0].part, "Gone");
	EXPECT_EQ(differences[0].pinned, 9u);
	EXPECT_FALSE(differences[0].current.has_value());
	EXPECT_EQ(differences[1].part, "New");
	EXPECT_FALSE(differences[1].pinned.has_value());
	EXPECT_EQ(differences[2].part, "Transform");
	EXPECT_EQ(differences[2].pinned, 8u);
	EXPECT_EQ(differences[2].current, 80u);
}

TEST(StateHashWatch, ALineGoesRoundTrip)
{
	const auto line = Line(Make(1234, 0x1122334455667788ULL));
	EXPECT_EQ(line, "turn 1234 hash 1122334455667788");
	const auto back = FromLine(line);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->turn, 1234u);
	EXPECT_EQ(back->hash, 0x1122334455667788ULL);
	EXPECT_TRUE(back->parts.empty());
}

TEST(StateHashWatch, OtherTextIsNotALine)
{
	EXPECT_FALSE(FromLine("").has_value());
	EXPECT_FALSE(FromLine("turn x hash 1122334455667788").has_value());
	EXPECT_FALSE(FromLine("turn 5 hash 112233").has_value());
	EXPECT_FALSE(FromLine("turn 5 hash 112233445566778g").has_value());
	EXPECT_FALSE(FromLine("hash 1122334455667788").has_value());
}

TEST(StateHashWatch, ASnapshotIsDueOncePerMultipleOfTheNumberOfTurns)
{
	EXPECT_FALSE(Due(0, 100, std::nullopt)) << "0 never takes one";
	EXPECT_TRUE(Due(100, 200, std::nullopt));
	EXPECT_FALSE(Due(100, 150, std::nullopt));
	EXPECT_FALSE(Due(100, 200, 200u)) << "The turn was taken already";
	EXPECT_TRUE(Due(100, 300, 200u));
}
