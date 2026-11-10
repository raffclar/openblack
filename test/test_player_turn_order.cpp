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
#include <iterator>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "PlayerTurnOrder.h"

using namespace openblack;
using player_turn::Part;

namespace
{

using Done = std::pair<uint32_t, Part>;

/// What the turn did, in its order: each player and the part of its turn
std::vector<Done> TurnOf(const std::vector<uint32_t>& players)
{
	std::vector<Done> done;
	player_turn::Play(players, [&done](uint32_t player, Part part) { done.emplace_back(player, part); });
	return done;
}

/// Where a part of the turn of a player is in the record
std::ptrdiff_t IndexOf(const std::vector<Done>& done, uint32_t player, Part part)
{
	return std::distance(done.begin(), std::ranges::find(done, Done {player, part}));
}

} // namespace

TEST(PlayerTurnOrder, EachPlayerDoesItsWholeTurnBeforeTheNextPlayerStarts)
{
	const auto done = TurnOf({1, 2, 7});
	std::vector<Done> expected;
	for (const uint32_t player : {1u, 2u, 7u})
	{
		for (const auto part : {Part::Conditions, Part::Temple, Part::Towns, Part::Stones, Part::Alignment, Part::Mind,
		                        Part::Powers, Part::Influence})
		{
			expected.emplace_back(player, part);
		}
	}
	EXPECT_EQ(done, expected);
}

// The game draws the random numbers of a computer mind after the towns of its own player and before the towns of the
// players after it, the neutral player too
TEST(PlayerTurnOrder, TheMindOfAPlayerComesAfterItsTownsAndBeforeTheTownsOfTheNextPlayers)
{
	const auto done = TurnOf({1, 2, 7});
	EXPECT_LT(IndexOf(done, 2, Part::Towns), IndexOf(done, 2, Part::Mind));
	EXPECT_LT(IndexOf(done, 2, Part::Stones), IndexOf(done, 2, Part::Mind));
	EXPECT_LT(IndexOf(done, 2, Part::Mind), IndexOf(done, 7, Part::Temple));
	EXPECT_LT(IndexOf(done, 2, Part::Mind), IndexOf(done, 7, Part::Towns));
	EXPECT_LT(IndexOf(done, 1, Part::Mind), IndexOf(done, 2, Part::Towns));
}

// A computer mind looks at the alignments of the players before it and its own as they are after their change of this
// turn. The power of the tribes settles after the mind and before the influence power.
TEST(PlayerTurnOrder, TheAlignmentMovesBeforeTheMindAndThePowersSettleAfterIt)
{
	const auto done = TurnOf({1, 2});
	EXPECT_LT(IndexOf(done, 2, Part::Stones), IndexOf(done, 2, Part::Alignment));
	EXPECT_LT(IndexOf(done, 1, Part::Alignment), IndexOf(done, 2, Part::Mind));
	EXPECT_LT(IndexOf(done, 2, Part::Alignment), IndexOf(done, 2, Part::Mind));
	EXPECT_LT(IndexOf(done, 2, Part::Mind), IndexOf(done, 2, Part::Powers));
	EXPECT_LT(IndexOf(done, 2, Part::Powers), IndexOf(done, 2, Part::Influence));
	EXPECT_LT(IndexOf(done, 1, Part::Influence), IndexOf(done, 2, Part::Conditions));
}

TEST(PlayerTurnOrder, NoPlayersDoNoParts)
{
	EXPECT_TRUE(TurnOf({}).empty());
}
