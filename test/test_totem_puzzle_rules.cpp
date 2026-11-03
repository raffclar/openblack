/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <utility>

#include <gtest/gtest.h>

#include "ECS/Components/PuzzleGame.h"
#include "ECS/PuzzleGameRules.h"
#include "ECS/TotemPuzzleRules.h"

namespace totem_puzzle = openblack::ecs::totem_puzzle;
using openblack::script::PuzzleGameType;

namespace
{
using Heights = totem_puzzle::Heights;

/// The heights after the player gives one totem a new height
Heights Move(PuzzleGameType type, const Heights& kept, size_t totem, int32_t height)
{
	auto shown = kept;
	shown.at(totem) = height;
	const auto change = totem_puzzle::Step(type, kept, shown);
	EXPECT_TRUE(change.has_value());
	EXPECT_EQ(change->moved, totem);
	return change->kept;
}
} // namespace

TEST(TotemPuzzleRules, KindsAndLayouts)
{
	EXPECT_TRUE(totem_puzzle::IsTotemPuzzle(PuzzleGameType::Totem1));
	EXPECT_TRUE(totem_puzzle::IsTotemPuzzle(PuzzleGameType::Totem4));
	EXPECT_FALSE(totem_puzzle::IsTotemPuzzle(PuzzleGameType::Thesius1));
	EXPECT_FALSE(totem_puzzle::OnCurve(PuzzleGameType::Totem1));
	EXPECT_FALSE(totem_puzzle::OnCurve(PuzzleGameType::Totem2));
	EXPECT_TRUE(totem_puzzle::OnCurve(PuzzleGameType::Totem3));
	EXPECT_TRUE(totem_puzzle::OnCurve(PuzzleGameType::Totem4));
	EXPECT_EQ(&totem_puzzle::CouplingsOf(PuzzleGameType::Totem1), &totem_puzzle::CouplingsOf(PuzzleGameType::Totem3));
	EXPECT_EQ(&totem_puzzle::CouplingsOf(PuzzleGameType::Totem2), &totem_puzzle::CouplingsOf(PuzzleGameType::Totem4));
	EXPECT_NE(&totem_puzzle::CouplingsOf(PuzzleGameType::Totem1), &totem_puzzle::CouplingsOf(PuzzleGameType::Totem2));
}

TEST(TotemPuzzleRules, NoChangeWhenAllHeightsAreKept)
{
	const Heights kept {1, 2, 3, 4, 0, 1};
	EXPECT_FALSE(totem_puzzle::Step(PuzzleGameType::Totem3, kept, kept).has_value());
}

TEST(TotemPuzzleRules, ARaiseChangesTheCoupledTotems)
{
	// The third totem goes up by one: the first two go up by one, and the fourth by two
	EXPECT_EQ(Move(PuzzleGameType::Totem3, {}, 2, 1), (Heights {1, 1, 1, 2, 0, 0}));
}

TEST(TotemPuzzleRules, ALowerGoesRoundInFivePositions)
{
	// The first totem goes down by two, which counts as up by three: the second totem goes up by 12 and the last by 6
	EXPECT_EQ(Move(PuzzleGameType::Totem3, {2, 0, 0, 0, 0, 0}, 0, 0), (Heights {0, 2, 0, 0, 0, 1}));
}

TEST(TotemPuzzleRules, TheSecondTotemOfTheFirstTableChangesNothingElse)
{
	EXPECT_EQ(Move(PuzzleGameType::Totem1, {}, 1, 3), (Heights {0, 3, 0, 0, 0, 0}));
}

TEST(TotemPuzzleRules, TheSecondTableBelongsToTheOtherKinds)
{
	EXPECT_EQ(Move(PuzzleGameType::Totem4, {}, 0, 1), (Heights {1, 0, 0, 0, 0, 3}));
	EXPECT_EQ(Move(PuzzleGameType::Totem2, {}, 1, 1), (Heights {1, 1, 1, 1, 0, 0}));
}

TEST(TotemPuzzleRules, OnlyTheFirstChangedTotemCounts)
{
	const Heights kept {};
	const Heights shown {0, 3, 1, 0, 0, 0};
	const auto change = totem_puzzle::Step(PuzzleGameType::Totem3, kept, shown);
	ASSERT_TRUE(change.has_value());
	EXPECT_EQ(change->moved, 1U);
	EXPECT_EQ(change->kept, (Heights {0, 3, 0, 0, 0, 0}));
}

TEST(TotemPuzzleRules, TheFourthLandPuzzleHasASolutionInSixMoves)
{
	Heights kept {};
	EXPECT_FALSE(totem_puzzle::Solved(kept));
	constexpr std::array<std::pair<size_t, int32_t>, 6> k_Moves {{{0, 3}, {1, 1}, {2, 1}, {3, 0}, {4, 4}, {5, 4}}};
	for (const auto& [totem, height] : k_Moves)
	{
		kept = Move(PuzzleGameType::Totem3, kept, totem, height);
	}
	EXPECT_TRUE(totem_puzzle::Solved(kept));
}

TEST(TotemPuzzleRules, TheCurveIsAHalfCircleOfTwentyMetres)
{
	// The radius is two of the table's units, 131072 map units
	const auto first = totem_puzzle::CurveOffset(0, 1.0f);
	EXPECT_EQ(first.x, 0);
	EXPECT_EQ(first.z, 131072);
	const auto fourth = totem_puzzle::CurveOffset(3, 1.0f);
	EXPECT_EQ(fourth.x, 131072);
	EXPECT_EQ(fourth.z, 0);
	// The last totem is five sixths of the way round the half circle, at the table's place 853
	const auto last = totem_puzzle::CurveOffset(5, 1.0f);
	EXPECT_GT(last.x, 0);
	EXPECT_LT(last.z, 0);
	// The size scales the radius
	EXPECT_EQ(totem_puzzle::CurveOffset(0, 0.5f).z, 65536);
}

TEST(TotemPuzzleRules, ACurveTotemFacesAwayFromThePlace)
{
	const openblack::map_coords::MapCoords place {.x = 655360, .z = 655360, .altitude = 0.0f};
	const auto offset = [&place](size_t totem) {
		const auto step = totem_puzzle::CurveOffset(totem, 1.0f);
		return openblack::map_coords::MapCoords {.x = place.x + step.x, .z = place.z + step.z, .altitude = 0.0f};
	};
	// Straight along z: a quarter turn less a quarter turn
	EXPECT_NEAR(totem_puzzle::CurveFacing(place, offset(0)), 0.0f, 1e-6f);
	// Straight along x: no turn less a quarter turn
	EXPECT_NEAR(totem_puzzle::CurveFacing(place, offset(3)), -1.5707964f, 1e-6f);
}

TEST(TotemPuzzleRules, ASlideMovesAFifthOfTheWayEachStep)
{
	EXPECT_FLOAT_EQ(totem_puzzle::SlideShare(4, 0, 4, 4), 0.2f);
	EXPECT_FLOAT_EQ(totem_puzzle::SlideShare(4, 0, 2, 4), 0.6f);
	EXPECT_FLOAT_EQ(totem_puzzle::SlideShare(4, 0, 0, 4), 1.0f);
	EXPECT_FLOAT_EQ(totem_puzzle::SlideShare(0, 2, 4, 4), 0.4f);
}

TEST(TotemPuzzleRules, ADeleteKeepsTheTotemsOfTheFourthLandKind)
{
	const auto made = [](PuzzleGameType type) {
		openblack::ecs::components::PuzzleGame game {.type = type};
		for (size_t index = 0; index < game.totems.size(); ++index)
		{
			game.totems.at(index) = static_cast<entt::entity>(index + 1);
		}
		return game;
	};
	auto kept = made(PuzzleGameType::Totem3);
	EXPECT_TRUE(openblack::ecs::puzzle_game::TakePiecesForDelete(kept, [](entt::entity /*unused*/) { return true; }).empty());
	EXPECT_EQ(kept.totems.front(), static_cast<entt::entity>(1));
	EXPECT_TRUE(kept.played);
	auto taken = made(PuzzleGameType::Totem1);
	EXPECT_EQ(openblack::ecs::puzzle_game::TakePiecesForDelete(taken, [](entt::entity /*unused*/) { return true; }).size(),
	          totem_puzzle::k_Totems);
	EXPECT_TRUE(taken.totems.front() == entt::null);
}

TEST(TotemPuzzleRules, ALoadForgetsTheSpotVisuals)
{
	openblack::ecs::components::PuzzleGame game {.type = PuzzleGameType::Totem3};
	game.totemMarks.fill(7);
	game.totemHeights.fill(4);
	openblack::ecs::puzzle_game::ForgetUnsavedState(game);
	EXPECT_EQ(game.totemMarks.front(), 0U);
	EXPECT_EQ(game.totemHeights.front(), 4);
}
