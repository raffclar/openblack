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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs::components
{

/// A piece of the beach temple puzzle: a base or a ring. The puzzle moves it back to its place when it moves off it,
/// and puts a dropped ring on the stack that it lands near.
struct HanoiBlock
{
	/// The puzzle that the piece belongs to. None after the puzzle goes.
	entt::entity puzzle {entt::null};
	/// The place of the piece on the map, with the altitude above the ground
	map_coords::MapCoords home {};
	/// Where the puzzle put the piece in the world. The piece is off its place once it is anywhere else.
	glm::vec3 placedAt {0.0f};
	/// The piece that rests on this piece, and the piece that this piece rests on
	entt::entity above {entt::null};
	entt::entity below {entt::null};
	/// The puzzle is solved: the ring stays where it is and the hand can not take it
	bool fixed {false};
};

/// A list of a count of things, none of them there
template <size_t Count>
[[nodiscard]] constexpr std::array<entt::entity, Count> NoThings() noexcept
{
	std::array<entt::entity, Count> things {};
	things.fill(entt::null);
	return things;
}

/// A puzzle game that a script made. Each kind has its own pieces; the beach temple puzzle and the tree puzzles have
/// pieces now. The player cannot solve a kind that has none.
struct PuzzleGame
{
	script::PuzzleGameType type {script::PuzzleGameType::None};
	/// The place of the puzzle on the map, at altitude 0
	map_coords::MapCoords place {};
	/// The game angle and the size that the script gave
	uint16_t angle {0};
	float scale {1.0f};
	/// The steps of the puzzle's grid in map units: the game's cosine and sine of the angle, times the size
	int32_t cosineStep {0};
	int32_t sineStep {0};
	/// The puzzle is solved, and its "played" test gives yes from now on
	bool played {false};
	/// The player put a piece in place at least once
	bool started {false};
	/// The count of calm turns, in which no ring was off its place
	int32_t calmTurns {5};
	/// Puzzles take their turns in the order of this number, the newest first
	uint32_t order {0};
	/// The beach temple puzzle's three bases and four rings, the largest ring first
	std::array<entt::entity, 3> bases {entt::null, entt::null, entt::null};
	std::array<entt::entity, 4> rings {entt::null, entt::null, entt::null, entt::null};
	/// The last state of each piece, the bases first, from which the puzzle makes a piece again that went
	std::array<HanoiBlock, 7> kept {};
	/// The tree puzzle's count of changes of each spot, the rows across first. The count gives the state of the spot.
	std::array<int8_t, 9> treeCounts {};
	/// The tree on each spot, and where the puzzle put it in the world. The tree is off its spot once it is anywhere else.
	std::array<entt::entity, 9> trees {NoThings<9>()};
	std::array<glm::vec3, 9> treesPlacedAt {};
	/// The puff of mist over each spot and the bat for each spot, until the solution
	std::array<entt::entity, 9> mists {NoThings<9>()};
	std::array<entt::entity, 9> bats {NoThings<9>()};
	/// The lantern at each corner of the tree puzzle
	std::array<entt::entity, 4> lanterns {NoThings<4>()};
};

} // namespace openblack::ecs::components
