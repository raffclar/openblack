/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaWorldImport.h"

#include <cstring>

#include <algorithm>
#include <bit>
#include <ranges>

#include "VanillaWorld.h"

namespace openblack::ecs::vanilla_world_import
{
namespace
{
using gamesave::VanillaRecord;
using gamesave::VanillaType;
using gamesave::VanillaWorld;

/// The move states that the file gives a thing that does not walk: no state, arrived and the final step
constexpr std::array<uint8_t, 3> k_StillMoveStates = {0x00, 0x01, 0x04};

/// The record types of the buildings of a town that the import takes
constexpr std::array k_AbodeTypes = {VanillaType::Abode, VanillaType::Creche, VanillaType::Graveyard, VanillaType::StoragePit,
                                     VanillaType::TownCentre};

/// Each tribe has 16 rows of buildings in the same order. The import takes the six abodes, the storage pit, the creche,
/// the graveyard and the town centre.
constexpr int32_t k_TribeRows = 16;
constexpr int32_t k_Tribes = 9;
constexpr std::array<int32_t, 10> k_ImportedPlaces = {0, 1, 2, 3, 4, 5, 7, 8, 11, 12};

/// The record types of the villagers
constexpr std::array k_VillagerTypes = {VanillaType::Villager, VanillaType::SpecialVillager};

/// The map position of the record
std::optional<gamesave::MapCoords> PositionOf(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto bytes = world.Raw(record, "position");
	if (bytes.size() != sizeof(gamesave::MapCoords))
	{
		return std::nullopt;
	}
	gamesave::MapCoords coords;
	std::memcpy(&coords, bytes.data(), sizeof(coords));
	return coords;
}

int16_t SignedHalf(const VanillaWorld& world, const VanillaRecord& record, std::string_view name)
{
	return std::bit_cast<int16_t>(world.Half(record, name).value_or(0));
}

std::string Skip(const VanillaRecord& record, std::string_view why)
{
	return "object " + std::to_string(record.number) + " of type " + std::to_string(record.type) + ": " + std::string(why);
}

std::optional<ImportedTown> ReadTown(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto id = world.Word(record, "id");
	const auto tribe = world.Word(record, "tribe");
	const auto player = world.Byte(record, "player");
	if (!position.has_value() || !id.has_value() || !tribe.has_value() || !player.has_value())
	{
		why = "the town has no place, number, tribe or player";
		return std::nullopt;
	}
	if (static_cast<int32_t>(*tribe) < 0 || *tribe >= static_cast<uint32_t>(Tribe::_COUNT) ||
	    *player >= static_cast<uint8_t>(PlayerNames::_COUNT))
	{
		why = "the town has a tribe or a player that the game does not have";
		return std::nullopt;
	}
	ImportedTown town {.number = record.number,
	                   .id = *id,
	                   .tribe = static_cast<Tribe>(*tribe),
	                   .owner = static_cast<PlayerNames>(*player),
	                   .position = *position,
	                   .cannotBuildWorshipSite = world.Word(record, "cannotBuildWorshipSite").value_or(0) != 0,
	                   .uninhabitable = world.Word(record, "uninhabitable").value_or(0) != 0,
	                   .emptyTownTimer = world.Word(record, "emptyTownTimer").value_or(0)};
	const auto abodes = world.Refs(record, "abodes");
	town.abodes.assign(abodes.begin(), abodes.end());
	const auto homeless = world.Refs(record, "homeless");
	town.homeless.assign(homeless.begin(), homeless.end());
	return town;
}

std::optional<ImportedAbode> ReadAbode(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto row = AbodeRowOf(world.Word(record, "info").value_or(0));
	if (!position.has_value() || !row.has_value())
	{
		why = "the building has no place or no row of the table of buildings";
		return std::nullopt;
	}
	if (!IsImportedBuilding(*row))
	{
		why = "the import does not make buildings of the row " + std::to_string(static_cast<int32_t>(*row));
		return std::nullopt;
	}
	ImportedAbode abode {.number = record.number,
	                     .row = *row,
	                     .town = world.Ref(record, "town").value_or(0),
	                     .position = *position,
	                     .heading = world.Float(record, "heading").value_or(0.0f),
	                     .scale = world.Float(record, "scale").value_or(1.0f),
	                     .life = world.Float(record, "life").value_or(1.0f),
	                     .built = world.Float(record, "built").value_or(1.0f),
	                     .present = world.Byte(record, "present").value_or(0)};
	// The resources of the building: first its food, then its wood
	if (const auto* resources = world.Field(record, "resources"); resources != nullptr && resources->size == 4)
	{
		const auto bytes = world.Raw(record, "resources");
		std::array<uint32_t, 2> amounts {};
		for (size_t i = 0; i < amounts.size() && i < resources->count; ++i)
		{
			std::memcpy(&amounts.at(i), bytes.data() + i * sizeof(uint32_t), sizeof(uint32_t));
		}
		abode.food = amounts[0];
		abode.wood = amounts[1];
	}
	const auto villagers = world.Refs(record, "villagers");
	abode.villagers.assign(villagers.begin(), villagers.end());
	return abode;
}

std::optional<ImportedVillager> ReadVillager(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto row = VillagerRowOf(world.Word(record, "info").value_or(0));
	const auto action = world.Raw(record, "action");
	if (!position.has_value() || !row.has_value() || action.size() != 6)
	{
		why = "the villager has no place, no row of the table of villagers or no states";
		return std::nullopt;
	}
	ImportedVillager villager {.number = record.number,
	                           .row = *row,
	                           .position = *position,
	                           .heading = world.Float(record, "heading").value_or(0.0f),
	                           .scale = world.Float(record, "scale").value_or(1.0f),
	                           .life = world.Float(record, "life").value_or(1.0f),
	                           .made = world.Word(record, "created").value_or(0),
	                           .birthTurn = world.Word(record, "birthTurn").value_or(0),
	                           .food = world.Float(record, "food").value_or(0.0f),
	                           .lastCheckTurn = world.Word(record, "lastCheckTurn").value_or(0),
	                           .foodSpeedUp = world.Byte(record, "foodSpeedUp").value_or(0),
	                           .foodHeld = SignedHalf(world, record, "foodHeld"),
	                           .woodHeld = SignedHalf(world, record, "woodHeld"),
	                           .pregnancy = SignedHalf(world, record, "pregnancy"),
	                           .home = world.Ref(record, "home").value_or(0),
	                           .town = world.Ref(record, "town").value_or(0),
	                           .mother = world.Ref(record, "mother").value_or(0),
	                           .states = {action[0], action[1], action[2]},
	                           .moveState = world.Byte(record, "moveState").value_or(0),
	                           .turnsSinceStateChange = static_cast<uint16_t>(action[4] | (action[5] << 8)),
	                           .turnsUntilStateChange = world.Half(record, "turnsToStateChange").value_or(0)};
	if (const auto special = world.Word(record, "specialRow"); special.has_value())
	{
		villager.nameRow = static_cast<uint8_t>(*special);
	}
	return villager;
}

} // namespace

std::optional<AbodeInfo> AbodeRowOf(uint32_t number)
{
	const auto count = static_cast<uint32_t>(AbodeInfo::_COUNT);
	if (number > k_FirstAbodeRowNumber || number <= k_FirstAbodeRowNumber - count)
	{
		return std::nullopt;
	}
	return static_cast<AbodeInfo>(k_FirstAbodeRowNumber - number);
}

std::optional<VillagerInfo> VillagerRowOf(uint32_t number)
{
	const auto count = static_cast<uint32_t>(VillagerInfo::_COUNT);
	if (number > k_FirstVillagerRowNumber || number <= k_FirstVillagerRowNumber - count)
	{
		return std::nullopt;
	}
	return static_cast<VillagerInfo>(k_FirstVillagerRowNumber - number);
}

bool IsImportedBuilding(AbodeInfo row)
{
	const auto index = static_cast<int32_t>(row);
	if (index < 0 || index >= k_TribeRows * k_Tribes)
	{
		return false;
	}
	return std::ranges::find(k_ImportedPlaces, index % k_TribeRows) != k_ImportedPlaces.end();
}

bool IsInsideHome(VillagerStates state)
{
	return state == VillagerStates::AtHome || state == VillagerStates::GotoBedAtHome || state == VillagerStates::SleepingAtHome;
}

std::array<uint8_t, 3> StartStates(const std::array<uint8_t, 3>& states, uint8_t moveState)
{
	const bool still = std::ranges::find(k_StillMoveStates, moveState) != k_StillMoveStates.end();
	if (!still || gamesave::VillagerStateKeepsValues(states[0]) || gamesave::VillagerStateKeepsValues(states[1]))
	{
		return {static_cast<uint8_t>(VillagerStates::DecideWhatToDo), static_cast<uint8_t>(VillagerStates::InvalidState),
		        static_cast<uint8_t>(VillagerStates::InvalidState)};
	}
	return states;
}

ImportedTowns ReadTowns(const VanillaWorld& world)
{
	ImportedTowns imported;
	if (const auto data = world.GameData(); data.has_value())
	{
		imported.turn = data->gameTurn;
	}
	for (const auto& record : world.Records())
	{
		std::string why;
		if (record.Is(VanillaType::Town))
		{
			if (auto town = ReadTown(world, record, why); town.has_value())
			{
				imported.towns.push_back(*town);
				continue;
			}
		}
		else if (std::ranges::any_of(k_AbodeTypes, [&record](VanillaType type) { return record.Is(type); }))
		{
			if (auto abode = ReadAbode(world, record, why); abode.has_value())
			{
				imported.abodes.push_back(std::move(*abode));
				continue;
			}
		}
		else if (std::ranges::any_of(k_VillagerTypes, [&record](VanillaType type) { return record.Is(type); }))
		{
			if (auto villager = ReadVillager(world, record, why); villager.has_value())
			{
				imported.villagers.push_back(*villager);
				continue;
			}
		}
		else
		{
			continue;
		}
		imported.skipped.push_back(Skip(record, why));
	}
	return imported;
}

} // namespace openblack::ecs::vanilla_world_import
