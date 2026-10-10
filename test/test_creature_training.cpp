/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureTraining.h"

using namespace openblack;
using namespace openblack::creature_training;

namespace
{
/// Highlights placed by cell for a fake map, recording the cells asked about in order
struct FakeCells
{
	std::map<std::pair<int32_t, int32_t>, std::vector<HighlightInCell>> highlights;
	std::vector<glm::ivec2> asked;

	HighlightsInCell Lookup()
	{
		return [this](glm::ivec2 cell) {
			asked.push_back(cell);
			const auto found = highlights.find({cell.x, cell.y});
			return found != highlights.end() ? found->second : std::vector<HighlightInCell> {};
		};
	}
};

/// The middle of a cell, as a map position
map_coords::MapCoords CellMiddle(int32_t x, int32_t z)
{
	return map_coords::FromMetres(
	    {(static_cast<float>(x) + 0.5f) * map_coords::k_CellSize, (static_cast<float>(z) + 0.5f) * map_coords::k_CellSize});
}

entt::entity Thing(uint32_t number)
{
	return static_cast<entt::entity>(number);
}
} // namespace

TEST(CreatureTraining, EachActionCarriedOutCountsUnlessAScriptControlsIt)
{
	std::map<uint32_t, uint32_t> counts;
	EXPECT_EQ(TimesCarriedOut(counts, 11), 0u);
	CountCarriedOut(counts, 11, false);
	CountCarriedOut(counts, 11, false);
	CountCarriedOut(counts, 276, false);
	CountCarriedOut(counts, 12, true);
	EXPECT_EQ(TimesCarriedOut(counts, 11), 2u);
	EXPECT_EQ(TimesCarriedOut(counts, 276), 1u);
	EXPECT_EQ(TimesCarriedOut(counts, 12), 0u);
}

TEST(CreatureTraining, ThePointingSearchWalksEightyOneCellsInTheGamesSpiral)
{
	FakeCells cells;
	EXPECT_FALSE(HighlightToPointOut(CellMiddle(100, 100), cells.Lookup()).has_value());
	ASSERT_EQ(cells.asked.size(), static_cast<size_t>(k_PointOutCells));
	// Its own cell first, then a square growing outwards: west, south, east twice, north twice, west three times
	const std::vector<glm::ivec2> start {{100, 100}, {99, 100},  {99, 99},   {100, 99}, {101, 99},
	                                     {101, 100}, {101, 101}, {100, 101}, {99, 101}, {98, 101}};
	for (size_t i = 0; i < start.size(); ++i)
	{
		EXPECT_EQ(cells.asked[i], start[i]) << i;
	}
	// Nine by nine cells about its own, each once
	for (const auto& cell : cells.asked)
	{
		EXPECT_LE(std::abs(cell.x - 100), 4);
		EXPECT_LE(std::abs(cell.y - 100), 4);
	}
}

TEST(CreatureTraining, TipSignsAreNotPointedOut)
{
	FakeCells cells;
	cells.highlights[{100, 100}] = {{.entity = Thing(1), .tipSign = true}, {.entity = Thing(2), .tipSign = false}};
	EXPECT_EQ(HighlightToPointOut(CellMiddle(100, 100), cells.Lookup()), Thing(2));

	FakeCells signsOnly;
	signsOnly.highlights[{100, 100}] = {{.entity = Thing(1), .tipSign = true}};
	EXPECT_FALSE(HighlightToPointOut(CellMiddle(100, 100), signsOnly.Lookup()).has_value());
}

TEST(CreatureTraining, TheLastHighlightTheSpiralMeetsWins)
{
	FakeCells cells;
	// The first scroll in a cell, and a cell met later replacing it
	cells.highlights[{100, 100}] = {{.entity = Thing(1)}, {.entity = Thing(2)}};
	cells.highlights[{104, 104}] = {{.entity = Thing(3)}};
	EXPECT_EQ(HighlightToPointOut(CellMiddle(100, 100), cells.Lookup()), Thing(3));
	// Five cells away is beyond the search
	FakeCells far;
	far.highlights[{105, 100}] = {{.entity = Thing(4)}};
	EXPECT_FALSE(HighlightToPointOut(CellMiddle(100, 100), far.Lookup()).has_value());
}

TEST(CreatureTraining, CellsOffTheMapAreNotSearched)
{
	FakeCells cells;
	(void)HighlightToPointOut(CellMiddle(0, 0), cells.Lookup());
	for (const auto& cell : cells.asked)
	{
		EXPECT_GE(cell.x, 0);
		EXPECT_GE(cell.y, 0);
	}
	EXPECT_LT(cells.asked.size(), static_cast<size_t>(k_PointOutCells));
}
