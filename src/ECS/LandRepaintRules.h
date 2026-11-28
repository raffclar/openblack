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

#include <optional>

#include "ECS/Components/MapCellResident.h"

/// When the land paints the blocks under a thing again. Pure rules, tested on their own.
///
/// The game paints the footprints and the baked shadows of a land block in one paint of the block's texture. A thing
/// asks for this paint only when it goes into the map's cells or comes out of them, or when its building ends:
/// - A thing that stays put asks each time it goes into the cells.
/// - A thing that stays put over the cells of its outline also asks each time it comes out of them.
/// - A building asks when its building ends.
/// - A plain thing in one cell asks only when its model has the flag for it. Only the largest ring of the beach temple
///   puzzle gets this flag, at the start of the puzzle.
/// A change of size, place, turn or model in the same filing asks for no paint. A block that is painted again for another
/// cause then shows all its things as they are at that time.
namespace openblack::ecs::land_repaint
{

/// How the game keeps a thing in the map's cells
enum class Filing : uint8_t
{
	/// A thing that stays put, in each cell that its outline touches: a building, a rock, a feature, a worship site's
	/// icon or a big forest
	Outline,
	/// A thing that stays put, in one cell, at the front of the cell's list of things that stay put: a tree, a shield, a
	/// scroll or a fish farm
	OneCell,
	/// A plain thing in one cell: a thing that moves, a street lantern, a town's flag, a pot or a seed
	Plain,
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

/// The flag of a thing's model that asks for a paint each time the thing goes into the cells or comes out of them
/// (components::RepaintsLandWhenFiled). The thing gets the flag after it went into the cells. Thus the flag counts as the
/// thing comes out of that filing, and for each filing after it.
struct Flag
{
	/// The count of the thing's filings when it got the flag
	uint32_t afterFiling {0};

	bool operator==(const Flag&) const = default;
};

/// When a thing asks for a paint, for the filing with the number `filings`
[[nodiscard]] constexpr Repaints RepaintsOf(Filing filing, std::optional<Flag> flag, uint32_t filings)
{
	Repaints repaints {.goingIn = false, .comingOut = false};
	switch (filing)
	{
	case Filing::Outline:
		repaints = {.goingIn = true, .comingOut = true};
		break;
	case Filing::OneCell:
		repaints = {.goingIn = true, .comingOut = false};
		break;
	case Filing::Plain:
		break;
	}
	if (flag.has_value())
	{
		repaints.goingIn = repaints.goingIn || filings > flag->afterFiling;
		repaints.comingOut = true;
	}
	return repaints;
}

/// What changed in a thing that the land knows, since the last time
struct Change
{
	/// It came out of the map's cells and went into them again, in the same cells or in others
	bool filedAgain {false};
	/// Its place or its turn changed in the same filing
	bool moved {false};
	/// Its size changed in the same filing
	bool resized {false};
	/// Its model, or how it casts its shadow, changed in the same filing
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
/// 3. A change of place, turn, size or model in the same filing paints nothing.
/// The paint when a building ends is not a change of this kind: a building that casts no shadow until it is built
/// starts to cast, and asks as a thing that goes into the cells.
[[nodiscard]] constexpr Paints PaintsOf(Repaints repaints, const Change& change)
{
	if (change.modelReady)
	{
		return {.oldPlace = true, .newPlace = true};
	}
	if (change.filedAgain)
	{
		return {.oldPlace = repaints.comingOut, .newPlace = repaints.goingIn};
	}
	return {};
}

} // namespace openblack::ecs::land_repaint
