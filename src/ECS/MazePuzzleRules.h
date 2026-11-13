/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include "3D/MapCoords.h"
#include "ECS/HanoiRules.h"
#include "Enums.h"
#include "ScriptHeaders/ScriptEnums.h"

/// The rules of the maze puzzles. Fences make a maze on a square grid of cells.
/// - In the two Theseus mazes, the player taps the land to move a villager one or more cells in a line. Then a lion
///   moves up to two cells towards the villager. The villager must go out of the maze before the lion catches it.
/// - In the lion maze, the player taps one of four bells. The lion then slides in the direction of the bell until a
///   fence stops it. The lion must stop on the cell of the sheep. A fifth bell puts the lion and the sheep back.
///
/// A cell is a pair of numbers across and along. A grid step is half a cell: the middle of a cell is two times its
/// number plus one, and a fence stands on an even grid step between two cells.
namespace openblack::ecs::maze_puzzle
{

/// The directions of a move, as the game numbers them: along up, across up, along down and across down
inline constexpr int32_t k_AlongUp = 0;
inline constexpr int32_t k_AcrossUp = 1;
inline constexpr int32_t k_AlongDown = 2;
inline constexpr int32_t k_AcrossDown = 3;
inline constexpr int32_t k_Directions = 4;

/// The number of cells in each row of the largest maze, and the size of the lists of the fences
inline constexpr int32_t k_MostSide = 8;
inline constexpr size_t k_FenceSlots = 81;

/// The maze puzzle divides the steps of its grid by this number when it starts
inline constexpr int32_t k_StepDivisor = 5;

/// The kinds of the things of a maze
inline constexpr MobileStaticInfo k_FenceKind = MobileStaticInfo::CeltFenceShort;
inline constexpr TreeInfo k_CornerTreeKind = TreeInfo::Palm;
inline constexpr MobileStaticInfo k_LanternKind = MobileStaticInfo::StreetLantern;
inline constexpr AbodeInfo k_BellKind = AbodeInfo::Belltower;
inline constexpr AnimalInfo k_TheseusHeroKind = AnimalInfo::PuzzleVillager;
inline constexpr AnimalInfo k_SheepKind = AnimalInfo::PuzzleSheep;
inline constexpr AnimalInfo k_LionKind = AnimalInfo::PuzzleWolf;

/// Each animal of a maze starts at this age. The sheep and the lion are two times their usual size.
inline constexpr uint32_t k_PieceAge = 20;
inline constexpr float k_LargePieceScale = 2.0f;
/// An animal of a maze moves to its next cell at this speed, in metres a second, and then decides what to do
inline constexpr float k_PieceMetresPerSecond = 4.0f;
/// The top state of an animal that moves to a place
inline constexpr uint8_t k_MoveToPosState = 1;

/// The spot visual that marks a tapped cell, and the spot visuals at the start, at a bell and at a solution
inline constexpr SpotVisualType k_SpotVisual = SpotVisualType::CreatureTarget;
inline constexpr float k_MarkerMagnitude = 1.0f;
inline constexpr float k_SparkleMagnitude = 2.0f;
inline constexpr int k_StartSparkleTurns = 10;
inline constexpr int k_SparkleTurns = 3;

/// A bell rings with this sound of the scripts' bank, from the bell. A tap on a bell plays this sound of the game's
/// bank, heard everywhere.
inline constexpr uint32_t k_BellSound = 92;
inline constexpr uint32_t k_BellTapSound = 1;

/// The lion maze has four bells that move the lion and a fifth bell that puts the puzzle back
inline constexpr size_t k_MoveBells = 4;
inline constexpr size_t k_Bells = 5;
inline constexpr size_t k_ResetBell = 4;
/// A bell is half the size of a usual bell tower
inline constexpr float k_BellScale = 0.5f;
/// A puzzle totem with this greatest height takes the taps of the hand, and a tap sets its height to this number
inline constexpr int32_t k_TappableHighest = -1;

/// The two steps of the grid, the game angle and the size of a maze puzzle
struct Grid
{
	map_coords::MapCoords place {};
	hanoi::Steps steps {};
	uint16_t angle {0};
	float scale {1.0f};
};

/// A cell of a maze
struct Cell
{
	int32_t across {0};
	int32_t along {0};

	bool operator==(const Cell& other) const = default;
};

/// A place on the grid, in grid steps
struct GridStep
{
	int32_t across {0};
	int32_t along {0};

	bool operator==(const GridStep& other) const = default;
};

/// Whether a puzzle is one of the Theseus mazes, the lion maze or any maze
[[nodiscard]] constexpr bool IsTheseus(script::PuzzleGameType type) noexcept
{
	return type == script::PuzzleGameType::Thesius1 || type == script::PuzzleGameType::Thesius2;
}
[[nodiscard]] constexpr bool IsTilt(script::PuzzleGameType type) noexcept
{
	return type == script::PuzzleGameType::Tilt1 || type == script::PuzzleGameType::Tilt2;
}
[[nodiscard]] constexpr bool IsMaze(script::PuzzleGameType type) noexcept
{
	return IsTheseus(type) || IsTilt(type);
}

/// The number of cells in each row of a maze: seven for the first Theseus maze and the lion maze, eight for the others
[[nodiscard]] int32_t SideOf(script::PuzzleGameType type) noexcept;

/// The grid of a maze: the steps of the puzzle, each divided by five as the maze starts
[[nodiscard]] hanoi::Steps MazeSteps(hanoi::Steps steps) noexcept;

/// The middle of a cell in grid steps
[[nodiscard]] constexpr GridStep MiddleOf(Cell cell) noexcept
{
	return {.across = (cell.across * 2) + 1, .along = (cell.along * 2) + 1};
}

/// Whether the layout of a maze has a wall on a side of a cell. Side 0 is the wall towards the next cell along, and
/// side 1 is the wall towards the next cell across. A cell off the layout and any other side give yes.
[[nodiscard]] bool HasWall(script::PuzzleGameType type, int32_t across, int32_t along, int32_t side) noexcept;

/// Whether a wall stops a move from a cell in a direction. A number that is not a direction gives no.
[[nodiscard]] bool MoveBlocked(script::PuzzleGameType type, Cell cell, int32_t direction) noexcept;

/// A fence of a maze: on the side of a cell towards the next cell along, or towards the next cell across
struct Fence
{
	int32_t across {0};
	int32_t along {0};
	bool facesAcross {false};

	bool operator==(const Fence& other) const = default;
};
/// The fences of a maze, in the order that the puzzle makes them
[[nodiscard]] std::vector<Fence> FencesOf(script::PuzzleGameType type);
/// The slot of a fence in its list
[[nodiscard]] constexpr size_t SlotOf(const Fence& fence) noexcept
{
	return static_cast<size_t>((fence.across * 9) + fence.along);
}
/// Where a fence stands, in grid steps
[[nodiscard]] GridStep FenceStep(const Fence& fence) noexcept;
/// The angle of a fence round the vertical axis in radians, from the game angle of the puzzle. A fence towards the next
/// cell across turns a quarter of a circle more.
[[nodiscard]] float FenceAngle(uint16_t angle, bool facesAcross) noexcept;

/// The four corners of a maze, where palm trees stand, in grid steps
[[nodiscard]] std::array<GridStep, 4> CornerSteps(int32_t side) noexcept;
/// The four street lanterns of a maze, one at the middle of each edge, in grid steps
[[nodiscard]] std::array<GridStep, 4> LanternSteps(int32_t side) noexcept;

/// The first cell of the villager or the sheep, and the first cell of the lion
[[nodiscard]] Cell HeroStart(script::PuzzleGameType type) noexcept;
[[nodiscard]] Cell LionStart(script::PuzzleGameType type) noexcept;

/// The place of a tapped point on the grid of a maze: the middle of the cell under the point, in grid steps. A point
/// outside the box from the place of the puzzle to the far corner of the grid gives none, and so does a point at the
/// corner of the map.
[[nodiscard]] std::optional<GridStep> TappedStep(const Grid& grid, int32_t side, int32_t tapX, int32_t tapZ) noexcept;

/// What a tap asks the villager of a Theseus maze to do
enum class HeroMove : uint8_t
{
	/// The tap is not on the same row or column as the villager, or a wall stops the move: nothing happens
	None,
	/// The tap is on the villager's own cell: the villager stays and the lion moves
	Wait,
	/// The villager moves one cell in the direction of the tap
	Step,
};
struct HeroChoice
{
	HeroMove move {HeroMove::None};
	int32_t direction {0};
};
/// The move that a tap on a place asks of the villager on its cell
[[nodiscard]] HeroChoice ChooseHeroMove(script::PuzzleGameType type, GridStep tapped, Cell hero) noexcept;

/// The game angle that an animal of a maze faces to move in a direction
[[nodiscard]] uint16_t AngleOfDirection(int32_t direction) noexcept;

/// The cell after a move in a direction
[[nodiscard]] Cell Moved(Cell cell, int32_t direction) noexcept;

/// The lion of a Theseus maze moves up to two cells towards the villager. In each of its moves it goes across if it can,
/// otherwise along. It stops when a wall stops it or when it has nowhere to go. The result is its cell and the angle
/// that it then faces, or no angle when it did not move.
struct LionMove
{
	Cell cell {};
	std::optional<uint16_t> angle;
};
[[nodiscard]] LionMove MoveLion(script::PuzzleGameType type, Cell lion, Cell hero) noexcept;

/// The lion of the lion maze slides in a direction until a wall stops it
[[nodiscard]] Cell Slide(script::PuzzleGameType type, Cell lion, int32_t direction) noexcept;

/// Whether the villager of a Theseus maze is out of the maze
[[nodiscard]] bool Escaped(script::PuzzleGameType type, Cell hero) noexcept;

/// A bell of the lion maze: where it stands, in grid steps, and its angle round the vertical axis in radians
struct BellPlace
{
	GridStep step {};
	float angle {0.0f};
};
[[nodiscard]] std::array<BellPlace, k_Bells> BellPlaces(int32_t side) noexcept;

/// The height that a script gives a puzzle totem: the whole part of the value, towards zero, when it is less than the
/// greatest height, else the greatest height
[[nodiscard]] constexpr int32_t TotemHeight(float value, int32_t highest) noexcept
{
	return value < static_cast<float>(highest) ? map_coords::FtoL(value) : highest;
}

} // namespace openblack::ecs::maze_puzzle
