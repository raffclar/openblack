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
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <CreatureRecord.h>
#include <LHVMFile.h>

#include "ECS/ScriptObjectTable.h"
#include "Enums.h"

namespace openblack::gamesave
{
class VanillaWorld;
} // namespace openblack::gamesave

// The state of the scripts in a saved game of the game: the table of the objects that the scripts hold, and the
// running scripts with their values. The scripts of the game know an object by its place in the table, and the scripts
// of openblack know it by its entity. The rules here use no service of the game, so that tests can give them a world
// and a script file that they make.
namespace openblack::ecs::vanilla_script_import
{

/// One place of the table in the file
struct FilePlace
{
	/// The number of the object in the file, 0 for no object
	uint32_t number {0};
	bool createdByScript {false};
	/// The number of script references that hold the object
	uint8_t count {0};
};

/// The table of the objects that the scripts hold, as the file keeps it. Place 0 is not used.
struct FileTable
{
	std::array<FilePlace, script_objects::k_Places> places {};
	/// The place where the next search for a free place starts
	uint16_t next {1};
};

/// The table of the game object, nothing when the game object does not keep it
[[nodiscard]] std::optional<FileTable> ReadScriptTable(const gamesave::VanillaWorld& world);

/// The entity that openblack made for the object with a number of the file, nothing when it made none
using EntityOfNumber = std::function<std::optional<uint32_t>(uint32_t number)>;

/// The table for openblack, with the entities in the places of the file
struct ImportedTable
{
	script_objects::Table table;
	/// The places that keep an object of the file
	uint32_t kept {0};
	/// The numbers of the objects of the file that openblack did not make. Their places are free, because no script of
	/// openblack can hold them.
	std::vector<uint32_t> lost;
	/// The places that keep references but no object, because the object went before the save
	uint32_t gone {0};
};

/// Puts each object of the file into its place as the entity that openblack made for it. A place keeps whether a
/// script made its object and its count of references.
[[nodiscard]] ImportedTable ImportTable(const FileTable& file, const EntityOfNumber& entityOf);

/// The number of the first row of the table of the highlights. The game numbers the rows of a table back from the number
/// of its first row, as for the other tables.
constexpr uint32_t k_FirstHighlightRowNumber = 2088;

/// The highlight row of an info number of the file, nothing for a number of another table
[[nodiscard]] std::optional<HighlightInfo> HighlightRowOf(uint32_t number);

/// A point that a script marks
struct ImportedMarker
{
	uint32_t number {0};
	gamesave::MapCoords position;
};

/// A timer that a script made: the turn of the game that set it and the number of turns that it runs for
struct ImportedTimer
{
	uint32_t number {0};
	gamesave::MapCoords position;
	uint32_t setTurn {0};
	int32_t turns {0};
};

/// A scroll or a sign that a script put up
struct ImportedHighlight
{
	uint32_t number {0};
	gamesave::MapCoords position;
	HighlightInfo kind {HighlightInfo::Scroll};
	bool active {false};
	/// The height above the land that a script set, if a script set one
	std::optional<float> drawHeight;
	/// The challenge or the text of a tip, and the category of a tip
	uint32_t text {0};
	uint32_t category {0};
};

/// An influence that a script made for a player, at a point or with an object that it goes with
struct ImportedRing
{
	uint32_t number {0};
	gamesave::MapCoords position;
	/// The number of the object of the file that the influence goes with, 0 for none
	uint32_t follows {0};
	PlayerNames player {PlayerNames::NEUTRAL};
	float radius {0.0f};
	bool anti {false};
};

/// The things of the file that only the scripts make and use
struct ImportedScriptThings
{
	std::vector<ImportedMarker> markers;
	std::vector<ImportedTimer> timers;
	std::vector<ImportedHighlight> highlights;
	std::vector<ImportedRing> rings;
	/// Each record that the import cannot make, and why
	std::vector<std::string> skipped;
};

/// The markers, the timers, the highlights and the influences of the file, in the order of the file
[[nodiscard]] ImportedScriptThings ReadScriptThings(const gamesave::VanillaWorld& world);

/// The player of a temple heart of the file: the owner of the temple that has the heart. Nothing for a number that is
/// not a heart of a temple of a player.
[[nodiscard]] std::optional<PlayerNames> HeartPlayer(const gamesave::VanillaWorld& world, uint32_t number);

/// The players whose temple stands whole in the file. A file with a site of a temple has a temple that is not whole, so
/// this gives no player for such a file.
[[nodiscard]] std::vector<PlayerNames> WholeTemplePlayers(const gamesave::VanillaWorld& world);

/// The running scripts with the values that hold objects changed from places of the table into entities
struct ImportedScripts
{
	lhvm::LHVMFile file;
	/// The values that hold an object
	uint32_t objects {0};
	/// The values whose place has no object in the table of openblack. They hold no object now.
	uint32_t lost {0};
};

/// Changes each value that holds an object in the globals, the main stack and each task (its values and its stack) from
/// a place of the table to the entity in that place, or to no object
[[nodiscard]] ImportedScripts ImportScripts(const lhvm::LHVMFile& file, const script_objects::Table& table);

} // namespace openblack::ecs::vanilla_script_import
