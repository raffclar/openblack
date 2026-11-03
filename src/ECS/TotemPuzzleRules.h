/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>

#include "3D/MapCoords.h"
#include "Enums.h"
#include "ScriptHeaders/ScriptEnums.h"

/// The rules of the totem puzzles (the puzzle game kinds with six totems). The player raises and lowers the totems with
/// the hand. Each totem has five positions, 0 to 4. A change of one totem changes some of the others by a set amount, in
/// steps of five positions that go round. The puzzle is solved when all six totems are fully up.
///
/// Two kinds put the totems in a row, and two kinds put them on a curve round the puzzle's place. Two kinds have one
/// table of the changes, and the other two kinds have a second table.
namespace openblack::ecs::totem_puzzle
{

/// The count of totems, and the greatest height of each. A totem has the positions 0 to its greatest height.
inline constexpr size_t k_Totems = 6;
inline constexpr int32_t k_Highest = 4;
inline constexpr int32_t k_Positions = k_Highest + 1;
/// The kind of building that each totem is
inline constexpr AbodeInfo k_TotemKind = AbodeInfo::JapaneseTotem;
/// The size of each totem
inline constexpr float k_TotemScale = 1.0f;
/// A totem that the puzzle changed slides to its new height in this count of steps
inline constexpr int32_t k_SlideSteps = 5;
/// The spot visual over a totem that stands fully up, and the spot visuals over all totems at the solution, which stay
/// for some turns
inline constexpr float k_MarkMagnitude = 5.0f;
inline constexpr int k_SolvedMarkTurns = 3;

/// The height that the puzzle keeps for each totem
using Heights = std::array<int32_t, k_Totems>;
/// For each totem, how much a change of that totem changes each of the other totems
using Couplings = std::array<std::array<int32_t, k_Totems>, k_Totems>;

/// Whether a puzzle game kind is a totem puzzle
[[nodiscard]] constexpr bool IsTotemPuzzle(script::PuzzleGameType type) noexcept
{
	using script::PuzzleGameType;
	return type == PuzzleGameType::Totem1 || type == PuzzleGameType::Totem2 || type == PuzzleGameType::Totem3 ||
	       type == PuzzleGameType::Totem4;
}

/// Whether a totem puzzle puts its totems on a curve round its place. The other kinds put them in a row.
[[nodiscard]] constexpr bool OnCurve(script::PuzzleGameType type) noexcept
{
	return type == script::PuzzleGameType::Totem3 || type == script::PuzzleGameType::Totem4;
}

/// Whether a script delete of the puzzle also removes its totems. The kind that the fourth land uses keeps them.
[[nodiscard]] constexpr bool DeleteRemovesTotems(script::PuzzleGameType type) noexcept
{
	return type != script::PuzzleGameType::Totem3;
}

/// The table of the changes of a totem puzzle kind
[[nodiscard]] const Couplings& CouplingsOf(script::PuzzleGameType type) noexcept;

/// A change that the player made to one totem, and the heights that the puzzle keeps after it
struct Change
{
	size_t moved {0};
	Heights kept {};
};

/// One turn of a totem puzzle. The first totem whose height is not the height that the puzzle keeps is the moved totem.
/// The puzzle keeps the moved totem's new height and changes each of the other totems by its amount in the table. The
/// change goes round in five positions. Only one totem changes in each turn. There is no change when all heights are
/// the same.
[[nodiscard]] std::optional<Change> Step(script::PuzzleGameType type, const Heights& kept, const Heights& totems);

/// Whether all totems stand fully up
[[nodiscard]] bool Solved(const Heights& kept) noexcept;

/// The offset of a totem from the puzzle's place on the curve, in map units, for the puzzle's size. The totems are one
/// sixth of a half circle apart.
[[nodiscard]] map_coords::MapCoords CurveOffset(size_t totem, float scale);

/// The angle round the vertical axis, in radians, that a totem on the curve turns to: its face looks away from the
/// puzzle's place
[[nodiscard]] float CurveFacing(const map_coords::MapCoords& place, const map_coords::MapCoords& totem);

/// The part of its greatest height that a totem stands at while it slides from its last height to its height. The steps
/// left are the steps after this step; at no steps left the totem is at its height.
[[nodiscard]] float SlideShare(int32_t height, int32_t previous, int32_t stepsLeft, int32_t highest);

} // namespace openblack::ecs::totem_puzzle
