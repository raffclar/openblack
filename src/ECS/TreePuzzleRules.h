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
#include <span>

#include "Enums.h"
#include "ScriptHeaders/ScriptEnums.h"

/// The rules of the tree puzzles: nine trees on a grid of three by three spots. Each spot has a state that shows as the
/// kind of its tree. When the player pulls a tree from its spot, that spot and the four spots next to it change state,
/// and each changed spot gets a new small tree that grows.
namespace openblack::ecs::tree_puzzle
{

/// The grid has three rows of three spots
inline constexpr int32_t k_Side = 3;
inline constexpr size_t k_Spots = 9;
/// A new tree starts at this size and grows by this much each turn, up to its full size
inline constexpr float k_StartSize = 0.1f;
inline constexpr float k_GrowthStep = 0.05f;
/// At the start, the puzzle changes this many random spots and their neighbours, as a pull does
inline constexpr int32_t k_StartMoves = 7;
/// A lantern stands one step out from each corner of the grid
inline constexpr std::array<std::array<int32_t, 2>, 4> k_LanternSpots {{{-1, -1}, {-1, 3}, {3, -1}, {3, 3}}};
/// The flock of bats lives over the middle spot
inline constexpr std::array<int32_t, 2> k_MiddleSpot {1, 1};
/// A puff of mist over each spot: its colour (0xAARRGGBB), its size and how much it shrinks edge on
inline constexpr uint32_t k_MistColour = 0x143B658Bu;
inline constexpr float k_MistSize = 1.0f;
inline constexpr float k_MistEdgeShrink = 4.0f;
/// One bat for each spot, all of this age, in a flock of the puzzle's own number that keeps within these distances
inline constexpr AnimalInfo k_BatKind = AnimalInfo::Bat;
inline constexpr uint32_t k_BatAge = 20;
inline constexpr int32_t k_FlockNumber = 0xABA52;
inline constexpr float k_FlockReach = 25.0f;
inline constexpr float k_FlockDistance = 24.0f;
/// The animal states that the puzzle gives the bats: decide what to do, at the start; start to die, at the solution
inline constexpr uint32_t k_BatStartState = 43;
inline constexpr uint32_t k_BatSolvedState = 13;
/// When the player solves the puzzle, each tree keeps this much life and catches fire at this speed
inline constexpr float k_SolvedTreeLife = 0.5f;
inline constexpr float k_SolvedTreeFireSpeed = 1.0f;

/// The number of states of a spot: two for the first tree puzzle, three for the other
[[nodiscard]] constexpr int32_t StatesOf(script::PuzzleGameType type) noexcept
{
	return type == script::PuzzleGameType::Tree1 ? 2 : 3;
}

/// The spot's place in the list of spots: the rows across first
[[nodiscard]] constexpr size_t SpotIndex(int32_t across, int32_t along) noexcept
{
	return static_cast<size_t>(across * k_Side + along);
}

/// Whether a spot is on the grid
[[nodiscard]] constexpr bool OnGrid(int32_t across, int32_t along) noexcept
{
	return across >= 0 && across < k_Side && along >= 0 && along < k_Side;
}

/// The state that a spot shows: the remainder of its count of changes. The count is a signed byte that wraps after 127
/// changes, and a negative count gives a negative remainder.
[[nodiscard]] constexpr int32_t ShownState(script::PuzzleGameType type, int8_t count) noexcept
{
	return static_cast<int32_t>(count) % StatesOf(type);
}

/// The kind of tree that grows on a spot: a conifer, an oak or a birch. A negative state gives a beech.
[[nodiscard]] constexpr TreeInfo TreeKindOf(script::PuzzleGameType type, int8_t count) noexcept
{
	switch (ShownState(type, count))
	{
	case 0:
		return TreeInfo::Conifer;
	case 1:
		return TreeInfo::Oak;
	case 2:
		return TreeInfo::Birch;
	default:
		return TreeInfo::Beech;
	}
}

/// The full size of the tree on a spot. A negative state gives a full size of 0.
[[nodiscard]] constexpr float FullSizeOf(script::PuzzleGameType type, int8_t count) noexcept
{
	const auto state = ShownState(type, count);
	return state >= 0 && state <= 2 ? 1.0f : 0.0f;
}

/// One more change of a spot. The count wraps as a signed byte.
[[nodiscard]] constexpr int8_t Changed(int8_t count) noexcept
{
	return static_cast<int8_t>(count + 1);
}

/// The spots that a pull changes, in this order:
/// - the next spot across and the spot before it across
/// - the next spot along and the spot before it along
/// - the spot itself.
/// A spot off the grid does not change.
[[nodiscard]] constexpr std::array<std::array<int32_t, 2>, 5> PulledSpots(int32_t across, int32_t along) noexcept
{
	return {{{across + 1, along}, {across - 1, along}, {across, along + 1}, {across, along - 1}, {across, along}}};
}

/// Whether all spots show the same state as the first spot
[[nodiscard]] constexpr bool SameState(script::PuzzleGameType type, std::span<const int8_t, k_Spots> counts) noexcept
{
	const auto first = ShownState(type, counts[0]);
	for (const auto count : counts)
	{
		if (ShownState(type, count) != first)
		{
			return false;
		}
	}
	return true;
}

/// The size of a tree after one turn of growth: one step more, but not more than its full size
[[nodiscard]] constexpr float Grown(float size, float fullSize) noexcept
{
	const float next = size + k_GrowthStep;
	return next < fullSize ? next : fullSize;
}

} // namespace openblack::ecs::tree_puzzle
