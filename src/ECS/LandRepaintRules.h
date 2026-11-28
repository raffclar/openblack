/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include "ECS/Components/MapCellResident.h"

/// When the land paints the blocks under a thing again. Pure rules, tested on their own.
///
/// The game paints the footprints and the baked shadows of a land block in one paint of the block's texture. A thing
/// asks for this paint only when it goes into the map's cells or comes out of them, and only some things ask:
/// - A thing that stays put asks each time it goes into the cells.
/// - A thing that stays put over the cells of its outline also asks each time it comes out of them, and when its
///   building ends.
/// - A thing that moves asks only when its model has the flag for it. Only the largest ring of the beach temple puzzle
///   gets this flag, at the start of the puzzle.
/// A change of size, place or turn in the same cells asks for no paint. A block that is painted again for another cause
/// then shows all its things as they are at that time.
namespace openblack::ecs::land_repaint
{

/// How the game keeps a thing in the map's cells
enum class Filing : uint8_t
{
	/// A thing that stays put, in each cell that its outline touches: a building, a rock, a feature or a big forest
	Outline,
	/// A thing that stays put, in one cell: a tree or a shield
	OneCell,
	/// A thing that moves: a ball, a chest or another mobile object
	Mobile,
};

/// How a thing in the map's cells is kept in them
[[nodiscard]] Filing FilingOf(const components::MapCellResident& resident);

/// When a thing asks the land to paint the blocks under it again
struct Repaints
{
	/// As it goes into the map's cells
	bool goingIn {true};
	/// As it comes out of them
	bool comingOut {true};

	bool operator==(const Repaints&) const = default;
};

/// When a thing asks for a paint. `flagged` tells whether its model asks for a paint each time it goes into the cells or
/// comes out of them (components::RepaintsLandWhenFiled).
[[nodiscard]] constexpr Repaints RepaintsOf(Filing filing, bool flagged)
{
	if (flagged)
	{
		return {.goingIn = true, .comingOut = true};
	}
	switch (filing)
	{
	case Filing::Outline:
		return {.goingIn = true, .comingOut = true};
	case Filing::OneCell:
		return {.goingIn = true, .comingOut = false};
	case Filing::Mobile:
		break;
	}
	return {.goingIn = false, .comingOut = false};
}

/// What changed in a thing that the land knows, since the last time
struct Change
{
	/// It came out of the map's cells and went into them again, in the same cells or in others
	bool filedAgain {false};
	/// Its place or its turn changed in the same cells
	bool moved {false};
	/// Its size changed in the same cells
	bool resized {false};
	/// Its model, or how it casts its shadow, changed in the same cells
	bool otherLook {false};
	/// Its model became ready to read, or stopped being ready
	bool modelReady {false};
};

/// Which blocks a change paints again: those under the thing as it was, and those under it as it is now
struct Paints
{
	bool oldPlace {false};
	bool newPlace {false};

	bool operator==(const Paints&) const = default;
};

/// Which blocks a change of a known thing paints again. The rules, in order:
/// 1. A model that became ready, or stopped being ready, paints the two places. The game has all its models from the
///    start, thus its first paint of each block shows the thing.
/// 2. A thing that went into the cells again paints its old place if it asks as it comes out, and its new place if it
///    asks as it goes in.
/// 3. A thing that moves paints nothing for a change in its cells.
/// 4. A thing that stays put and moves, turns or changes its look in its cells paints the two places, as at a new filing.
/// 5. A change of size alone paints nothing, as a tree that grows.
[[nodiscard]] constexpr Paints PaintsOf(Filing filing, Repaints repaints, const Change& change)
{
	if (change.modelReady)
	{
		return {.oldPlace = true, .newPlace = true};
	}
	if (change.filedAgain)
	{
		return {.oldPlace = repaints.comingOut, .newPlace = repaints.goingIn};
	}
	if (filing == Filing::Mobile)
	{
		return {};
	}
	if (change.moved || change.otherLook)
	{
		return {.oldPlace = true, .newPlace = true};
	}
	return {};
}

} // namespace openblack::ecs::land_repaint
