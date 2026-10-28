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
#include <iterator>
#include <unordered_set>

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

} // namespace openblack::ecs::vanilla_world_import
