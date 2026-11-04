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
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <unordered_map>
#include <unordered_set>

#include "3D/MapCoords.h"
#include "DanceFile.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/VanillaSceneryImport.h"
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

/// The reaction of the file that a living thing reacts to, 0 for none or for a record that is not a reaction
uint32_t ReactionOf(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto number = world.Ref(record, "reaction").value_or(0);
	const auto* reaction = number != 0 ? world.Record(number) : nullptr;
	return reaction != nullptr && reaction->Is(VanillaType::Reaction) ? number : 0;
}

/// The flag of a pot whose food is poisoned
constexpr uint8_t k_PotPoisonedFlag = 0x01;

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
/// The script states: each keeps the place, the number and the pair of whole numbers that the script last gave
constexpr std::array k_ScriptValueStates = {VillagerStates::InScript, VillagerStates::ScriptWanderAroundPosition,
                                            VillagerStates::ScriptPlayAnim, VillagerStates::ScriptInCrowd};
/// The states of a shepherd: each keeps the countdown and the state after a wait
constexpr std::array k_ShepherdStates = {VillagerStates::ShepherdLookForFlock,
                                         VillagerStates::ShepherdMoveFlockToWater,
                                         VillagerStates::ShepherdMoveFlockToFood,
                                         VillagerStates::ShepherdMoveFlockBack,
                                         VillagerStates::ShepherdDecideWhatToDoWithFlock,
                                         VillagerStates::ShepherdWaitForFlock,
                                         VillagerStates::ShepherdSlaughterAnimal,
                                         VillagerStates::ShepherdFetchStray,
                                         VillagerStates::ShepherdGotoFlock,
                                         VillagerStates::ShepherdCheckAnimalForSlaughter};
/// The states of a dance that the import keeps for a dancer of a group that it makes: the dance of a worship site, of a
/// script or of a creature. The two values that some of these states keep are left over from other states: no part of a
/// dance reads them.
constexpr std::array k_DanceStates = {VillagerStates::InDance, VillagerStates::MoveToDancePos,
                                      VillagerStates::WorshippingCreature, VillagerStates::RestartWorshippingCreature,
                                      VillagerStates::ControlledByCreature};
/// The states of the dance of an artefact of a town. They keep the same values as the other dance states.
constexpr std::array k_ArtefactDanceStates = {VillagerStates::ArtifactDance, VillagerStates::WaitForArtifactDance};
/// The state of the dance of a reaction. A villager keeps it only when it dances in a group of a dance that the import
/// makes, and it reacts to a reaction of the file.
constexpr std::array k_ReactionDanceStates = {VillagerStates::DanceWhileReacting};
/// The states of worship at a worship site. Each keeps one value: whether the villager asked to go home. Only code
/// that the game never runs sets it, and no villager asks to go home, so the value changes nothing.
constexpr std::array k_WorshipStates = {VillagerStates::WorshippingAtWorshipSite, VillagerStates::HidingAtWorshipSite,
                                        VillagerStates::GetFoodAtWorshipSite};
/// The flags of a villager that is a disciple, and of a villager that follows a disciple one time
constexpr uint16_t k_DiscipleFlag = 0x0200;
constexpr uint16_t k_FollowerFlag = 0x0400;
/// The bits of the living status that keep the pose that the living thing landed in, and the pose on its back
constexpr uint16_t k_LandingPoseShift = 4;
constexpr uint16_t k_LandingPoseMask = 0x3;
constexpr uint16_t k_OnBackPose = 2;
/// The flag of the status of a living thing that walks a footpath towards its first node
constexpr uint16_t k_TowardsStartStatus = 0x0008;
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
                                     VanillaType::StoragePit, VanillaType::Wonder, VanillaType::TownCentre,
                                     VanillaType::Workshop};

/// Each tribe has 16 rows of buildings in the same order. The import takes the six abodes, the storage pit, the creche,
/// the workshop, the wonder, the graveyard and the town centre.
constexpr int32_t k_TribeRows = 16;
constexpr int32_t k_Tribes = 9;
constexpr std::array<int32_t, 12> k_ImportedPlaces = {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12};

/// The word that the file keeps for a scaffold that no script gave a building
constexpr uint32_t k_NoScriptBuilding = 0xFFFFFFFFU;

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

/// A record of the file with the number and the type, or none
const VanillaRecord* RecordOf(const VanillaWorld& world, uint32_t number, VanillaType type)
{
	const auto* record = number != 0 ? world.Record(number) : nullptr;
	return record != nullptr && record->Is(type) ? record : nullptr;
}

/// The player of a player record of the file, none for no player
std::optional<PlayerNames> PlayerOfRecord(const VanillaWorld& world, uint32_t number)
{
	const auto* record = RecordOf(world, number, VanillaType::Player);
	const auto player = record != nullptr ? world.Byte(*record, "number") : std::nullopt;
	if (!player.has_value() || *player >= static_cast<uint8_t>(PlayerNames::_COUNT))
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(*player);
}

std::optional<ImportedTown> ReadTown(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto id = world.Word(record, "id");
	const auto tribe = world.Word(record, "tribe");
	// The owner reference gives the player of the town. The town also keeps a player number: the first player of the
	// town, or the player before the last change of owner. The game does not use that number as the owner.
	const auto player = PlayerOfRecord(world, world.Ref(record, "owner").value_or(0));
	if (!position.has_value() || !id.has_value() || !tribe.has_value() || !player.has_value())
	{
		why = "the town has no place, number, tribe or player";
		return std::nullopt;
	}
	if (static_cast<int32_t>(*tribe) < 0 || *tribe >= static_cast<uint32_t>(Tribe::_COUNT))
	{
		why = "the town has a tribe that the game does not have";
		return std::nullopt;
	}
	ImportedTown town {.number = record.number,
	                   .id = *id,
	                   .tribe = static_cast<Tribe>(*tribe),
	                   .owner = *player,
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
	if (record.Is(VanillaType::Workshop))
	{
		ImportedWorkshop workshop {.makingTurns = std::bit_cast<int32_t>(world.Word(record, "workshopMakingTurns").value_or(0)),
		                           .counted = world.Word(record, "workshopCounted").value_or(0),
		                           .woodPile = world.Ref(record, "workshopWoodPile").value_or(0)};
		const auto places = world.Raw(record, "workshopPlaces");
		std::copy_n(places.begin(), std::min(places.size(), workshop.places.size()), workshop.places.begin());
		const auto scaffolds = world.Refs(record, "workshopScaffolds");
		workshop.scaffolds.assign(scaffolds.begin(), scaffolds.end());
		abode.workshop = std::move(workshop);
	}
	return abode;
}

/// The building that a plan of the file offers, nothing for no plan or a plan with no row of the table of buildings
std::optional<ImportedScaffoldOffer> OfferOf(const VanillaWorld& world, uint32_t number)
{
	const auto* record = number != 0 ? world.Record(number) : nullptr;
	if (record == nullptr)
	{
		return std::nullopt;
	}
	const auto row = AbodeRowOf(world.Word(*record, "info").value_or(0));
	if (!row.has_value())
	{
		return std::nullopt;
	}
	return ImportedScaffoldOffer {.row = *row,
	                              .heading = world.Float(*record, "heading").value_or(0.0f),
	                              .scale = world.Float(*record, "scale").value_or(1.0f)};
}

/// The player whose interface the record with the number is, nothing for none
std::optional<PlayerNames> PlayerOfInterface(const VanillaWorld& world, uint32_t number)
{
	if (number == 0)
	{
		return std::nullopt;
	}
	for (const auto& record : world.Records())
	{
		if (!record.Is(VanillaType::Player))
		{
			continue;
		}
		const auto interfaces = world.Refs(record, "interfaces");
		if (std::ranges::find(interfaces, number) == interfaces.end())
		{
			continue;
		}
		const auto player = world.Byte(record, "number");
		if (player.has_value() && *player < static_cast<uint8_t>(PlayerNames::_COUNT))
		{
			return static_cast<PlayerNames>(*player);
		}
	}
	return std::nullopt;
}

/// The player whose hand the interface status or the interface with the number is, nothing for none
std::optional<PlayerNames> PlayerOfHand(const VanillaWorld& world, uint32_t number)
{
	const auto* record = number != 0 ? world.Record(number) : nullptr;
	if (record != nullptr && record->Is(VanillaType::PlayerInterfaceStatus))
	{
		return PlayerOfInterface(world, world.Ref(*record, "interface").value_or(0));
	}
	return PlayerOfInterface(world, number);
}

/// The first word of an array of words of the record, or nothing for an empty array
std::optional<uint32_t> FirstWordOf(const VanillaWorld& world, const VanillaRecord& record, std::string_view name)
{
	const auto* field = world.Field(record, name);
	const auto bytes = world.Raw(record, name);
	if (field == nullptr || field->size != sizeof(uint32_t) || field->count == 0 || bytes.size() < sizeof(uint32_t))
	{
		return std::nullopt;
	}
	uint32_t word = 0;
	std::memcpy(&word, bytes.data(), sizeof(word));
	return word;
}

/// A worship site of the file with its prayer power, its needs, its altar and the food in its pot
std::optional<ImportedWorshipSite> ReadWorshipSite(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto citadel = world.Ref(record, "partCitadel");
	const auto tribe = TribeRowOf(world.Word(record, "siteTribe").value_or(0));
	const auto place = world.Byte(record, "sitePlace");
	if (!citadel.has_value() || *citadel == 0 || !tribe.has_value() || !place.has_value())
	{
		why = "the worship site has no temple, no tribe of the table or no place";
		return std::nullopt;
	}
	ImportedWorshipSite site {
	    .number = record.number,
	    .citadel = *citadel,
	    .tribe = *tribe,
	    .place = *place,
	    .built = world.Float(record, "built").value_or(0.0f),
	    .dance = world.Ref(record, "siteDance").value_or(0),
	    .foodPot = world.Ref(record, "siteFoodPot").value_or(0),
	    .worshipCount = world.Word(record, "siteWorshipCount").value_or(0),
	    .foodOnTheWay = std::bit_cast<int32_t>(FirstWordOf(world, record, "siteFoodOnTheWay").value_or(0)),
	    .chants = {.battery = world.Float(record, "siteBattery").value_or(0.0f),
	               .sacrificed = world.Float(record, "siteSacrificed").value_or(0.0f),
	               .available = world.Float(record, "siteAvailable").value_or(0.0f),
	               .used = world.Float(record, "siteUsed").value_or(0.0f),
	               .requested = world.Float(record, "siteRequested").value_or(0.0f),
	               .chantsPerDancer = world.Float(record, "siteChantsPerDancer").value_or(0.0f),
	               .strain = world.Float(record, "siteStrain").value_or(0.0f),
	               .infinite = world.Word(record, "siteInfinite").value_or(0) != 0,
	               .freeMaintenance = world.Word(record, "siteFreeMaintenance").value_or(0) != 0},
	    .footpathLink = world.Ref(record, "footpathLink").value_or(0),
	    .goHome = static_cast<uint32_t>(world.Refs(record, "siteGoHome").size()),
	};
	const auto towns = world.Refs(record, "siteTowns");
	site.towns.assign(towns.begin(), towns.end());
	const auto icons = world.Refs(record, "siteIcons");
	site.icons.assign(icons.begin(), icons.end());
	for (const auto number : site.icons)
	{
		if (const auto* icon = number != 0 ? world.Record(number) : nullptr; icon != nullptr)
		{
			if (auto values = ReadWorshipIcon(world, *icon); values.has_value())
			{
				site.iconValues.push_back(std::move(*values));
			}
		}
	}
	if (const auto* dance = site.dance != 0 ? world.Record(site.dance) : nullptr; dance != nullptr)
	{
		site.danceValues = ReadDance(world, *dance);
	}
	const auto worshippers = world.Refs(record, "siteWorshippers");
	site.worshippers.assign(worshippers.begin(), worshippers.end());
	// The pot is a pot of any kind that keeps an amount
	if (const auto* pot = site.foodPot != 0 ? world.Record(site.foodPot) : nullptr; pot != nullptr)
	{
		site.food = world.Word(*pot, "potAmount");
	}
	// The needs display keeps the three flags in the order food, rest and hiding
	if (const auto* needs = RecordOf(world, world.Ref(record, "siteNeeds").value_or(0), VanillaType::ShowNeeds);
	    needs != nullptr)
	{
		const auto flags = world.Refs(*needs, "needFlags");
		std::array<float, k_SiteNeedFlags> values {};
		bool whole = flags.size() == k_SiteNeedFlags;
		for (size_t i = 0; whole && i < k_SiteNeedFlags; ++i)
		{
			const auto* flag = RecordOf(world, flags[i], VanillaType::ShowNeedsVisuals);
			const auto need = flag != nullptr ? world.Float(*flag, "needFlagNeed") : std::nullopt;
			whole = need.has_value();
			values.at(i) = need.value_or(0.0f);
		}
		if (whole)
		{
			site.needs = values;
		}
	}
	const auto altarNumber = world.Ref(record, "siteAltar").value_or(0);
	if (const auto* altar = RecordOf(world, altarNumber, VanillaType::WorshipTotem); altar != nullptr)
	{
		site.altar = ImportedAltar {.number = altarNumber,
		                            .built = world.Float(*altar, "built").value_or(0.0f),
		                            .life = world.Float(*altar, "life").value_or(1.0f)};
	}
	return site;
}

std::optional<ImportedScaffold> ReadScaffold(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	if (!position.has_value())
	{
		why = "the scaffold has no place";
		return std::nullopt;
	}
	ImportedScaffold scaffold {.number = record.number,
	                           .position = *position,
	                           .heading = world.Float(record, "heading").value_or(0.0f),
	                           .scale = world.Float(record, "scale").value_or(1.0f),
	                           .town = world.Ref(record, "scaffoldTown").value_or(0),
	                           .site = world.Ref(record, "scaffoldSite").value_or(0),
	                           .offer = OfferOf(world, world.Ref(record, "scaffoldOffer").value_or(0)),
	                           .nextOffer = OfferOf(world, world.Ref(record, "scaffoldNextOffer").value_or(0)),
	                           .changeMs = std::bit_cast<int32_t>(world.Word(record, "scaffoldChangeMs").value_or(0)),
	                           .handPlayer = PlayerOfInterface(world, world.Ref(record, "scaffoldHolder").value_or(0)),
	                           .phantomAngle = world.Float(record, "scaffoldAngle").value_or(0.0f),
	                           .phantomScale = world.Float(record, "scaffoldScale").value_or(0.0f),
	                           .flags = ScaffoldFlagsOf(world.Half(record, "scaffoldFlags").value_or(0))};
	const auto building = world.Word(record, "scaffoldScriptBuilding").value_or(k_NoScriptBuilding);
	if (building != k_NoScriptBuilding && building < static_cast<uint32_t>(AbodeNumber::_COUNT))
	{
		scaffold.scriptBuilding = static_cast<AbodeNumber>(building);
	}
	return scaffold;
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
	                           .discipleType = world.Byte(record, "discipleType").value_or(0),
	                           .villagerFlags = world.Half(record, "villagerFlags").value_or(0),
	                           .status = world.Half(record, "status").value_or(0),
	                           .turnsSinceStateChange = static_cast<uint16_t>(action[4] | (action[5] << 8)),
	                           .turnsUntilStateChange = world.Half(record, "turnsToStateChange").value_or(0),
	                           .danceGroup = world.Ref(record, "danceGroup").value_or(0),
	                           .reaction = ReactionOf(world, record)};
	villager.walk = WalkOf(world, record);
	villager.footpathWalk = {.footpath = world.Ref(record, "walkFootpath").value_or(0),
	                         .node = world.Ref(record, "walkFootpathNode").value_or(0),
	                         .towardsStart = (villager.status & k_TowardsStartStatus) != 0,
	                         .next = world.Ref(record, "nextFootpath").value_or(0)};
	villager.topValues = StateValues(world, record, "topStateValue");
	villager.finalValues = StateValues(world, record, "finalStateValue");
	if (const auto special = world.Word(record, "specialRow"); special.has_value())
	{
		villager.nameRow = static_cast<uint8_t>(*special);
	}
	return villager;
}

/// A pile of food or of wood of the file, nothing for another thing or a pile without all its values
std::optional<ImportedPile> ReadPile(const VanillaWorld& world, const VanillaRecord& record)
{
	if (!record.Is(VanillaType::PileFood) && !record.Is(VanillaType::PileResource) && !record.Is(VanillaType::MagicWood))
	{
		return std::nullopt;
	}
	const auto row = PotRowOf(world.Word(record, "info").value_or(0));
	const auto position = PositionOf(world, record);
	const auto rise = world.Raw(record, "pileRise");
	const auto amount = world.Word(record, "potAmount");
	if (!row.has_value() || !position.has_value() || rise.size() != k_PileRiseBytes || !amount.has_value())
	{
		return std::nullopt;
	}
	return ImportedPile {.number = record.number,
	                     .row = *row,
	                     .position = *position,
	                     .heading = world.Float(record, "heading").value_or(0.0f),
	                     .amount = *amount,
	                     .poisoned = (world.Byte(record, "potFlags").value_or(0) & k_PotPoisonedFlag) != 0,
	                     .rise = PileRiseOf(rise.first<k_PileRiseBytes>())};
}

/// The pile of the file with the number, nothing for no pile
std::optional<ImportedPile> PileNumbered(const VanillaWorld& world, uint32_t number)
{
	const auto* record = number != 0 ? world.Record(number) : nullptr;
	return record != nullptr ? ReadPile(world, *record) : std::nullopt;
}

/// Gives each storage pit of the file its piles (the piles whose building is the storage pit), and each building site
/// the values of its piles
void PutPiles(const VanillaWorld& world, ImportedTowns& imported)
{
	std::unordered_map<uint32_t, ImportedAbode*> pits;
	for (auto& abode : imported.abodes)
	{
		pits.emplace(abode.number, &abode);
	}
	for (const auto& record : world.Records())
	{
		const auto building = pits.find(world.Ref(record, "potBuilding").value_or(0));
		if (building == pits.end())
		{
			continue;
		}
		if (auto pile = ReadPile(world, record); pile.has_value())
		{
			building->second->piles.push_back(*pile);
		}
	}
	// A workshop keeps the number of its wood pile
	for (auto& abode : imported.abodes)
	{
		if (!abode.workshop.has_value() || abode.workshop->woodPile == 0 ||
		    std::ranges::any_of(abode.piles,
		                        [&abode](const ImportedPile& pile) { return pile.number == abode.workshop->woodPile; }))
		{
			continue;
		}
		if (auto pile = PileNumbered(world, abode.workshop->woodPile); pile.has_value())
		{
			abode.piles.push_back(*pile);
		}
	}
	for (auto& site : imported.sites)
	{
		site.woodPileValues = PileNumbered(world, site.woodPile);
		site.pileValues.clear();
		for (const auto number : site.piles)
		{
			site.pileValues.push_back(PileNumbered(world, number));
		}
	}
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

std::optional<Tribe> TribeRowOf(uint32_t number)
{
	const auto count = static_cast<uint32_t>(Tribe::_COUNT);
	if (number > k_FirstTribeRowNumber || number <= k_FirstTribeRowNumber - count)
	{
		return std::nullopt;
	}
	return static_cast<Tribe>(k_FirstTribeRowNumber - number);
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

std::optional<SpellSeedType> SpellSeedRowOf(uint32_t number)
{
	const auto count = static_cast<uint32_t>(SpellSeedType::BeamExplosion) + 1;
	if (number > k_FirstSpellSeedRowNumber || number <= k_FirstSpellSeedRowNumber - count)
	{
		return std::nullopt;
	}
	return static_cast<SpellSeedType>(k_FirstSpellSeedRowNumber - number);
}

namespace
{
/// The words of a value of bytes or of an array, in the order of the file
std::vector<uint32_t> WordsOf(std::span<const uint8_t> bytes)
{
	std::vector<uint32_t> words(bytes.size() / sizeof(uint32_t));
	std::memcpy(words.data(), bytes.data(), words.size() * sizeof(uint32_t));
	return words;
}

/// A text of the file that ends at its first zero byte
std::string TextOf(std::span<const uint8_t> bytes)
{
	const auto end = std::ranges::find(bytes, uint8_t {0});
	return {bytes.begin(), end};
}

std::optional<ImportedDanceGroup> ReadDanceGroup(const VanillaWorld& world, uint32_t number)
{
	const auto* record = number != 0 ? world.Record(number) : nullptr;
	if (record == nullptr || !record->Is(VanillaType::DanceGroup))
	{
		return std::nullopt;
	}
	ImportedDanceGroup group {
	    .number = number,
	    .id = world.Word(*record, "groupId").value_or(0),
	    .name = TextOf(world.Raw(*record, "groupName")),
	    .dancers = {},
	    .limited = false,
	    .quota = 100,
	    .weight = 100,
	    .limitedDancers = world.Word(*record, "groupLimitedDancers").value_or(0),
	    .danceType = world.Word(*record, "groupDanceType").value_or(0),
	    .sexes = world.Word(*record, "groupSexes").value_or(3),
	    .formation = world.Word(*record, "groupFormation").value_or(0),
	    .inFormation = world.Word(*record, "groupInFormation").value_or(0),
	    .flag = world.Word(*record, "groupFlag").value_or(1) != 0,
	    .shape = world.Word(*record, "groupShape").value_or(0),
	    .radius = world.Float(*record, "groupRadius").value_or(20.0f),
	    .offset = {0.0f, 0.0f},
	    .rotation = world.Float(*record, "groupRotation").value_or(0.0f),
	    .spin = world.Float(*record, "groupSpin").value_or(0.0f),
	    .move = {},
	    .moved = world.Word(*record, "groupMoved").value_or(0) != 0,
	    .parent = world.Ref(*record, "groupParent").value_or(0),
	    .indexInParent = world.Byte(*record, "groupIndexInParent").value_or(0),
	    .children = WordsOf(world.Raw(*record, "groupChildren")),
	};
	const auto dancers = world.Refs(*record, "groupDancers");
	group.dancers.assign(dancers.begin(), dancers.end());
	if (const auto offset = world.Raw(*record, "groupOffset"); offset.size() == sizeof(float) * 2)
	{
		std::memcpy(&group.offset.x, offset.data(), sizeof(float));
		std::memcpy(&group.offset.y, offset.data() + sizeof(float), sizeof(float));
	}
	if (const auto membership = WordsOf(world.Raw(*record, "groupMembership")); membership.size() == 3)
	{
		group.limited = membership[0] != 0;
		group.quota = membership[1];
		group.weight = membership[2];
	}
	if (const auto move = WordsOf(world.Raw(*record, "groupMove")); move.size() == 4)
	{
		group.move = {.action = move[0], .first = move[1], .second = move[2], .end = move[3]};
	}
	return group;
}

std::vector<ImportedDanceKeyFrame> ReadDanceKeyFrames(const VanillaWorld& world, const VanillaRecord& dance)
{
	std::vector<ImportedDanceKeyFrame> keyFrames;
	for (const auto number : world.Refs(dance, "danceKeyFrames"))
	{
		const auto* record = number != 0 ? world.Record(number) : nullptr;
		if (record == nullptr || !record->Is(VanillaType::DanceKeyFrame))
		{
			continue;
		}
		ImportedDanceKeyFrame keyFrame {.time = world.Float(*record, "keyFrameTime").value_or(0.0f),
		                                .flags = world.Word(*record, "keyFrameFlags").value_or(0),
		                                .actions = {}};
		for (const auto actionNumber : world.Refs(*record, "keyFrameActions"))
		{
			const auto* action = actionNumber != 0 ? world.Record(actionNumber) : nullptr;
			if (action == nullptr || !action->Is(VanillaType::DanceKeyAction))
			{
				continue;
			}
			ImportedDanceAction values {.groups = WordsOf(world.Raw(*action, "keyActionGroups")),
			                            .type = world.Word(*action, "keyActionType").value_or(0),
			                            .arguments = {}};
			if (const auto arguments = WordsOf(world.Raw(*action, "keyActionArguments"));
			    arguments.size() == values.arguments.size())
			{
				std::ranges::copy(arguments, values.arguments.begin());
			}
			keyFrame.actions.push_back(std::move(values));
		}
		keyFrames.push_back(std::move(keyFrame));
	}
	return keyFrames;
}

/// The places in the list of all groups of the groups of a list of the file
std::vector<size_t> GroupPlaces(const std::vector<ImportedDanceGroup>& groups, std::span<const uint32_t> numbers)
{
	std::vector<size_t> places;
	for (const auto number : numbers)
	{
		const auto found = std::ranges::find(groups, number, &ImportedDanceGroup::number);
		if (found != groups.end())
		{
			places.push_back(static_cast<size_t>(std::distance(groups.begin(), found)));
		}
	}
	return places;
}
} // namespace

std::optional<ImportedDance> ReadDance(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto row = world.Word(record, "danceRow");
	if (!record.Is(VanillaType::Dance) || !row.has_value() || *row > static_cast<uint32_t>(DanceInfo::CitadelDance_6))
	{
		return std::nullopt;
	}
	ImportedDance dance {
	    .number = record.number,
	    .type = static_cast<DanceInfo>(*row),
	    .place = PositionOf(world, record).value_or(gamesave::MapCoords {}),
	    .owner = world.Ref(record, "danceOwner").value_or(0),
	    .madeByScript = world.Word(record, "danceCameraOff").value_or(0) != 0,
	    .clock = world.Float(record, "danceClock").value_or(0.0f),
	    .loopLength = world.Word(record, "danceLoopLength").value_or(1),
	    .speed = world.Float(record, "danceSpeed").value_or(0.0f),
	    .rate = world.Float(record, "danceRate").value_or(1.0f),
	    .dancingRate = world.Float(record, "danceDancingRate").value_or(1.0f),
	    .angle = world.Float(record, "danceAngle").value_or(0.0f),
	    // The game reads only 0 as dancing
	    .stopped = world.Word(record, "danceState").value_or(1) != 0,
	    .duration = world.Word(record, "danceDuration").value_or(0),
	    .startTurn = world.Word(record, "danceStartTurn").value_or(0),
	    .dancers = world.Word(record, "danceDancers").value_or(0),
	    .onTheirWay = world.Word(record, "danceOnTheirWay").value_or(0),
	    .handTurns = world.Word(record, "danceHandTurns").value_or(0),
	    .cameraWanted = world.Word(record, "danceCameraWanted").value_or(0) != 0,
	    .cameraMade = world.Word(record, "danceCameraMade").value_or(0) != 0,
	};
	if (world.Word(record, "danceFirstDancerCame").value_or(0) != 0)
	{
		dance.firstDancerTurn = world.Word(record, "danceFirstDancerTurn").value_or(0);
	}
	for (const auto number : world.Refs(record, "danceGroups"))
	{
		if (auto group = ReadDanceGroup(world, number); group.has_value())
		{
			dance.groups.push_back(std::move(*group));
		}
	}
	dance.limitedGroups = GroupPlaces(dance.groups, world.Refs(record, "danceLimitedGroups"));
	dance.sharedGroups = GroupPlaces(dance.groups, world.Refs(record, "danceSharedGroups"));
	dance.round = world.Byte(record, "danceRound").value_or(0);
	dance.roundLength = world.Word(record, "danceRoundLength").value_or(0);
	dance.keyFrames = ReadDanceKeyFrames(world, record);
	return dance;
}

ImportedDances ReadOtherDances(const VanillaWorld& world)
{
	std::unordered_set<uint32_t> ofSites;
	for (const auto& record : world.Records())
	{
		if (record.Is(VanillaType::WorshipSite))
		{
			ofSites.insert(world.Ref(record, "siteDance").value_or(0));
		}
	}
	ImportedDances dances;
	for (const auto& record : world.Records())
	{
		if (!record.Is(VanillaType::Dance) || ofSites.contains(record.number))
		{
			continue;
		}
		if (auto dance = ReadDance(world, record); dance.has_value())
		{
			dances.made.push_back(std::move(*dance));
		}
	}
	return dances;
}

dances::DanceSetup SetupOf(const ImportedDance& file, entt::entity owner, bool autostart)
{
	return {.type = file.type,
	        .autostart = autostart,
	        .place = map_coords::MapCoords {.x = file.place.x, .z = file.place.z, .altitude = file.place.altitude},
	        .owner = owner,
	        .duration = file.duration,
	        .madeByScript = file.madeByScript,
	        .angle = file.angle};
}

std::optional<ImportedWorshipIcon> ReadWorshipIcon(const VanillaWorld& world, const VanillaRecord& record)
{
	const auto seed = SpellSeedRowOf(world.Word(record, "iconSeed").value_or(0));
	if (!record.Is(VanillaType::WorshipSpellIcon) || !seed.has_value())
	{
		return std::nullopt;
	}
	ImportedWorshipIcon icon {
	    .number = record.number,
	    .seed = *seed,
	    .slot = static_cast<int16_t>(world.Half(record, "iconSlot").value_or(0)),
	    .charging = world.Byte(record, "iconCharging").value_or(0) != 0,
	    .powerUp = std::bit_cast<int32_t>(world.Word(record, "iconPowerUp").value_or(0xFFFFFFFFU)),
	    .charger = PlayerOfHand(world, world.Ref(record, "iconCharger").value_or(0)),
	    .chargeStartTurn = world.Word(record, "iconChargeStart").value_or(0),
	    .store = world.Float(record, "iconStore").value_or(0.0f),
	    .removeTurns = world.Half(record, "iconRemoveTurns").value_or(0),
	};
	const auto seeds = world.Refs(record, "iconSeedsOut");
	icon.seedsOut.assign(seeds.begin(), seeds.end());
	return icon;
}

void PutDanceValues(components::Dance& dance, const ImportedDance& file)
{
	dance.clock = file.clock;
	dance.loopLength = file.loopLength;
	dance.speed = file.speed;
	dance.rate = file.rate;
	dance.dancingRate = file.dancingRate;
	dance.angle = file.angle;
	dance.state = file.stopped ? components::Dance::State::Stopped : components::Dance::State::Dancing;
	dance.duration = file.duration;
	dance.startTurn = file.startTurn;
	dance.firstDancerTurn = file.firstDancerTurn;
	auto& hand = dance.hands.front();
	hand.nearTurns = file.handTurns;
	hand.cameraWanted = file.cameraWanted;
	hand.cameraMade = file.cameraMade;
	hand.cameraAsked = false;
}

std::vector<PlacedDancer> PutDanceGroups(components::Dance& dance, const ImportedDance& file,
                                         const std::unordered_map<uint32_t, entt::entity>& kept)
{
	// The parts of a group name their groups by the number of the group in the dance
	std::unordered_map<uint32_t, size_t> places;
	for (size_t place = 0; place < file.groups.size(); ++place)
	{
		places.emplace(file.groups[place].id, place);
	}
	std::vector<PlacedDancer> placed;
	auto& groups = dance.groups;
	groups.all.clear();
	dance.dancers = 0;
	for (size_t place = 0; place < file.groups.size(); ++place)
	{
		const auto& values = file.groups[place];
		// The file keeps no rates of a move, so a group starts with no rate until a key frame starts its move again
		components::DanceGroup group {
		    .name = values.name,
		    .dancers = {},
		    .limited = values.limited,
		    .quota = values.quota,
		    .weight = values.weight,
		    .limitedDancers = values.limitedDancers,
		    .danceType = values.danceType,
		    .sexes = values.sexes,
		    .formation = values.formation,
		    .inFormation = values.inFormation,
		    .flag = values.flag,
		    .shape = values.shape,
		    .radius = values.radius,
		    .offset = values.offset,
		    .rotation = values.rotation,
		    .spin = values.spin,
		    .move = {.action = values.move.action,
		             .first = values.move.first,
		             .second = values.move.second,
		             .end = values.move.end},
		    .moved = values.moved,
		};
		if (const auto parent = std::ranges::find(file.groups, values.parent, &ImportedDanceGroup::number);
		    values.parent != 0 && parent != file.groups.end())
		{
			group.parent = static_cast<size_t>(std::distance(file.groups.begin(), parent));
			group.indexInParent = values.indexInParent;
		}
		for (const auto child : values.children)
		{
			if (const auto found = places.find(child); found != places.end())
			{
				group.children.push_back(found->second);
			}
		}
		for (const auto number : values.dancers)
		{
			const auto found = kept.find(number);
			if (found == kept.end())
			{
				// The dancer leaves the group as when it leaves the dance
				if (group.limited && group.limitedDancers > 0)
				{
					--group.limitedDancers;
				}
				continue;
			}
			group.dancers.push_back(found->second);
			placed.push_back({.dancer = found->second, .group = place});
			++dance.dancers;
		}
		groups.all.push_back(std::move(group));
	}
	groups.limited = file.limitedGroups;
	groups.shared = file.sharedGroups;
	groups.round = file.round;
	groups.roundLength = file.roundLength;
	return placed;
}

bool SameKeyFrames(const dance::DanceFile& file, const std::vector<ImportedDanceKeyFrame>& keyFrames)
{
	const auto sameAction = [](const dance::DanceAction& a, const ImportedDanceAction& b) {
		return a.type == b.type && a.arguments == b.arguments && a.groups == b.groups;
	};
	const auto sameKeyFrame = [&sameAction](const dance::DanceKeyFrame& a, const ImportedDanceKeyFrame& b) {
		return std::bit_cast<uint32_t>(a.time) == std::bit_cast<uint32_t>(b.time) && a.flags == b.flags &&
		       std::ranges::equal(a.actions, b.actions, sameAction);
	};
	return std::ranges::equal(file.keyFrames, keyFrames, sameKeyFrame);
}

void PutIconValues(components::WorshipIcon& icon, const ImportedWorshipIcon& file)
{
	icon.charging = file.charging;
	icon.powerUp = file.powerUp;
	icon.charger = file.charger.value_or(PlayerNames::NEUTRAL);
	icon.chargeStartTurn = file.chargeStartTurn;
	icon.store = file.store;
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
	return !gamesave::VillagerStateKeepsValues(state) || IsOneOf(state, k_FishermanStates) || IsOneOf(state, k_FarmerStates) ||
	       IsOneOf(state, k_ShepherdStates) || IsOneOf(state, k_ScriptValueStates) ||
	       state == static_cast<uint8_t>(VillagerStates::MoveOnPath) || state == static_cast<uint8_t>(VillagerStates::Dead) ||
	       state == static_cast<uint8_t>(VillagerStates::DiscipleNothingToDo) ||
	       state == static_cast<uint8_t>(VillagerStates::BreederDisciple);
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

namespace
{
/// The value of the type in the bytes of a value of the file, nothing when the size is not the size of the type
template <typename T>
std::optional<T> ValueOf(const ImportedStateValue& value)
{
	if (value.object.has_value() || value.bytes.size() != sizeof(T))
	{
		return std::nullopt;
	}
	T result;
	std::memcpy(&result, value.bytes.data(), sizeof(T));
	return result;
}
} // namespace

std::optional<ImportedScriptValues> ScriptValuesOf(uint8_t state, const std::vector<ImportedStateValue>& values)
{
	if (!IsOneOf(state, k_ScriptValueStates) || values.size() != 4)
	{
		return std::nullopt;
	}
	// The place (x, z), then the number, then the pair
	const auto place = ValueOf<std::array<int32_t, 2>>(values[0]);
	const auto number = ValueOf<float>(values[1]);
	const auto first = ValueOf<uint32_t>(values[2]);
	const auto second = ValueOf<uint32_t>(values[3]);
	if (!place.has_value() || !number.has_value() || !first.has_value() || !second.has_value())
	{
		return std::nullopt;
	}
	return ImportedScriptValues {.x = (*place)[0], .z = (*place)[1], .number = *number, .first = *first, .second = *second};
}

float MetresOfMapUnits(int32_t units)
{
	float metres = map_coords::ToMetres(units);
	// The rounding of the float can give one unit more or less: the value moves to the next float until it agrees
	for (int i = 0; i < k_MetreTries && map_coords::ToFixed(metres) != units; ++i)
	{
		metres = std::nextafter(metres, map_coords::ToFixed(metres) < units ? std::numeric_limits<float>::infinity()
		                                                                    : -std::numeric_limits<float>::infinity());
	}
	return metres;
}

std::optional<ImportedShepherdValues> ShepherdValuesOf(uint8_t state, const std::vector<ImportedStateValue>& values)
{
	if (!IsOneOf(state, k_ShepherdStates) || values.size() != 2)
	{
		return std::nullopt;
	}
	const auto countdown = ValueOf<int16_t>(values[0]);
	const auto pending = ValueOf<uint16_t>(values[1]);
	if (!countdown.has_value() || !pending.has_value())
	{
		return std::nullopt;
	}
	return ImportedShepherdValues {.countdown = *countdown, .pending = *pending};
}

std::optional<ImportedBreederValues> BreederValuesOf(uint8_t state, const std::vector<ImportedStateValue>& values)
{
	if (state != static_cast<uint8_t>(VillagerStates::BreederDisciple) || values.size() != 2 || !values[0].object.has_value())
	{
		return std::nullopt;
	}
	// The lover, then the place (x, z)
	const auto place = ValueOf<std::array<int32_t, 2>>(values[1]);
	if (!place.has_value())
	{
		return std::nullopt;
	}
	return ImportedBreederValues {.lover = *values[0].object, .x = (*place)[0], .z = (*place)[1]};
}

std::optional<DeathReason> DeathReasonOf(uint8_t state, const std::vector<ImportedStateValue>& values)
{
	if (state != static_cast<uint8_t>(VillagerStates::Dead) || values.size() != 1)
	{
		return std::nullopt;
	}
	const auto reason = ValueOf<uint8_t>(values[0]);
	if (!reason.has_value() || *reason >= static_cast<uint8_t>(DeathReason::_COUNT))
	{
		return std::nullopt;
	}
	return static_cast<DeathReason>(*reason);
}

bool LiesOnBack(uint16_t status)
{
	return ((status >> k_LandingPoseShift) & k_LandingPoseMask) == k_OnBackPose;
}

std::optional<ImportedTrade> TradeOf(uint8_t state, uint8_t discipleType, const std::vector<ImportedStateValue>& values)
{
	if (state != static_cast<uint8_t>(VillagerStates::DiscipleNothingToDo) ||
	    discipleType != static_cast<uint8_t>(VillagerDisciple::Trader) || values.size() != 2 || !values[1].object.has_value())
	{
		return std::nullopt;
	}
	const auto resource = ValueOf<int32_t>(values[0]);
	if (!resource.has_value())
	{
		return std::nullopt;
	}
	return ImportedTrade {.resource = static_cast<ResourceType>(*resource), .town = *values[1].object};
}

std::optional<ImportedDisciple> DiscipleOf(uint8_t discipleType, uint16_t villagerFlags)
{
	if (discipleType > static_cast<uint8_t>(VillagerDisciple::FromVortex))
	{
		return std::nullopt;
	}
	const auto type = static_cast<VillagerDisciple>(discipleType);
	if ((villagerFlags & k_DiscipleFlag) != 0)
	{
		return ImportedDisciple {.type = type, .follower = false};
	}
	if ((villagerFlags & k_FollowerFlag) != 0)
	{
		return ImportedDisciple {.type = type, .follower = true};
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

std::array<uint8_t, 3> StartStates(const ImportedVillager& villager)
{
	const auto canKeep = [&villager](uint8_t state) {
		if (state == static_cast<uint8_t>(VillagerStates::WorshippingAtWorshipSite))
		{
			return villager.importedWorshipper && villager.importedDancer;
		}
		if (IsOneOf(state, k_WorshipStates))
		{
			return villager.importedWorshipper;
		}
		if (IsOneOf(state, k_DanceStates) || IsOneOf(state, k_ArtefactDanceStates))
		{
			return villager.importedDancer;
		}
		if (IsOneOf(state, k_ReactionDanceStates))
		{
			return villager.importedDancer && villager.reaction != 0;
		}
		return CanKeepStateValues(state);
	};
	if (!CanKeepWalk(villager.moveState) || !canKeep(villager.states[0]) || !canKeep(villager.states[1]))
	{
		return DecideAgain();
	}
	return villager.states;
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
	if (record.Is(VanillaType::CitadelBuildingSite))
	{
		site.temple = true;
		const auto piles = world.Refs(record, "templeSitePiles");
		site.piles.assign(piles.begin(), piles.end());
	}
	return site;
}

namespace
{
/// The artefacts of the towns of the file. The artefacts of each town come in the reverse order of the list of the
/// town, the oldest first. The artefacts that no town lists come after them, in the order of the file.
std::vector<ImportedTownArtefact> ReadTownArtefacts(const VanillaWorld& world)
{
	std::vector<uint32_t> order;
	for (const auto* town : world.OfType(VanillaType::Town))
	{
		const auto listed = world.Refs(*town, "artifacts");
		for (const auto number : listed | std::views::reverse)
		{
			if (std::ranges::find(order, number) == order.end())
			{
				order.push_back(number);
			}
		}
	}
	const auto inTowns = order.size();
	for (const auto* record : world.OfType(VanillaType::TownArtifact))
	{
		if (std::ranges::find(order, record->number) == order.end())
		{
			order.push_back(record->number);
		}
	}
	std::vector<ImportedTownArtefact> artefacts;
	for (size_t i = 0; i < order.size(); ++i)
	{
		const auto* record = RecordOf(world, order[i], VanillaType::TownArtifact);
		if (record == nullptr)
		{
			continue;
		}
		artefacts.push_back({.number = record->number,
		                     .object = world.Ref(*record, "artefactObject").value_or(0),
		                     .town = world.Ref(*record, "artefactTown").value_or(0),
		                     .dance = world.Ref(*record, "artefactDance").value_or(0),
		                     .player = PlayerOfRecord(world, world.Ref(*record, "artefactPlayer").value_or(0)),
		                     .value = world.Float(*record, "artefactValue").value_or(0.0f),
		                     .dancing = world.Word(*record, "artefactDancing").value_or(0) != 0,
		                     .danceStarted = world.Word(*record, "artefactDanceStarted").value_or(0),
		                     .inTown = i < inTowns});
	}
	return artefacts;
}

/// Marks each villager that dances in a group of a dance that the import makes, and each villager at worship at a worship
/// site of the file. A dancer is in the list of its group, and its group is its group.
void MarkDancersAndWorshippers(ImportedTowns& imported)
{
	std::unordered_set<uint32_t> worshippers;
	std::unordered_map<uint32_t, uint32_t> groupOf;
	const auto addDancers = [&groupOf](const ImportedDance& dance) {
		for (const auto& group : dance.groups)
		{
			for (const auto dancer : group.dancers)
			{
				groupOf.emplace(dancer, group.number);
			}
		}
	};
	for (const auto& site : imported.worshipSites)
	{
		worshippers.insert(site.worshippers.begin(), site.worshippers.end());
		if (site.danceValues.has_value())
		{
			addDancers(*site.danceValues);
		}
	}
	std::ranges::for_each(imported.dances.made, addDancers);
	for (auto& villager : imported.villagers)
	{
		const auto group = groupOf.find(villager.number);
		villager.importedDancer = group != groupOf.end() && villager.danceGroup == group->second;
		villager.importedWorshipper = worshippers.contains(villager.number);
	}
}
} // namespace

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
		else if (record.Is(VanillaType::TotemStatue))
		{
			const auto position = PositionOf(world, record);
			const auto townCentre = world.Ref(record, "totemTownCentre");
			const auto share = world.Float(record, "totemShare");
			const auto held = world.Float(record, "totemHeldShare");
			const auto word84 = world.Word(record, "totemWord84");
			if (position.has_value() && townCentre.has_value() && *townCentre != 0 && share.has_value() && held.has_value() &&
			    word84.has_value())
			{
				imported.totems.push_back({.number = record.number,
				                           .townCentre = *townCentre,
				                           .position = *position,
				                           .heading = world.Float(record, "heading").value_or(0.0f),
				                           .scale = world.Float(record, "scale").value_or(1.0f),
				                           .life = world.Float(record, "life").value_or(1.0f),
				                           .share = *share,
				                           .heldShare = *held,
				                           .word84 = *word84});
				continue;
			}
			why = "the totem has no place, no town centre or not all its values";
		}
		else if (record.Is(VanillaType::PlannedAbode) || record.Is(VanillaType::PlannedTownCentre))
		{
			if (auto plan = ReadPlan(world, record, why); plan.has_value())
			{
				imported.plans.push_back(*plan);
				continue;
			}
		}
		else if (record.Is(VanillaType::StandardBuildingSite) || record.Is(VanillaType::CitadelBuildingSite))
		{
			if (auto site = ReadSite(world, record, why); site.has_value())
			{
				imported.sites.push_back(std::move(*site));
				continue;
			}
		}
		else if (record.Is(VanillaType::WorshipSite))
		{
			if (auto site = ReadWorshipSite(world, record, why); site.has_value())
			{
				imported.worshipSites.push_back(std::move(*site));
				continue;
			}
		}
		else if (record.Is(VanillaType::Scaffold))
		{
			if (auto scaffold = ReadScaffold(world, record, why); scaffold.has_value())
			{
				imported.scaffolds.push_back(*scaffold);
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
	PutPiles(world, imported);
	imported.dances = ReadOtherDances(world);
	imported.artefacts = ReadTownArtefacts(world);
	if (!world.Records().empty() && world.Game().Is(VanillaType::Game))
	{
		if (const auto lists = world.Fields(world.Game(), "lists"); lists.size() == k_GameObjectLists)
		{
			const auto dances = world.Refs(*lists.at(k_GameDanceList));
			imported.dances.gameList.assign(dances.begin(), dances.end());
		}
	}
	MarkDancersAndWorshippers(imported);
	return imported;
}

std::vector<SiteListing> PlanSiteListings(std::span<const ImportedTown> towns,
                                          const std::function<std::optional<std::size_t>(uint32_t)>& ownerOf)
{
	// The towns that list each site, in the order of the towns
	std::unordered_map<uint32_t, std::vector<std::size_t>> listers;
	for (std::size_t town = 0; town < towns.size(); ++town)
	{
		for (const auto site : towns[town].sites)
		{
			auto& list = listers[site];
			if (std::ranges::find(list, town) == list.end())
			{
				list.push_back(town);
			}
		}
	}
	std::unordered_map<uint32_t, std::size_t> openers;
	for (const auto& [site, list] : listers)
	{
		const auto owner = ownerOf(site);
		const bool ownerLists = owner.has_value() && std::ranges::find(list, *owner) != list.end();
		openers.emplace(site, ownerLists ? *owner : list.front());
	}
	std::vector<SiteListing> listings;
	for (std::size_t town = 0; town < towns.size(); ++town)
	{
		for (const auto site : std::views::reverse(towns[town].sites))
		{
			listings.push_back({.town = town, .site = site, .opener = openers.at(site)});
		}
	}
	return listings;
}

magic::piles::Rise PileRiseOf(std::span<const uint8_t, k_PileRiseBytes> bytes)
{
	std::array<float, k_PileRiseBytes / sizeof(float)> values {};
	std::memcpy(values.data(), bytes.data(), bytes.size());
	// The game does not read the fifth value of the file
	return {.start = values[7],
	        .speed = values[8],
	        .acceleration = values[9],
	        .jerk = values[10],
	        .snap = values[11],
	        .target = values[1],
	        .time = values[5],
	        .duration = values[6],
	        .offset = values[0],
	        .currentSpeed = values[3]};
}

} // namespace openblack::ecs::vanilla_world_import

std::vector<entt::entity> openblack::ecs::vanilla_world_import::LandThingsToRemove(std::span<const entt::entity> land,
                                                                                   std::span<const entt::entity> taken)
{
	std::vector<entt::entity> removed;
	std::ranges::copy_if(land, std::back_inserter(removed),
	                     [taken](entt::entity thing) { return std::ranges::find(taken, thing) == taken.end(); });
	return removed;
}

std::vector<entt::entity>
openblack::ecs::vanilla_world_import::DancesToJoin(std::span<const uint32_t> gameList,
                                                   const std::unordered_map<uint32_t, entt::entity>& made,
                                                   std::span<const entt::entity> dances)
{
	std::vector<entt::entity> order;
	// The last dance of the game's list is the oldest, so it joins first
	for (const auto number : gameList | std::views::reverse)
	{
		const auto entity = made.find(number);
		if (entity != made.end() && std::ranges::find(dances, entity->second) != dances.end() &&
		    std::ranges::find(order, entity->second) == order.end())
		{
			order.push_back(entity->second);
		}
	}
	return order;
}
