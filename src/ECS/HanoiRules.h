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

#include "3D/MapCoords.h"
#include "Enums.h"

/// The rules of the beach temple puzzle: four rings of different sizes on three bases in a row. The player moves the
/// rings by hand, one at a time, and a ring can rest only on a base or on a larger ring.
namespace openblack::ecs::hanoi
{

/// The puzzle has three bases and four rings
inline constexpr size_t k_Bases = 3;
inline constexpr size_t k_Rings = 4;
/// The kinds of the pieces, the largest ring first
inline constexpr MobileObjectInfo k_BaseKind = MobileObjectInfo::HanoiPuzzleBase;
inline constexpr std::array<MobileObjectInfo, k_Rings> k_RingKinds {
    MobileObjectInfo::HanoiPuzzlePart1, MobileObjectInfo::HanoiPuzzlePart2, MobileObjectInfo::HanoiPuzzlePart3,
    MobileObjectInfo::HanoiPuzzlePart4};
/// A dropped ring settles onto the stack of a piece within this distance of it, in metres
inline constexpr float k_SettleReach = 10.0f;
/// The search for a stack looks at this many map cells, in a spiral out from the ring's cell
inline constexpr int32_t k_SettleCells = 9;
/// The count of calm turns starts at its top. Each turn with a ring off its place takes one away, down to the bottom.
/// Each calm turn adds one, up to the top.
inline constexpr int32_t k_CalmTop = 5;
inline constexpr int32_t k_CalmBottom = -5;
/// The hand can take a ring only after more calm turns than this
inline constexpr int32_t k_CalmToPickUp = 4;
/// The bases stand two steps of the puzzle apart
inline constexpr int32_t k_BaseSpacing = 2;

/// How much higher a piece puts a ring that rests on it: the height of the piece. A base has no height.
[[nodiscard]] float HeightOf(MobileObjectInfo kind);

/// Whether a ring can rest on the top piece of a stack: on a base, or on a larger ring. A ring of the same size as the
/// smallest ring can rest on the smallest ring, as the game allows, but the puzzle has only one of each size.
[[nodiscard]] bool MayRestOn(MobileObjectInfo ring, MobileObjectInfo top);

/// The game angle that a puzzle keeps from the angle that the script gives in radians: truncated, not masked
[[nodiscard]] int32_t GameAngleOf(float radians);

/// The two steps of a puzzle's grid in map units: the game's cosine and sine of the puzzle's angle, each times the
/// puzzle's size and truncated
struct Steps
{
	int32_t cosine {0};
	int32_t sine {0};
};
/// The steps for a game angle and a size. The game reads the angle as a whole 16-bit index of its sine table. An angle
/// past the table reads past its end in the game, so this function wraps such an angle onto the circle.
[[nodiscard]] Steps StepsOf(uint16_t angle, float scale);

/// The place on the puzzle's grid a number of steps across and along from the puzzle's place, at the puzzle's altitude.
/// A step along moves by the sine on x and by the cosine on z. A step across moves by the cosine on x and by minus the
/// sine on z. The sums wrap as 32-bit integers.
[[nodiscard]] map_coords::MapCoords GridPlace(const map_coords::MapCoords& puzzle, Steps steps, int32_t across, int32_t along);

/// The beach temple puzzle doubles its steps when it starts. The products wrap as 32-bit integers.
[[nodiscard]] Steps Spaced(Steps steps);

/// The place of a base, from the doubled steps: the bases go along from the puzzle's place, the first at the place
[[nodiscard]] map_coords::MapCoords BasePlace(const map_coords::MapCoords& puzzle, Steps spaced, int32_t base);

/// The places of the four rings at the start: stacked on the first base, the largest at the bottom
[[nodiscard]] std::array<map_coords::MapCoords, k_Rings> StartRingPlaces(const map_coords::MapCoords& firstBase);

/// The links of the pieces, by any key that names a piece (an entity in the game, a number in a test). None is no piece.
template <typename Key>
struct Stacks
{
	std::array<Key, k_Bases> bases {};
	std::array<Key, k_Rings> rings {};
	/// The piece that each ring rests on, the largest ring first
	std::array<Key, k_Rings> below {};
};

/// Whether the puzzle is solved: each ring rests on the next larger ring, and the largest ring rests on neither of the
/// first two bases. The largest ring on nothing at all also counts, but it cannot be bare while the others rest on it.
template <typename Key>
[[nodiscard]] constexpr bool Solved(const Stacks<Key>& stacks)
{
	for (size_t ring = k_Rings - 1; ring > 0; --ring)
	{
		if (stacks.below.at(ring) != stacks.rings.at(ring - 1))
		{
			return false;
		}
	}
	const auto& bottom = stacks.below.front();
	return bottom != stacks.bases.at(0) && bottom != stacks.bases.at(1);
}

/// What decides whether the hand may take a piece
struct PickUpFacts
{
	bool base {false};
	/// Another piece rests on it
	bool covered {false};
	/// It belongs to a puzzle
	bool hasPuzzle {false};
	/// The puzzle's count of calm turns
	int32_t calm {k_CalmTop};
	/// The puzzle is solved, which fixed the ring
	bool fixed {false};
};
/// The hand may not take a base, a piece with another piece on it, a piece of no puzzle, a piece of a puzzle that is
/// not calm for long enough, or a ring of a solved puzzle
[[nodiscard]] constexpr bool CannotBePickedUp(const PickUpFacts& facts)
{
	return facts.base || facts.covered || !facts.hasPuzzle || facts.calm <= k_CalmToPickUp || facts.fixed;
}

/// A turn with a ring off its place takes one from the count of calm turns, down to the bottom
[[nodiscard]] constexpr int32_t CalmAfterRestlessTurn(int32_t calm)
{
	return calm > k_CalmBottom ? calm - 1 : calm;
}
/// A calm turn adds one, up to the top
[[nodiscard]] constexpr int32_t CalmAfterCalmTurn(int32_t calm)
{
	return calm < k_CalmTop ? calm + 1 : calm;
}

} // namespace openblack::ecs::hanoi
