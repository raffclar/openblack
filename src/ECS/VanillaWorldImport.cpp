/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaWorldImport.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <bit>
#include <limits>
#include <ranges>
#include <span>

#include "3D/MapCoords.h"
#include "ECS/VillagerWalkRules.h"
#include "ECS/WallHugRules.h"
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
/// The move state of a walk that steps straight through all that is in its way
constexpr uint8_t k_StepThroughMoveState = 0x0B;
/// The first and the last move state of a walk that keeps a circle: it goes straight, goes round a circle or goes out
/// of a circle
constexpr uint8_t k_FirstCircleMoveState = 0x0C;
constexpr uint8_t k_LastCircleMoveState = 0x12;
/// For a circle with no thing, the index of water or of the edge of the land
constexpr int32_t k_LandscapeIndex = 1;
/// The shift that changes whole map units to cells, and the metres along the side of a cell
constexpr int32_t k_CellShift = 16;
constexpr float k_CellMetres = 10.0f;

/// The states of a fisherman that keep the fish farm, and the states of a farmer that keep the spot and the field
constexpr std::array k_FishermanStates = {VillagerStates::FishermanArrivesAtFishing, VillagerStates::Fishing};
constexpr std::array k_FarmerStates = {VillagerStates::FarmerArrivesAtFarm, VillagerStates::FarmerPlantsCrop,
                                       VillagerStates::FarmerDigsUpCrop};
/// The number of floats on each side of a speed that WalkSpeedOf tries
constexpr int k_MetreTries = 4;

bool IsOneOf(uint8_t state, std::span<const VillagerStates> states)
{
	return std::ranges::find(states, static_cast<VillagerStates>(state)) != states.end();
}

/// The values of a state of the villager, in the order of the file
std::vector<ImportedStateValue> StateValues(const VanillaWorld& world, const VanillaRecord& record, std::string_view name)
{
	std::vector<ImportedStateValue> values;
	for (const auto* field : world.Fields(record, name))
	{
		if (field->kind == gamesave::VanillaValue::Reference)
		{
			values.push_back({.object = field->number, .bytes = {}});
		}
		else if (field->at + field->size <= world.Bytes().size())
		{
			const auto bytes = world.Bytes().subspan(field->at, field->size);
			values.push_back({.object = std::nullopt, .bytes = {bytes.begin(), bytes.end()}});
		}
	}
	return values;
}

ImportedWalk WalkOf(const VanillaWorld& world, const VanillaRecord& record)
{
	ImportedWalk walk {.speed = world.Half(record, "speed").value_or(0),
	                   .angle = world.Half(record, "angle").value_or(0),
	                   .turnsUntilStepRebuild = std::bit_cast<int8_t>(world.Byte(record, "turnsUntilStepRebuild").value_or(0))};
	if (const auto goal = world.Raw(record, "goal"); goal.size() == sizeof(gamesave::MapCoords))
	{
		std::memcpy(&walk.goal, goal.data(), sizeof(walk.goal));
	}
	// The step is the whole units along x, its height as a float, then the whole units along z
	if (const auto step = world.Raw(record, "step"); step.size() == 12)
	{
		std::memcpy(&walk.stepX, step.data(), 4);
		std::memcpy(&walk.stepAltitude, step.data() + 4, 4);
		std::memcpy(&walk.stepZ, step.data() + 8, 4);
	}
	if (const auto turns = world.Byte(record, "hugTurnsToObject"); turns.has_value())
	{
		ImportedCircle circle {.turnsToObject = *turns,
		                       .entryDistance = world.Word(record, "hugEntryDistance").value_or(0),
		                       .object = world.Ref(record, "hugObject").value_or(0),
		                       .index = std::bit_cast<int32_t>(world.Word(record, "hugIndex").value_or(0))};
		if (const auto coords = world.Raw(record, "hugCoords"); coords.size() == sizeof(gamesave::MapCoords))
		{
			std::memcpy(&circle.coords, coords.data(), sizeof(circle.coords));
		}
		walk.circle = circle;
	}
	return walk;
}

/// The record types of the buildings of a town that the import takes
constexpr std::array k_AbodeTypes = {VanillaType::Abode,      VanillaType::Creche, VanillaType::Graveyard,
                                     VanillaType::StoragePit, VanillaType::Wonder, VanillaType::TownCentre};

/// Each tribe has 16 rows of buildings in the same order. The import takes the six abodes, the storage pit, the creche,
/// the wonder, the graveyard and the town centre.
constexpr int32_t k_TribeRows = 16;
constexpr int32_t k_Tribes = 9;
constexpr std::array<int32_t, 11> k_ImportedPlaces = {0, 1, 2, 3, 4, 5, 7, 8, 10, 11, 12};

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

/// The belief of a town keeps the values for each player at these places, from the start of its bytes
constexpr size_t k_BeliefAt = 0x08;
constexpr size_t k_RecentAt = 0x28;
constexpr size_t k_CapAt = 0x68;
constexpr size_t k_GainedSinceShownAt = 0x88;
constexpr size_t k_PendingAt = 0xC8;
constexpr size_t k_BoredomAt = 0xE8;
constexpr size_t k_DesireLevelAt = 0x18C;
constexpr size_t k_BeliefSize = 464;
/// The statistics of a town keep the running counts of its food and wood at these places
constexpr size_t k_FoodUsedAt = 0xE8;
constexpr size_t k_WoodUsedAt = 0xEC;
constexpr size_t k_FoodCarriedAt = 0xF8;
constexpr size_t k_WoodCarriedAt = 0xFC;
constexpr size_t k_WoodAtSitesAt = 0x100;
constexpr size_t k_StatsSize = 280;

template <typename T, size_t N>
std::array<T, N> ValuesAt(std::span<const uint8_t> bytes, size_t at)
{
	std::array<T, N> values {};
	std::memcpy(values.data(), bytes.data() + at, sizeof(values));
	return values;
}

float FloatAt(std::span<const uint8_t> bytes, size_t at)
{
	return ValuesAt<float, 1>(bytes, at)[0];
}

/// The values of an array of the town that has a value for each desire
template <typename T>
std::optional<std::array<T, k_ImportedDesires>> DesireArray(const VanillaWorld& world, const VanillaRecord& record,
                                                            std::string_view name)
{
	const auto* field = world.Field(record, name);
	if (field == nullptr || field->count != k_ImportedDesires || field->size != sizeof(T) ||
	    field->at + sizeof(T) * k_ImportedDesires > world.Bytes().size())
	{
		return std::nullopt;
	}
	return ValuesAt<T, k_ImportedDesires>(world.Bytes(), field->at);
}

std::optional<ImportedBelief> ReadBelief(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto bytes = world.Raw(record, "belief");
	if (bytes.size() != k_BeliefSize)
	{
		return std::nullopt;
	}
	return ImportedBelief {.belief = ValuesAt<float, k_ImportedPlayers>(bytes, k_BeliefAt),
	                       .recent = ValuesAt<float, k_ImportedPlayers>(bytes, k_RecentAt),
	                       .cap = ValuesAt<float, k_ImportedPlayers>(bytes, k_CapAt),
	                       .pending = ValuesAt<float, k_ImportedPlayers>(bytes, k_PendingAt),
	                       .gainedSinceShown = ValuesAt<float, k_ImportedPlayers>(bytes, k_GainedSinceShownAt),
	                       .boredom = ValuesAt<float, k_ImportedReactions>(bytes, k_BoredomAt),
	                       .desireLevel = ValuesAt<float, k_ImportedDesires>(bytes, k_DesireLevelAt)};
}

std::optional<ImportedDesire> ReadDesire(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto cheat = DesireArray<float>(world, record, "desireCheat");
	const auto boost = DesireArray<float>(world, record, "desireBoost");
	const auto desire = DesireArray<float>(world, record, "desire");
	const auto raw = DesireArray<float>(world, record, "desireRaw");
	const auto sorted = DesireArray<ImportedDesireSort>(world, record, "desireSorted");
	const auto sortedRaw = DesireArray<ImportedDesireSort>(world, record, "desireSortedRaw");
	const auto turns = DesireArray<uint32_t>(world, record, "desireTurnsOverAlignment");
	const auto doingAtStart = DesireArray<float>(world, record, "desireDoingAtStart");
	const auto doingCountAtStart = DesireArray<float>(world, record, "desireDoingCountAtStart");
	const auto doing = DesireArray<float>(world, record, "desireDoing");
	const auto doingCount = DesireArray<float>(world, record, "desireDoingCount");
	if (!cheat || !boost || !desire || !raw || !sorted || !sortedRaw || !turns || !doingAtStart || !doingCountAtStart ||
	    !doing || !doingCount)
	{
		return std::nullopt;
	}
	return ImportedDesire {.cheat = *cheat,
	                       .boost = *boost,
	                       .desire = *desire,
	                       .population = world.Float(record, "desirePopulation").value_or(0.0f),
	                       .raw = *raw,
	                       .sorted = *sorted,
	                       .sortedRaw = *sortedRaw,
	                       .turnsOverAlignment = *turns,
	                       .doingAtStart = *doingAtStart,
	                       .doingCountAtStart = *doingCountAtStart,
	                       .doing = *doing,
	                       .doingCount = *doingCount};
}

std::optional<ImportedTally> ReadTally(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto bytes = world.Raw(record, "stats");
	if (bytes.size() != k_StatsSize)
	{
		return std::nullopt;
	}
	return ImportedTally {.foodUsed = FloatAt(bytes, k_FoodUsedAt),
	                      .woodUsed = FloatAt(bytes, k_WoodUsedAt),
	                      .foodCarried = FloatAt(bytes, k_FoodCarriedAt),
	                      .woodCarried = FloatAt(bytes, k_WoodCarriedAt),
	                      .woodAtSites = FloatAt(bytes, k_WoodAtSitesAt)};
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
	                   .emptyTownTimer = world.Word(record, "emptyTownTimer").value_or(0),
	                   .worshipShare = world.Float(record, "worshipShare").value_or(0.0f),
	                   .beliefInNeutral = world.Float(record, "beliefInNeutral").value_or(0.0f),
	                   .beliefScale = world.Float(record, "balanceBeliefScale").value_or(1.0f),
	                   .belief = ReadBelief(world, record),
	                   .desire = ReadDesire(world, record),
	                   .tally = ReadTally(world, record)};
	const auto abodes = world.Refs(record, "abodes");
	town.abodes.assign(abodes.begin(), abodes.end());
	const auto homeless = world.Refs(record, "homeless");
	town.homeless.assign(homeless.begin(), homeless.end());
	const auto planned = world.Refs(record, "planned");
	town.planned.assign(planned.begin(), planned.end());
	const auto sites = world.Refs(record, "buildingSites");
	town.sites.assign(sites.begin(), sites.end());
	return town;
}

/// The tribal powers of a player record. The file keeps a count before each list, and the import takes only lists of a
/// value for each tribe.
std::optional<ImportedTribalPower> ReadTribalPower(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto player = world.Byte(record, "number");
	const auto* power = world.Field(record, "tribalPower");
	const auto* most = world.Field(record, "maxTribalPower");
	ImportedTribalPower imported;
	if (!player.has_value() || *player >= static_cast<uint8_t>(PlayerNames::_COUNT) || power == nullptr || most == nullptr ||
	    power->size != sizeof(float) || most->size != sizeof(float) || power->count != imported.power.size() ||
	    most->count != imported.most.size())
	{
		return std::nullopt;
	}
	imported.player = static_cast<PlayerNames>(*player);
	const auto powerBytes = world.Raw(record, "tribalPower");
	const auto mostBytes = world.Raw(record, "maxTribalPower");
	if (powerBytes.size() != sizeof(imported.power) || mostBytes.size() != sizeof(imported.most))
	{
		return std::nullopt;
	}
	std::memcpy(imported.power.data(), powerBytes.data(), sizeof(imported.power));
	std::memcpy(imported.most.data(), mostBytes.data(), sizeof(imported.most));
	return imported;
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
	if (record.Is(VanillaType::Wonder))
	{
		abode.wonderPower = world.Float(record, "wonderPower");
	}
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
	villager.walk = WalkOf(world, record);
	villager.topValues = StateValues(world, record, "topStateValue");
	villager.finalValues = StateValues(world, record, "finalStateValue");
	if (const auto special = world.Word(record, "specialRow"); special.has_value())
	{
		villager.nameRow = static_cast<uint8_t>(*special);
	}
	return villager;
}

} // namespace

ImportedWalk ReadWalk(const VanillaWorld& world, const VanillaRecord& record)
{
	return WalkOf(world, record);
}

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

bool CanKeepStateValues(uint8_t state)
{
	return !gamesave::VillagerStateKeepsValues(state) || IsOneOf(state, k_FishermanStates) || IsOneOf(state, k_FarmerStates);
}

bool CanKeepWalk(uint8_t moveState)
{
	return std::ranges::find(k_StillMoveStates, moveState) != k_StillMoveStates.end() || moveState == k_StepThroughMoveState ||
	       (moveState >= k_FirstCircleMoveState && moveState <= k_LastCircleMoveState);
}

CircleKind KindOf(const ImportedCircle& circle)
{
	if (circle.object != 0)
	{
		return CircleKind::Thing;
	}
	return circle.index == k_LandscapeIndex ? CircleKind::Landscape : CircleKind::None;
}

std::optional<size_t> CircleAfterLoad(size_t circles)
{
	if (circles == 0)
	{
		return std::nullopt;
	}
	return circles - 1;
}

wall_hug::BlockingCircle LandscapeCircleOf(const gamesave::MapCoords& coords)
{
	const glm::ivec2 cell {coords.x >> k_CellShift, coords.z >> k_CellShift};
	return {.centre = (glm::vec2(cell) + 0.5f) * k_CellMetres,
	        .radius = wall_hug::k_LandscapeBlockerRadius,
	        .landscape = true,
	        .landscapeOrFence = true};
}

std::optional<ImportedWork> WorkOf(uint8_t state, const std::vector<ImportedStateValue>& values)
{
	if (IsOneOf(state, k_FishermanStates) && values.size() == 1 && values[0].object.has_value())
	{
		return ImportedWork {.target = *values[0].object, .spot = std::nullopt};
	}
	if (IsOneOf(state, k_FarmerStates) && values.size() == 2 && values[0].bytes.size() == sizeof(gamesave::MapCoords) &&
	    values[1].object.has_value())
	{
		gamesave::MapCoords spot;
		std::memcpy(&spot, values[0].bytes.data(), sizeof(spot));
		return ImportedWork {.target = *values[1].object, .spot = spot};
	}
	return std::nullopt;
}

float WalkSpeedOf(uint16_t wholeSpeed)
{
	float speed = static_cast<float>(wholeSpeed) * wall_hug::k_TurnsPerSecond / wall_hug::k_WholePerMetre;
	// The rounding of the float can give one unit more or less: the speed moves to the next float until it agrees
	for (int i = 0; i < k_MetreTries && wall_hug::WholeSpeed(speed) != wholeSpeed; ++i)
	{
		speed = std::nextafter(speed, wall_hug::WholeSpeed(speed) < wholeSpeed ? std::numeric_limits<float>::infinity()
		                                                                       : -std::numeric_limits<float>::infinity());
	}
	return speed;
}

std::array<uint8_t, 3> DecideAgain()
{
	return {static_cast<uint8_t>(VillagerStates::DecideWhatToDo), static_cast<uint8_t>(VillagerStates::InvalidState),
	        static_cast<uint8_t>(VillagerStates::InvalidState)};
}

std::array<uint8_t, 3> StartStates(const std::array<uint8_t, 3>& states, uint8_t moveState)
{
	if (!CanKeepWalk(moveState) || !CanKeepStateValues(states[0]) || !CanKeepStateValues(states[1]))
	{
		return DecideAgain();
	}
	return states;
}

std::optional<ImportedPlan> ReadPlan(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto row = AbodeRowOf(world.Word(record, "info").value_or(0));
	if (!position.has_value() || !row.has_value())
	{
		why = "the plan has no place or no row of the table of buildings";
		return std::nullopt;
	}
	return ImportedPlan {.number = record.number,
	                     .row = *row,
	                     .town = world.Ref(record, "town").value_or(0),
	                     .position = *position,
	                     .heading = world.Float(record, "heading").value_or(0.0f),
	                     .scale = world.Float(record, "scale").value_or(1.0f),
	                     .wasBuilt = world.Word(record, "wasBuilt").value_or(0) != 0};
}

// The file keeps each place of a builder as three floats
static_assert(sizeof(glm::vec3) == 3 * sizeof(float));

std::optional<ImportedSite> ReadSite(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto places = world.Raw(record, "sitePlaces");
	const auto building = world.Ref(record, "siteBuilding").value_or(0);
	if (places.size() != k_ImportedSitePlaces * sizeof(glm::vec3) || building == 0)
	{
		why = "the building site has no building or no places for its builders";
		return std::nullopt;
	}
	ImportedSite site {.number = record.number,
	                   .building = building,
	                   .woodPile = world.Ref(record, "siteWoodPile").value_or(0),
	                   .workerCount = std::bit_cast<int32_t>(world.Word(record, "siteWorkerCount").value_or(0)),
	                   .desire = world.Float(record, "siteDesire").value_or(0.0f)};
	std::memcpy(site.places.data(), places.data(), places.size());
	const auto workers = world.Refs(record, "siteWorkers");
	site.workers.assign(workers.begin(), workers.end());
	return site;
}

ImportedTowns ReadTowns(const VanillaWorld& world)
{
	ImportedTowns imported;
	if (const auto data = world.GameData(); data.has_value())
	{
		imported.turn = data->gameTurn;
	}
	if (!world.Records().empty() && world.Game().Is(VanillaType::Game))
	{
		if (const auto lists = world.Fields(world.Game(), "lists"); lists.size() == k_GameObjectLists)
		{
			const auto towns = world.Refs(*lists.at(k_GameTownList));
			imported.gameTownList.assign(towns.begin(), towns.end());
		}
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
		else if (record.Is(VanillaType::PlannedAbode) || record.Is(VanillaType::PlannedTownCentre))
		{
			if (auto plan = ReadPlan(world, record, why); plan.has_value())
			{
				imported.plans.push_back(*plan);
				continue;
			}
		}
		else if (record.Is(VanillaType::StandardBuildingSite))
		{
			if (auto site = ReadSite(world, record, why); site.has_value())
			{
				imported.sites.push_back(std::move(*site));
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
		else if (record.Is(VanillaType::TownDesireFlags))
		{
			const auto town = world.Ref(record, "flagTown");
			const auto desire = world.Word(record, "flagType");
			const auto level = world.Float(record, "flagLevel");
			if (town.has_value() && desire.has_value() && level.has_value() && *desire < k_ImportedDesires)
			{
				imported.desireFlags.push_back({.town = *town, .desire = *desire, .level = *level});
				continue;
			}
			why = "the flag of a desire has no town, no desire of the table or no level";
		}
		else if (record.Is(VanillaType::Player))
		{
			// The player's list of towns, in the order that the player got them
			const auto towns = world.Refs(record, "towns");
			imported.townLists.insert(imported.townLists.end(), towns.begin(), towns.end());
			if (auto powers = ReadTribalPower(world, record); powers.has_value())
			{
				imported.tribalPowers.push_back(*powers);
			}
			continue;
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
