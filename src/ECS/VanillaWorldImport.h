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
#include <string>
#include <vector>

#include "CreatureRecord.h"
#include "Enums.h"

namespace openblack::gamesave
{
class VanillaWorld;
} // namespace openblack::gamesave

// The towns, the buildings of the towns and the villagers of a world file of the game, as values that openblack can
// make again. The rules here use no service of the game, so that tests can give them a world that they make.
namespace openblack::ecs::vanilla_world_import
{

/// The game gives each row of its tables a number, in the order that it loads the rows. The last row that it loads
/// has the number 0, and each row before it has the number of the next row plus 1. The rows of the buildings come
/// first, and the rows of the villagers come next.
constexpr uint32_t k_FirstAbodeRowNumber = 2394;
constexpr uint32_t k_FirstVillagerRowNumber = k_FirstAbodeRowNumber - static_cast<uint32_t>(AbodeInfo::_COUNT);

/// The building row that has the number, or nothing for a number of another table
[[nodiscard]] std::optional<AbodeInfo> AbodeRowOf(uint32_t number);
/// The villager row that has the number, or nothing for a number of another table
[[nodiscard]] std::optional<VillagerInfo> VillagerRowOf(uint32_t number);

/// True when the import makes buildings of the row from the file: the abodes, the storage pit, the creche, the
/// graveyard and the town centre of each tribe. The import removes the buildings of these rows that the land made.
[[nodiscard]] bool IsImportedBuilding(AbodeInfo row);

/// True when a villager in the state is in its abode: the game does not show it, and its abode counts it as present
[[nodiscard]] bool IsInsideHome(VillagerStates state);

/// True when the import can keep the values that the file keeps for the state: a state with no values, or a state of
/// a fisherman or of a farmer
[[nodiscard]] bool CanKeepStateValues(uint8_t state);

/// True when the import can keep a walk in the move state of the file. It keeps a walk that stands still, takes its
/// last step, steps straight through, or goes straight with no circle ahead. It does not keep a walk round a circle.
[[nodiscard]] bool CanKeepWalk(uint8_t moveState, bool hugsCircle);

/// The states of a villager that decides again: it decides what to do, with no final state and no state before
[[nodiscard]] std::array<uint8_t, 3> DecideAgain();

/// The three states that a villager starts in after the import: the top state, the final state and the state before.
/// The import keeps the states of the file when it can keep the walk and the values of the top and final states.
/// Otherwise the villager decides again.
[[nodiscard]] std::array<uint8_t, 3> StartStates(const std::array<uint8_t, 3>& states, uint8_t moveState,
                                                 bool hugsCircle = false);

/// One value that a villager keeps in the file for one of its states: the number of an object, or bytes
struct ImportedStateValue
{
	std::optional<uint32_t> object;
	std::vector<uint8_t> bytes;
};

/// The work that the values of a state give: the field or the fish farm, and the spot in the field
struct ImportedWork
{
	/// The number of the object of the field or the fish farm in the file
	uint32_t target {0};
	/// The spot in the field that a farmer goes to next
	std::optional<gamesave::MapCoords> spot;
};

/// The work of a state of a fisherman or of a farmer, from the values of the file. Nothing for another state.
[[nodiscard]] std::optional<ImportedWork> WorkOf(uint8_t state, const std::vector<ImportedStateValue>& values);

/// The walk of a villager in the file
struct ImportedWalk
{
	/// Where the walk goes to
	gamesave::MapCoords goal {};
	/// The step of each turn, in whole map units, and its height
	int32_t stepX {0};
	int32_t stepZ {0};
	float stepAltitude {0.0f};
	/// Whole map units in a turn
	uint16_t speed {0};
	/// The way the walker faces, in game angles (2048 to the circle)
	uint16_t angle {0};
	/// Turns until a walker that steps straight through aims at its goal again
	int8_t turnsUntilStepRebuild {0};
	/// It heads for or goes round the circle of a thing, of water or of the edge of the land
	bool hugsCircle {false};
};

/// True when the villager walks in its top state, or goes back to the walk in the state before
[[nodiscard]] bool UsesWalkGoal(const std::array<uint8_t, 3>& states);

/// A metre value that gives the whole map units again, as openblack changes metres to whole units. Nothing when no
/// float near the value does.
[[nodiscard]] std::optional<float> MetresOfWhole(int32_t whole);

/// A speed in metres a second that gives the whole map units a turn of the file again
[[nodiscard]] float WalkSpeedOf(uint16_t wholeSpeed);

struct ImportedTown
{
	/// The number of the object in the file
	uint32_t number {0};
	/// The number that the land's script gives the town
	uint32_t id {0};
	Tribe tribe {Tribe::NONE};
	PlayerNames owner {PlayerNames::NEUTRAL};
	gamesave::MapCoords position {};
	bool cannotBuildWorshipSite {false};
	bool uninhabitable {false};
	uint32_t emptyTownTimer {0};
	/// The numbers of the objects of its buildings and of its homeless villagers, in the order of the file
	std::vector<uint32_t> abodes;
	std::vector<uint32_t> homeless;
};

struct ImportedAbode
{
	uint32_t number {0};
	AbodeInfo row {AbodeInfo::None};
	/// The number of the object of its town in the file, 0 for no town
	uint32_t town {0};
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	/// How much of the building stands, from 0 to 1
	float built {1.0f};
	uint32_t food {0};
	uint32_t wood {0};
	/// The numbers of the objects of its villagers in the file
	std::vector<uint32_t> villagers;
	/// The number of its villagers that are in it
	uint8_t present {0};
};

struct ImportedVillager
{
	uint32_t number {0};
	VillagerInfo row {VillagerInfo::None};
	/// The row of its name in the table of named villagers, for a named villager
	std::optional<uint8_t> nameRow;
	gamesave::MapCoords position {};
	float heading {0.0f};
	float scale {1.0f};
	float life {1.0f};
	/// The place of the villager in the order of all things that the game made
	uint32_t made {0};
	uint32_t birthTurn {0};
	float food {0.0f};
	uint32_t lastCheckTurn {0};
	uint8_t foodSpeedUp {0};
	int16_t foodHeld {0};
	int16_t woodHeld {0};
	int16_t pregnancy {0};
	/// The numbers of the objects of its abode, its town and its mother in the file, 0 for none
	uint32_t home {0};
	uint32_t town {0};
	uint32_t mother {0};
	/// The three states of the file: the top state, the final state and the state before
	std::array<uint8_t, 3> states {};
	uint8_t moveState {0};
	ImportedWalk walk;
	/// The values of the top state and of the final state
	std::vector<ImportedStateValue> topValues;
	std::vector<ImportedStateValue> finalValues;
	uint16_t turnsSinceStateChange {0};
	uint16_t turnsUntilStateChange {0};
};

/// The towns, buildings and villagers of a world file, in the order of the file
struct ImportedTowns
{
	uint32_t turn {0};
	std::vector<ImportedTown> towns;
	std::vector<ImportedAbode> abodes;
	std::vector<ImportedVillager> villagers;
	/// Each record that the import does not take, and why
	std::vector<std::string> skipped;
};

/// The towns, the buildings of the towns and the villagers of the world. A record that has a row of another table, or
/// no value that the import needs, goes into the list of records that the import does not take.
[[nodiscard]] ImportedTowns ReadTowns(const gamesave::VanillaWorld& world);

} // namespace openblack::ecs::vanilla_world_import
