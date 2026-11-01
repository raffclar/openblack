/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaSceneryImport.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

#include "ECS/ScriptFlockRules.h"
#include "VanillaWorld.h"
#include "VanillaWorldImport.h"

namespace openblack::ecs::vanilla_world_import
{
namespace
{
using gamesave::VanillaRecord;
using gamesave::VanillaType;
using gamesave::VanillaWorld;

/// The place of the list of forests among the lists of the game object
constexpr size_t k_ForestListIndex = 4;

/// The row of a table that has the number, or nothing for a number of another table
template <typename Row>
std::optional<Row> RowIn(uint32_t number, uint32_t first, uint32_t count)
{
	if (number > first || first - number >= count)
	{
		return std::nullopt;
	}
	return static_cast<Row>(first - number);
}

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

std::string Skip(const VanillaRecord& record, std::string_view why)
{
	return "object " + std::to_string(record.number) + " of type " + std::to_string(record.type) + ": " + std::string(why);
}

std::optional<ImportedTree> ReadTree(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto row = TreeRowOf(world.Word(record, "info").value_or(0));
	if (!position.has_value() || !row.has_value())
	{
		why = "the tree has no place or no row of the table of trees";
		return std::nullopt;
	}
	const float scale = world.Float(record, "scale").value_or(1.0f);
	return ImportedTree {.number = record.number,
	                     .row = *row,
	                     .position = *position,
	                     .heading = world.Float(record, "heading").value_or(0.0f),
	                     .scale = scale,
	                     .life = world.Float(record, "life").value_or(1.0f),
	                     .maxSize = world.Float(record, "maxSize").value_or(scale),
	                     .growthCountdown = world.Half(record, "growthCountdown").value_or(0)};
}

std::optional<ImportedBigForest> ReadBigForest(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto row = BigForestRowOf(world.Word(record, "info").value_or(0));
	if (!position.has_value() || !row.has_value())
	{
		why = "the big forest has no place or no row of the table of big forests";
		return std::nullopt;
	}
	return ImportedBigForest {.number = record.number,
	                          .row = *row,
	                          .position = *position,
	                          .heading = world.Float(record, "heading").value_or(0.0f),
	                          .scale = world.Float(record, "scale").value_or(1.0f),
	                          .life = world.Float(record, "life").value_or(1.0f),
	                          .wood = world.Float(record, "wood").value_or(0.0f)};
}

std::optional<ImportedForest> ReadForest(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto id = world.Word(record, "forestId");
	if (!position.has_value() || !id.has_value())
	{
		why = "the forest has no place or no number";
		return std::nullopt;
	}
	ImportedForest forest {.number = record.number,
	                       .id = *id,
	                       .position = *position,
	                       .scenic = world.Word(record, "scenic").value_or(0) == 1,
	                       .bigForest = world.Ref(record, "bigForest").value_or(0)};
	const auto grown = world.Refs(record, "grownTrees");
	forest.grown.assign(grown.begin(), grown.end());
	const auto growing = world.Refs(record, "growingTrees");
	forest.growing.assign(growing.begin(), growing.end());
	return forest;
}

std::optional<ImportedField> ReadField(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	const auto row = AbodeRowOf(world.Word(record, "info").value_or(0));
	const auto type = FieldTypeRowOf(world.Word(record, "cropInfo").value_or(0));
	const auto town = world.Ref(record, "fieldTown").value_or(world.Ref(record, "town").value_or(0));
	if (!position.has_value() || !row.has_value() || !type.has_value() || town == 0)
	{
		why = "the field has no place, no town, or no row of the tables of buildings and crops";
		return std::nullopt;
	}
	ImportedField field {.number = record.number,
	                     .row = *row,
	                     .type = *type,
	                     .town = town,
	                     .position = *position,
	                     .heading = world.Float(record, "heading").value_or(0.0f),
	                     .scale = world.Float(record, "scale").value_or(1.0f),
	                     .life = world.Float(record, "life").value_or(1.0f),
	                     .built = world.Float(record, "built").value_or(1.0f),
	                     .timesSown = world.Byte(record, "timesSown").value_or(0),
	                     .age = world.Float(record, "age").value_or(0.0f),
	                     .food = world.Float(record, "food").value_or(0.0f),
	                     .growthTurn = world.Word(record, "growthTurn").value_or(0)};
	const auto farmers = world.Refs(record, "farmers");
	field.farmers.assign(farmers.begin(), farmers.end());
	return field;
}

std::optional<ImportedFishFarm> ReadFishFarm(const VanillaWorld& world, const VanillaRecord& record, std::string& why)
{
	const auto position = PositionOf(world, record);
	if (!position.has_value())
	{
		why = "the fish farm has no place";
		return std::nullopt;
	}
	ImportedFishFarm farm {.number = record.number,
	                       .position = *position,
	                       .town = world.Ref(record, "town").value_or(0),
	                       .fish = world.Float(record, "fish").value_or(0.0f)};
	const auto fishermen = world.Refs(record, "fishermen");
	farm.fishermen.assign(fishermen.begin(), fishermen.end());
	return farm;
}

/// The numbers of the forests in the list of the game object, in the order of the list
std::vector<uint32_t> ForestOrder(const VanillaWorld& world)
{
	if (world.Records().empty())
	{
		return {};
	}
	const auto lists = world.Fields(world.Game(), "lists");
	if (lists.size() <= k_ForestListIndex)
	{
		return {};
	}
	const auto numbers = world.Refs(*lists[k_ForestListIndex]);
	return {numbers.begin(), numbers.end()};
}

/// The kind of still thing or loose thing that a record is, nothing for any other record
std::optional<StaticKind> StaticKindOf(const VanillaRecord& record)
{
	switch (static_cast<VanillaType>(record.type))
	{
	case VanillaType::MobileStatic:
		return StaticKind::MobileStatic;
	case VanillaType::Rock:
	case VanillaType::Bonfire:
		return StaticKind::Rock;
	case VanillaType::BaseOnly:
		return StaticKind::BaseOnly;
	case VanillaType::StreetLantern:
		return StaticKind::StreetLantern;
	case VanillaType::DeadTree:
		return StaticKind::DeadTree;
	case VanillaType::Feature:
		return StaticKind::Feature;
	case VanillaType::AnimatedStatic:
		return StaticKind::AnimatedStatic;
	case VanillaType::MobileObject:
	case VanillaType::Poo:
		return StaticKind::MobileObject;
	default:
		return std::nullopt;
	}
}

/// The row of a still thing or a loose thing in its table. A dead tree keeps the row of its tree after the row of the
/// static things.
std::optional<uint32_t> StaticRowOf(const VanillaWorld& world, const VanillaRecord& record, StaticKind kind)
{
	const auto info = world.Word(record, "info").value_or(0);
	const auto index = [](auto row) { return row.has_value() ? std::optional(static_cast<uint32_t>(*row)) : std::nullopt; };
	switch (kind)
	{
	case StaticKind::MobileStatic:
	case StaticKind::Rock:
	case StaticKind::BaseOnly:
	case StaticKind::StreetLantern:
		return index(MobileStaticRowOf(info));
	case StaticKind::DeadTree:
		return index(TreeRowOf(world.Word(record, "treeInfo").value_or(0)));
	case StaticKind::Feature:
		return index(FeatureRowOf(info));
	case StaticKind::AnimatedStatic:
		return index(AnimatedStaticRowOf(info));
	case StaticKind::MobileObject:
		return index(MobileObjectRowOf(info));
	}
	return std::nullopt;
}

} // namespace

std::optional<TreeInfo> TreeRowOf(uint32_t number)
{
	return RowIn<TreeInfo>(number, k_FirstTreeRowNumber, static_cast<uint32_t>(TreeInfo::_COUNT));
}

std::optional<FeatureInfo> FeatureRowOf(uint32_t number)
{
	return RowIn<FeatureInfo>(number, k_FirstFeatureRowNumber, static_cast<uint32_t>(FeatureInfo::_COUNT));
}

std::optional<AnimatedStaticInfo> AnimatedStaticRowOf(uint32_t number)
{
	return RowIn<AnimatedStaticInfo>(number, k_FirstAnimatedStaticRowNumber, static_cast<uint32_t>(AnimatedStaticInfo::_COUNT));
}

std::optional<MobileObjectInfo> MobileObjectRowOf(uint32_t number)
{
	return RowIn<MobileObjectInfo>(number, k_FirstMobileObjectRowNumber, static_cast<uint32_t>(MobileObjectInfo::_COUNT));
}

std::optional<MobileStaticInfo> MobileStaticRowOf(uint32_t number)
{
	return RowIn<MobileStaticInfo>(number, k_FirstMobileStaticRowNumber, static_cast<uint32_t>(MobileStaticInfo::_COUNT));
}

std::optional<PotInfo> PotRowOf(uint32_t number)
{
	return RowIn<PotInfo>(number, k_FirstPotRowNumber, static_cast<uint32_t>(PotInfo::_COUNT));
}

std::optional<FieldTypeInfo> FieldTypeRowOf(uint32_t number)
{
	return RowIn<FieldTypeInfo>(number, k_FirstFieldTypeRowNumber, static_cast<uint32_t>(FieldTypeInfo::_COUNT));
}

std::optional<AnimalInfo> AnimalRowOf(uint32_t number)
{
	return RowIn<AnimalInfo>(number, k_FirstAnimalRowNumber, static_cast<uint32_t>(AnimalInfo::_COUNT));
}

std::optional<BigForestInfo> BigForestRowOf(uint32_t number)
{
	return RowIn<BigForestInfo>(number, k_FirstBigForestRowNumber, static_cast<uint32_t>(BigForestInfo::_COUNT));
}

std::optional<uint32_t> TotemStatueRowOf(uint32_t number)
{
	constexpr uint32_t k_TotemStatueRows = 9;
	return RowIn<uint32_t>(number, k_FirstTotemStatueRowNumber, k_TotemStatueRows);
}

ImportedForests ReadForests(const VanillaWorld& world)
{
	ImportedForests imported;
	std::vector<ImportedForest> forests;
	for (const auto& record : world.Records())
	{
		std::string why;
		if (record.Is(VanillaType::Tree))
		{
			if (auto tree = ReadTree(world, record, why); tree.has_value())
			{
				imported.trees.push_back(*tree);
				continue;
			}
		}
		else if (record.Is(VanillaType::BigForest))
		{
			if (auto bigForest = ReadBigForest(world, record, why); bigForest.has_value())
			{
				imported.bigForests.push_back(*bigForest);
				continue;
			}
		}
		else if (record.Is(VanillaType::Forest))
		{
			if (auto forest = ReadForest(world, record, why); forest.has_value())
			{
				// Each forest keeps the two counts of all the forests. The last forest of the file gives them.
				imported.lastTreeAddedTurn = world.Word(record, "lastTreeAddedTurn").value_or(0);
				imported.nextForestId = world.Word(record, "nextForestId").value_or(1);
				forests.push_back(std::move(*forest));
				continue;
			}
		}
		else
		{
			continue;
		}
		imported.skipped.push_back(Skip(record, why));
	}

	// The forests in the order of the game's list, then the forests that the list does not have
	std::unordered_set<uint32_t> listed;
	for (const auto number : ForestOrder(world))
	{
		const auto found =
		    std::ranges::find_if(forests, [number](const ImportedForest& forest) { return forest.number == number; });
		if (found != forests.end() && listed.insert(number).second)
		{
			imported.forests.push_back(*found);
		}
	}
	std::ranges::copy_if(forests, std::back_inserter(imported.forests),
	                     [&listed](const ImportedForest& forest) { return !listed.contains(forest.number); });
	return imported;
}

ImportedFarms ReadFarms(const VanillaWorld& world)
{
	ImportedFarms imported;
	for (const auto& record : world.Records())
	{
		std::string why;
		if (record.Is(VanillaType::Field))
		{
			if (auto field = ReadField(world, record, why); field.has_value())
			{
				imported.fields.push_back(std::move(*field));
				continue;
			}
		}
		else if (record.Is(VanillaType::FishFarm))
		{
			if (auto farm = ReadFishFarm(world, record, why); farm.has_value())
			{
				imported.fishFarms.push_back(std::move(*farm));
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

ImportedStatics ReadStatics(const VanillaWorld& world)
{
	ImportedStatics imported;
	for (const auto& record : world.Records())
	{
		const auto kind = StaticKindOf(record);
		if (!kind.has_value())
		{
			continue;
		}
		const auto position = PositionOf(world, record);
		const auto row = StaticRowOf(world, record, *kind);
		if (!position.has_value() || !row.has_value())
		{
			imported.skipped.push_back(Skip(record, "the thing has no place or no row of its table"));
			continue;
		}
		imported.statics.push_back({.number = record.number,
		                            .kind = *kind,
		                            .row = *row,
		                            .position = *position,
		                            .heading = world.Float(record, "heading").value_or(0.0f),
		                            .scale = world.Float(record, "scale").value_or(1.0f),
		                            .life = world.Float(record, "life").value_or(1.0f),
		                            .xAngle = world.Float(record, "xAngle").value_or(0.0f),
		                            .zAngle = world.Float(record, "zAngle").value_or(0.0f),
		                            .woodMultiplier = world.Float(record, "woodMultiplier").value_or(1.0f)});
	}
	return imported;
}

ImportedAnimals ReadAnimals(const VanillaWorld& world)
{
	ImportedAnimals imported;
	// The towns of the file give the town of each animal and of each flock that belongs to a town
	std::unordered_map<uint32_t, uint32_t> townOf;
	for (const auto* town : world.OfType(VanillaType::Town))
	{
		for (const auto number : world.Refs(*town, "animals"))
		{
			townOf.emplace(number, town->number);
		}
		for (const auto number : world.Refs(*town, "flocks"))
		{
			townOf.emplace(number, town->number);
		}
	}
	const auto town = [&townOf](uint32_t number) {
		const auto found = townOf.find(number);
		return found != townOf.end() ? found->second : 0;
	};
	for (const auto& record : world.Records())
	{
		if (record.Is(VanillaType::Flock))
		{
			const auto place = PositionOf(world, record);
			const auto saved = world.Raw(record, "domainCentre");
			const auto radius = world.Half(record, "domainRadius");
			const auto distance = world.Half(record, "flockDistance");
			if (!place.has_value() || saved.size() != sizeof(gamesave::MapCoords) || !radius.has_value() ||
			    !distance.has_value())
			{
				imported.skipped.push_back(Skip(record, "the flock has no place or not all its values"));
				continue;
			}
			ImportedFlock flock {.number = record.number,
			                     .place = *place,
			                     .domainRadius = *radius,
			                     .flockDistance = *distance,
			                     .shepherd = world.Ref(record, "shepherd").value_or(0),
			                     .heart = world.Ref(record, "heart").value_or(0),
			                     .town = town(record.number)};
			std::memcpy(&flock.savedPlace, saved.data(), sizeof(flock.savedPlace));
			const auto members = world.Refs(record, "members");
			flock.members.assign(members.begin(), members.end());
			for (const auto member : members)
			{
				const auto* living = world.Record(member);
				flock.memberOrders.push_back(living != nullptr ? world.Byte(*living, "flockOrder").value_or(0) : uint8_t {0});
			}
			flock.mostMembers = world.Word(record, "mostMembers").value_or(0);
			flock.turnsOnLeg = world.Word(record, "turnsOnLeg").value_or(0);
			flock.height = world.Float(record, "flockHeight").value_or(0.0f);
			flock.keepsTogether = world.Word(record, "keepsTogether").value_or(0) != 0;
			flock.calm = static_cast<int32_t>(world.Word(record, "calm").value_or(0));
			if (const auto id = world.Word(record, "flockScriptId").value_or(0); id != 0)
			{
				flock.scriptId = static_cast<int32_t>(id);
			}
			imported.flocks.push_back(std::move(flock));
			continue;
		}
		// An animal is a living thing that is not a villager, with a row of the table of the animals
		if (world.Field(record, "birthTurn") == nullptr || world.Field(record, "villagerFlags") != nullptr)
		{
			continue;
		}
		const auto row = AnimalRowOf(world.Word(record, "info").value_or(0));
		if (!row.has_value())
		{
			continue;
		}
		const auto position = PositionOf(world, record);
		const auto action = world.Raw(record, "action");
		if (!position.has_value() || action.size() < 2)
		{
			imported.skipped.push_back(Skip(record, "the animal has no place or no states"));
			continue;
		}
		const auto walk = ReadWalk(world, record);
		const auto bank = world.Raw(record, "bank");
		const auto sleepCell = world.Raw(record, "sleepCell");
		imported.animals.push_back(
		    {.number = record.number,
		     .row = *row,
		     .position = *position,
		     .scale = world.Float(record, "scale").value_or(1.0f),
		     .life = world.Float(record, "life").value_or(1.0f),
		     .birthTurn = world.Word(record, "birthTurn").value_or(0),
		     .goal = walk.goal,
		     .angle = walk.angle,
		     .speed = walk.speed,
		     .stepX = walk.stepX,
		     .stepZ = walk.stepZ,
		     .moveState = world.Byte(record, "moveState").value_or(0),
		     .state = action[0],
		     .finalState = action[1],
		     .flock = world.Ref(record, "flock").value_or(0),
		     .town = town(record.number),
		     .hunger = static_cast<int16_t>(world.Half(record, "hunger").value_or(0)),
		     .sleep = static_cast<int16_t>(world.Half(record, "sleep").value_or(0)),
		     .breed = static_cast<int16_t>(world.Half(record, "breed").value_or(0)),
		     .sleepCell = sleepCell.size() == 4 ? CellOf(sleepCell.first<4>()) : std::nullopt,
		     .bank = bank.size() == k_GlideBytes ? GlideOf(bank.first<k_GlideBytes>()) : animals::Zoomer::State {},
		     .stepAltitude = walk.stepAltitude,
		     .circle = walk.circle});
	}
	return imported;
}

std::vector<size_t> LineAfterLoad(std::span<const uint8_t> orders)
{
	std::vector<size_t> line;
	std::vector<uint8_t> lineOrders;
	for (size_t i = 0; i < orders.size(); ++i)
	{
		const auto at = static_cast<std::ptrdiff_t>(script_flock_rules::InsertAt(lineOrders, orders[i]));
		line.insert(line.begin() + at, i);
		lineOrders.insert(lineOrders.begin() + at, orders[i]);
	}
	return line;
}

animals::Zoomer::State GlideOf(std::span<const uint8_t, k_GlideBytes> bytes)
{
	std::array<float, k_GlideBytes / sizeof(float)> values {};
	std::memcpy(values.data(), bytes.data(), bytes.size());
	// The game does not read the fifth value of the file
	return {.value = values[0],
	        .speed = values[3],
	        .target = values[1],
	        .targetSpeed = values[2],
	        .elapsed = values[5],
	        .duration = values[6],
	        .start = values[7],
	        .startSpeed = values[8],
	        .c2 = values[9],
	        .c3 = values[10],
	        .c4 = values[11]};
}

std::optional<glm::ivec2> CellOf(std::span<const uint8_t, 4> bytes)
{
	std::array<uint16_t, 2> halves {};
	std::memcpy(halves.data(), bytes.data(), bytes.size());
	if (halves[0] == 0 && halves[1] == 0)
	{
		return std::nullopt;
	}
	return glm::ivec2 {halves[0], halves[1]};
}

std::optional<ImportedDayCycle> ReadDayCycle(const VanillaWorld& world)
{
	if (world.Records().empty())
	{
		return std::nullopt;
	}
	const auto& game = world.Game();
	const auto visualTime = world.Float(game, "visualTime");
	const auto duration = world.Float(game, "dayDuration");
	const auto night = world.Float(game, "dayNight");
	const auto change = world.Float(game, "dayChange");
	const auto scale = world.Float(game, "visualTimeScale");
	if (!visualTime.has_value() || !duration.has_value() || !night.has_value() || !change.has_value() || !scale.has_value())
	{
		return std::nullopt;
	}
	return ImportedDayCycle {
	    .visualTime = *visualTime, .duration = *duration, .night = *night, .change = *change, .scale = *scale};
}

} // namespace openblack::ecs::vanilla_world_import
