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

/// The three states that a villager starts in after the import: the top state, the final state and the state before.
/// The import keeps the states of the file when the file keeps all that the states need. openblack does not read the
/// values of a state or of a walk yet. Thus a villager that walks, or that has a state with more values, decides again.
[[nodiscard]] std::array<uint8_t, 3> StartStates(const std::array<uint8_t, 3>& states, uint8_t moveState);

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
