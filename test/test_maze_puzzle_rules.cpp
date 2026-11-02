/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <array>
#include <deque>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/PuzzleGame.h"
#include "ECS/HanoiRules.h"
#include "ECS/MazePuzzleRules.h"
#include "ECS/PuzzleGameRules.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Reflection/WorldArchive.h"

using openblack::script::PuzzleGameType;
namespace hanoi = openblack::ecs::hanoi;
namespace maze = openblack::ecs::maze_puzzle;

namespace
{
constexpr std::array<PuzzleGameType, 4> k_Mazes {PuzzleGameType::Thesius1, PuzzleGameType::Thesius2, PuzzleGameType::Tilt1,
                                                 PuzzleGameType::Tilt2};

using CellKey = std::pair<int32_t, int32_t>;

CellKey KeyOf(maze::Cell cell)
{
	return {cell.across, cell.along};
}

/// Whether the villager of a Theseus maze can go out of the maze from its first cell, with the lion after it. The lion
/// must not stand on the villager's cell after its moves.
bool TheseusSolvable(PuzzleGameType type)
{
	using State = std::tuple<int32_t, int32_t, int32_t, int32_t>;
	const auto hero = maze::HeroStart(type);
	const auto lion = maze::LionStart(type);
	std::set<State> seen {{hero.across, hero.along, lion.across, lion.along}};
	std::deque<State> open {{hero.across, hero.along, lion.across, lion.along}};
	while (!open.empty())
	{
		const auto [heroAcross, heroAlong, lionAcross, lionAlong] = open.front();
		open.pop_front();
		const maze::Cell from {.across = heroAcross, .along = heroAlong};
		for (int32_t direction = -1; direction < maze::k_Directions; ++direction)
		{
			auto to = from;
			if (direction >= 0)
			{
				if (maze::MoveBlocked(type, from, direction))
				{
					continue;
				}
				to = maze::Moved(from, direction);
			}
			if (maze::Escaped(type, to))
			{
				return true;
			}
			const auto chaser = maze::MoveLion(type, {.across = lionAcross, .along = lionAlong}, to).cell;
			if (chaser == to)
			{
				continue;
			}
			const State next {to.across, to.along, chaser.across, chaser.along};
			if (seen.insert(next).second)
			{
				open.push_back(next);
			}
		}
	}
	return false;
}

/// Whether the lion of the lion maze can stop on the cell of the sheep with slides from its first cell
bool TiltSolvable(PuzzleGameType type)
{
	const auto sheep = maze::HeroStart(type);
	const auto start = maze::LionStart(type);
	std::set<CellKey> seen {KeyOf(start)};
	std::deque<maze::Cell> open {start};
	while (!open.empty())
	{
		const auto lion = open.front();
		open.pop_front();
		if (lion == sheep)
		{
			return true;
		}
		for (int32_t direction = 0; direction < maze::k_Directions; ++direction)
		{
			const auto next = maze::Slide(type, lion, direction);
			if (seen.insert(KeyOf(next)).second)
			{
				open.push_back(next);
			}
		}
	}
	return false;
}
} // namespace

TEST(MazePuzzleRules, KindsAndSides)
{
	EXPECT_TRUE(maze::IsTheseus(PuzzleGameType::Thesius1));
	EXPECT_TRUE(maze::IsTheseus(PuzzleGameType::Thesius2));
	EXPECT_FALSE(maze::IsTheseus(PuzzleGameType::Tilt1));
	EXPECT_TRUE(maze::IsTilt(PuzzleGameType::Tilt2));
	EXPECT_FALSE(maze::IsMaze(PuzzleGameType::Maze1));
	EXPECT_FALSE(maze::IsMaze(PuzzleGameType::Fishes1));
	EXPECT_EQ(maze::SideOf(PuzzleGameType::Thesius1), 7);
	EXPECT_EQ(maze::SideOf(PuzzleGameType::Tilt1), 7);
	EXPECT_EQ(maze::SideOf(PuzzleGameType::Thesius2), 8);
	EXPECT_EQ(maze::SideOf(PuzzleGameType::Tilt2), 8);
}

TEST(MazePuzzleRules, StepsAreDividedByFiveAsWholeNumbers)
{
	const auto steps = maze::MazeSteps({.cosine = 65536, .sine = -13});
	EXPECT_EQ(steps.cosine, 13107);
	EXPECT_EQ(steps.sine, -2);
}

TEST(MazePuzzleRules, WallsOffTheLayoutAlwaysStop)
{
	for (const auto type : k_Mazes)
	{
		EXPECT_TRUE(maze::HasWall(type, -1, 0, 0));
		EXPECT_TRUE(maze::HasWall(type, 0, -1, 1));
		EXPECT_TRUE(maze::HasWall(type, 8, 0, 0));
		EXPECT_TRUE(maze::HasWall(type, 0, 8, 1));
		EXPECT_TRUE(maze::HasWall(type, 0, 0, 2));
		// A number that is not a direction is no move, so no wall stops it
		EXPECT_FALSE(maze::MoveBlocked(type, {.across = 3, .along = 3}, -1));
	}
}

TEST(MazePuzzleRules, MoveBlockedReadsTheWallOfTheNextCellDown)
{
	for (const auto type : k_Mazes)
	{
		for (int32_t across = 0; across < 8; ++across)
		{
			for (int32_t along = 0; along < 8; ++along)
			{
				const maze::Cell cell {.across = across, .along = along};
				EXPECT_EQ(maze::MoveBlocked(type, cell, maze::k_AlongUp), maze::HasWall(type, across, along, 0));
				EXPECT_EQ(maze::MoveBlocked(type, cell, maze::k_AcrossUp), maze::HasWall(type, across, along, 1));
				EXPECT_EQ(maze::MoveBlocked(type, cell, maze::k_AlongDown), maze::HasWall(type, across, along - 1, 0));
				EXPECT_EQ(maze::MoveBlocked(type, cell, maze::k_AcrossDown), maze::HasWall(type, across - 1, along, 1));
			}
		}
	}
}

TEST(MazePuzzleRules, FencesStandOnTheWallsInsideTheMaze)
{
	for (const auto type : k_Mazes)
	{
		const auto side = maze::SideOf(type);
		std::set<std::tuple<int32_t, int32_t, bool>> fences;
		for (const auto& fence : maze::FencesOf(type))
		{
			EXPECT_TRUE(fences.insert({fence.across, fence.along, fence.facesAcross}).second);
			EXPECT_LT(maze::SlotOf(fence), maze::k_FenceSlots);
			if (fence.facesAcross)
			{
				EXPECT_LT(fence.along, side);
				EXPECT_TRUE(maze::HasWall(type, fence.across - 1, fence.along, 1));
			}
			else
			{
				EXPECT_LT(fence.across, side);
				EXPECT_TRUE(maze::HasWall(type, fence.across, fence.along - 1, 0));
			}
		}
		// The edge of the maze has a fence where its walls are
		EXPECT_FALSE(fences.empty());
	}
}

TEST(MazePuzzleRules, FenceStepsAndSlots)
{
	EXPECT_EQ(maze::FenceStep({.across = 2, .along = 3, .facesAcross = true}), (maze::GridStep {.across = 4, .along = 7}));
	EXPECT_EQ(maze::FenceStep({.across = 2, .along = 3, .facesAcross = false}), (maze::GridStep {.across = 5, .along = 6}));
	EXPECT_EQ(maze::SlotOf({.across = 8, .along = 8}), 80u);
	EXPECT_EQ(maze::MiddleOf({.across = 3, .along = 0}), (maze::GridStep {.across = 7, .along = 1}));
}

TEST(MazePuzzleRules, FenceAngleTurnsAQuarterCircleForFencesAcross)
{
	EXPECT_EQ(maze::FenceAngle(0, false), 0.0f);
	EXPECT_EQ(maze::FenceAngle(0, true), 1.57079637f);
	EXPECT_EQ(maze::FenceAngle(512, false), 3.14159274f);
	EXPECT_NEAR(maze::FenceAngle(1024, true), 6.28318548f + 1.57079637f, 1e-6f);
}

TEST(MazePuzzleRules, CornersLanternsAndBells)
{
	const auto corners = maze::CornerSteps(7);
	EXPECT_EQ(corners.at(0), (maze::GridStep {.across = 0, .along = 0}));
	EXPECT_EQ(corners.at(1), (maze::GridStep {.across = 14, .along = 14}));
	EXPECT_EQ(corners.at(2), (maze::GridStep {.across = 14, .along = 0}));
	EXPECT_EQ(corners.at(3), (maze::GridStep {.across = 0, .along = 14}));
	const auto lanterns = maze::LanternSteps(8);
	EXPECT_EQ(lanterns.at(0), (maze::GridStep {.across = 7, .along = 0}));
	EXPECT_EQ(lanterns.at(1), (maze::GridStep {.across = 16, .along = 7}));
	EXPECT_EQ(lanterns.at(2), (maze::GridStep {.across = 7, .along = 16}));
	EXPECT_EQ(lanterns.at(3), (maze::GridStep {.across = 0, .along = 7}));
	const auto bells = maze::BellPlaces(7);
	EXPECT_EQ(bells.at(0).step, (maze::GridStep {.across = 7, .along = 17}));
	EXPECT_EQ(bells.at(1).step, (maze::GridStep {.across = 17, .along = 7}));
	EXPECT_EQ(bells.at(2).step, (maze::GridStep {.across = 7, .along = -3}));
	EXPECT_EQ(bells.at(3).step, (maze::GridStep {.across = -3, .along = 7}));
	EXPECT_EQ(bells.at(maze::k_ResetBell).step, (maze::GridStep {.across = -3, .along = -3}));
	EXPECT_FLOAT_EQ(bells.at(1).angle, 4.5f);
	EXPECT_FLOAT_EQ(bells.at(maze::k_ResetBell).angle, 2.4f);
}

TEST(MazePuzzleRules, StartCells)
{
	EXPECT_EQ(maze::HeroStart(PuzzleGameType::Thesius1), (maze::Cell {.across = 2, .along = 3}));
	EXPECT_EQ(maze::LionStart(PuzzleGameType::Thesius1), (maze::Cell {.across = 2, .along = 5}));
	EXPECT_EQ(maze::HeroStart(PuzzleGameType::Tilt2), (maze::Cell {.across = 0, .along = 4}));
	EXPECT_EQ(maze::LionStart(PuzzleGameType::Tilt2), (maze::Cell {.across = 6, .along = 4}));
}

TEST(MazePuzzleRules, AnglesOfTheDirections)
{
	EXPECT_EQ(maze::AngleOfDirection(maze::k_AlongUp), 512);
	EXPECT_EQ(maze::AngleOfDirection(maze::k_AcrossUp), 0);
	EXPECT_EQ(maze::AngleOfDirection(maze::k_AlongDown), 1536);
	EXPECT_EQ(maze::AngleOfDirection(maze::k_AcrossDown), 1024);
}

TEST(MazePuzzleRules, MovedChangesOneNumber)
{
	const maze::Cell cell {.across = 3, .along = 3};
	EXPECT_EQ(maze::Moved(cell, maze::k_AlongUp), (maze::Cell {.across = 3, .along = 4}));
	EXPECT_EQ(maze::Moved(cell, maze::k_AcrossUp), (maze::Cell {.across = 4, .along = 3}));
	EXPECT_EQ(maze::Moved(cell, maze::k_AlongDown), (maze::Cell {.across = 3, .along = 2}));
	EXPECT_EQ(maze::Moved(cell, maze::k_AcrossDown), (maze::Cell {.across = 2, .along = 3}));
	EXPECT_EQ(maze::Moved(cell, -1), cell);
}

TEST(MazePuzzleRules, TappedStepFindsTheMiddleOfTheCellUnderThePoint)
{
	const auto steps = maze::MazeSteps(hanoi::StepsOf(0, 1.0f));
	const maze::Grid grid {.place = {.x = 1000000, .z = 2000000}, .steps = steps, .angle = 0, .scale = 1.0f};
	// A quarter of a grid step into the fourth grid step across and the second along
	const auto quarter = steps.cosine / 4;
	const auto tapped =
	    maze::TappedStep(grid, 7, grid.place.x + (steps.cosine * 3) + quarter, grid.place.z + steps.cosine + quarter);
	ASSERT_TRUE(tapped.has_value());
	EXPECT_EQ(*tapped, (maze::GridStep {.across = 3, .along = 1}));
	// The second grid step of a cell is in the same cell
	const auto same = maze::TappedStep(grid, 7, grid.place.x + (steps.cosine * 2) + quarter, grid.place.z + quarter);
	ASSERT_TRUE(same.has_value());
	EXPECT_EQ(*same, (maze::GridStep {.across = 3, .along = 1}));
	// Before the place of the puzzle, past its far corner, or at the corner of the map: no tap
	EXPECT_FALSE(maze::TappedStep(grid, 7, grid.place.x - 1, grid.place.z + quarter).has_value());
	EXPECT_FALSE(maze::TappedStep(grid, 7, grid.place.x + (steps.cosine * 17), grid.place.z + quarter).has_value());
	EXPECT_FALSE(maze::TappedStep(grid, 7, 0, 0).has_value());
}

TEST(MazePuzzleRules, ChooseHeroMove)
{
	const auto type = PuzzleGameType::Thesius1;
	const maze::Cell hero {.across = 3, .along = 3};
	const auto middle = maze::MiddleOf(hero);
	EXPECT_EQ(maze::ChooseHeroMove(type, middle, hero).move, maze::HeroMove::Wait);
	// A tap on another row and another column does nothing
	EXPECT_EQ(maze::ChooseHeroMove(type, {.across = middle.across + 2, .along = middle.along + 2}, hero).move,
	          maze::HeroMove::None);
	for (int32_t direction = 0; direction < maze::k_Directions; ++direction)
	{
		const auto next = maze::MiddleOf(maze::Moved(maze::Moved(hero, direction), direction));
		const auto choice = maze::ChooseHeroMove(type, next, hero);
		if (maze::MoveBlocked(type, hero, direction))
		{
			EXPECT_EQ(choice.move, maze::HeroMove::None);
		}
		else
		{
			EXPECT_EQ(choice.move, maze::HeroMove::Step);
			EXPECT_EQ(choice.direction, direction);
		}
	}
}

TEST(MazePuzzleRules, LionMovesAtMostTwoCellsTowardsTheVillager)
{
	for (const auto type : {PuzzleGameType::Thesius1, PuzzleGameType::Thesius2})
	{
		for (int32_t across = 0; across < maze::SideOf(type); ++across)
		{
			for (int32_t along = 0; along < maze::SideOf(type); ++along)
			{
				const maze::Cell lion {.across = across, .along = along};
				const maze::Cell hero {.across = 3, .along = 4};
				const auto moved = maze::MoveLion(type, lion, hero);
				const auto distance = std::abs(moved.cell.across - lion.across) + std::abs(moved.cell.along - lion.along);
				EXPECT_LE(distance, 2);
				EXPECT_EQ(moved.angle.has_value(), !(moved.cell == lion));
				// It never moves away from the villager
				EXPECT_LE(std::abs(moved.cell.across - hero.across), std::abs(lion.across - hero.across));
				EXPECT_LE(std::abs(moved.cell.along - hero.along), std::abs(lion.along - hero.along));
			}
		}
	}
}

TEST(MazePuzzleRules, LionAnglesFromItsMoves)
{
	// With no walls in the way, a lion two cells down across and along from the villager moves across first
	const auto type = PuzzleGameType::Thesius1;
	for (int32_t across = 0; across < 7; ++across)
	{
		for (int32_t along = 0; along < 7; ++along)
		{
			const maze::Cell lion {.across = across, .along = along};
			const auto moved = maze::MoveLion(type, lion, {.across = 6, .along = 6});
			if (moved.cell.across == lion.across + 2)
			{
				EXPECT_EQ(moved.angle, std::optional<uint16_t>(0));
			}
			if (moved.cell.along == lion.along + 2)
			{
				EXPECT_EQ(moved.angle, std::optional<uint16_t>(512));
			}
		}
	}
}

TEST(MazePuzzleRules, SlideStopsAtAWall)
{
	for (const auto type : {PuzzleGameType::Tilt1, PuzzleGameType::Tilt2})
	{
		for (int32_t direction = 0; direction < maze::k_Directions; ++direction)
		{
			const auto end = maze::Slide(type, maze::LionStart(type), direction);
			EXPECT_TRUE(maze::MoveBlocked(type, end, direction));
		}
	}
}

TEST(MazePuzzleRules, EscapedAtTheFarEdge)
{
	EXPECT_TRUE(maze::Escaped(PuzzleGameType::Thesius1, {.across = 7, .along = 2}));
	EXPECT_TRUE(maze::Escaped(PuzzleGameType::Thesius1, {.across = 2, .along = 7}));
	EXPECT_FALSE(maze::Escaped(PuzzleGameType::Thesius1, {.across = 6, .along = 6}));
	EXPECT_TRUE(maze::Escaped(PuzzleGameType::Thesius2, {.across = 3, .along = 8}));
}

TEST(MazePuzzleRules, EachMazeHasASolution)
{
	EXPECT_TRUE(TheseusSolvable(PuzzleGameType::Thesius1));
	EXPECT_TRUE(TheseusSolvable(PuzzleGameType::Thesius2));
	EXPECT_TRUE(TiltSolvable(PuzzleGameType::Tilt1));
	EXPECT_TRUE(TiltSolvable(PuzzleGameType::Tilt2));
}

TEST(MazePuzzleRules, DeleteTakesTheThingsOfAMazeInOrder)
{
	openblack::ecs::components::PuzzleGame game {.type = PuzzleGameType::Tilt1};
	const auto thing = [](uint32_t number) { return static_cast<entt::entity>(number); };
	game.maze.palms.at(0) = thing(1);
	game.lanterns.at(0) = thing(2);
	game.maze.palms.at(1) = thing(3);
	game.maze.lion = thing(4);
	game.maze.hero = thing(5);
	game.maze.alongFences.at(0) = thing(6);
	game.maze.acrossFences.at(0) = thing(7);
	game.maze.acrossFences.at(9) = thing(8);
	game.maze.bells.at(4) = thing(9);
	const auto pieces =
	    openblack::ecs::puzzle_game::TakePiecesForDelete(game, [&thing](entt::entity piece) { return piece != thing(3); });
	const std::vector<entt::entity> expected {thing(1), thing(2), thing(4), thing(5), thing(7), thing(6), thing(8), thing(9)};
	EXPECT_EQ(pieces, expected);
	EXPECT_EQ(game.maze.palms.at(1), thing(3));
	EXPECT_EQ(game.maze.hero, entt::entity {entt::null});
	EXPECT_EQ(game.maze.bells.at(4), entt::entity {entt::null});
	EXPECT_TRUE(game.played);
}

TEST(MazePuzzleRules, AScriptHeightOfATotemIsWholeAndNotMoreThanItsGreatestHeight)
{
	using openblack::ecs::maze_puzzle::TotemHeight;
	EXPECT_EQ(TotemHeight(42.9f, 100), 42);
	EXPECT_EQ(TotemHeight(-3.7f, 100), -3);
	EXPECT_EQ(TotemHeight(100.0f, 100), 100);
	EXPECT_EQ(TotemHeight(250.0f, 100), 100);
	EXPECT_EQ(TotemHeight(5.0f, -1), -1);
	EXPECT_EQ(TotemHeight(-2.5f, -1), -2);
}

// A save keeps a maze with its fences, its animals, their cells and move stages, its bells and the bells' heights
TEST(MazePuzzleRules, ASaveKeepsTheMazeAndItsPieces)
{
	using openblack::ecs::components::PuzzleGame;
	using openblack::ecs::components::PuzzlePiece;
	using openblack::ecs::components::PuzzleTotem;
	namespace reflection = openblack::ecs::reflection;
	entt::meta_ctx context;
	reflection::RegisterComponents(context);
	entt::registry registry;
	const auto puzzle = registry.create();
	const auto alongFence = registry.create();
	const auto acrossFence = registry.create();
	const auto sheep = registry.create();
	const auto lion = registry.create();
	const auto bell = registry.create();
	PuzzleGame game {.type = PuzzleGameType::Tilt1, .angle = 512, .started = true};
	game.maze.alongFences.at(10) = alongFence;
	game.maze.acrossFences.at(11) = acrossFence;
	game.maze.hero = sheep;
	game.maze.lion = lion;
	game.maze.heroAcross = 3;
	game.maze.lionAlong = 5;
	game.maze.lionStage = 1;
	game.maze.lionAngle = 1536;
	game.maze.bells.at(4) = bell;
	registry.emplace<PuzzleGame>(puzzle, game);
	registry.emplace<PuzzlePiece>(sheep, PuzzlePiece {.puzzle = puzzle});
	registry.emplace<PuzzlePiece>(lion, PuzzlePiece {.puzzle = puzzle});
	registry.emplace<PuzzleTotem>(bell, PuzzleTotem {.puzzle = puzzle, .highest = maze::k_TappableHighest, .height = -1});

	reflection::WorldSaveReport report;
	const auto bytes = reflection::SaveWorld(registry, context, &report);
	EXPECT_TRUE(report.unsavedValues.empty());
	entt::registry loaded;
	ASSERT_TRUE(reflection::LoadWorld(loaded, context, bytes, reflection::WorldLoadMode::Exact).has_value());
	const auto& maze = loaded.get<PuzzleGame>(puzzle).maze;
	EXPECT_EQ(maze.alongFences.at(10), alongFence);
	EXPECT_EQ(maze.acrossFences.at(11), acrossFence);
	EXPECT_EQ(maze.hero, sheep);
	EXPECT_EQ(maze.lion, lion);
	EXPECT_EQ(maze.heroAcross, 3);
	EXPECT_EQ(maze.lionAlong, 5);
	EXPECT_EQ(maze.lionStage, 1);
	EXPECT_EQ(maze.bells.at(4), bell);
	EXPECT_EQ(loaded.get<PuzzlePiece>(lion).puzzle, puzzle);
	EXPECT_EQ(loaded.get<PuzzleTotem>(bell).highest, maze::k_TappableHighest);
	EXPECT_EQ(loaded.get<PuzzleTotem>(bell).height, -1);
}

// After a load the game gives back a puzzle with the calm count 5, no marker, both animals at angle 0, and no fences
// towards the next cell across. The other state stays.
TEST(MazePuzzleRules, ALoadForgetsWhatTheGameDoesNotSave)
{
	using openblack::ecs::components::PuzzleGame;
	PuzzleGame game {.type = PuzzleGameType::Thesius1, .calmTurns = -3};
	game.maze.marker = 7;
	game.maze.heroAngle = 1024;
	game.maze.lionAngle = 512;
	game.maze.heroStage = 1;
	game.maze.lionStage = 2;
	game.maze.heroAlong = 4;
	game.maze.acrossFences.fill(static_cast<entt::entity>(3));
	game.maze.alongFences.fill(static_cast<entt::entity>(5));
	openblack::ecs::puzzle_game::ForgetUnsavedState(game);
	EXPECT_EQ(game.calmTurns, openblack::ecs::puzzle_game::k_CalmTurnsAfterLoad);
	EXPECT_EQ(game.maze.marker, 0u);
	EXPECT_EQ(game.maze.heroAngle, 0);
	EXPECT_EQ(game.maze.lionAngle, 0);
	EXPECT_EQ(game.maze.heroStage, 1);
	EXPECT_EQ(game.maze.lionStage, 2);
	EXPECT_EQ(game.maze.heroAlong, 4);
	for (size_t slot = 0; slot < game.maze.acrossFences.size(); ++slot)
	{
		EXPECT_EQ(game.maze.acrossFences.at(slot), entt::entity {entt::null});
		EXPECT_EQ(game.maze.alongFences.at(slot), static_cast<entt::entity>(5));
	}
}
