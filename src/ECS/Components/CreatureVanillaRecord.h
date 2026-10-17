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

#include <optional>

#include <SavedObjects.h>

namespace openblack::ecs::components
{

/// A creature that a load took from a saved game in the game's layout. openblack does not have every value of the
/// game's record, so it keeps the record as the load found it. A save writes the record again with the values that
/// openblack changed since the load, and the values of the file for all other fields.
struct CreatureVanillaRecord
{
	/// The number of the creature in the stream of the file
	uint32_t number {0};
	/// The record as the file had it
	gamesave::CreatureObject loaded;
	/// The record that openblack wrote from the creature at once after the load
	gamesave::CreatureObject baseline;
	/// The leash of the creature's player, as the file had it and as openblack wrote it after the load
	std::optional<gamesave::LeashStatusObject> loadedLeash;
	std::optional<gamesave::LeashStatusObject> baselineLeash;
	/// The number of the leash in the stream of the file
	uint32_t leashNumber {0};
};

} // namespace openblack::ecs::components
