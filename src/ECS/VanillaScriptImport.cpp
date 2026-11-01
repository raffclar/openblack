/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VanillaScriptImport.h"

#include <cstring>

#include <algorithm>
#include <span>
#include <string_view>

#include <VanillaWorld.h>

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::vanilla_script_import;

namespace
{

/// The little-endian value that starts at the field
template <typename T>
T ValueOf(const gamesave::VanillaWorld& world, const gamesave::VanillaField& field)
{
	T value {};
	const auto bytes = world.Bytes();
	if (field.at + sizeof(T) <= bytes.size())
	{
		std::memcpy(&value, bytes.data() + field.at, sizeof(T));
	}
	return value;
}

/// The value of one object, changed from a place of the table to the entity in the place
uint32_t EntityInPlace(uint32_t place, const script_objects::Table& table, ImportedScripts& counts)
{
	if (place == 0)
	{
		return 0;
	}
	++counts.objects;
	if (place >= script_objects::k_Places || !table.At(static_cast<uint16_t>(place)).used ||
	    table.At(static_cast<uint16_t>(place)).object == 0)
	{
		++counts.lost;
		return 0;
	}
	return table.At(static_cast<uint16_t>(place)).object;
}

void ImportValues(std::vector<lhvm::VMVar>& values, const script_objects::Table& table, ImportedScripts& counts)
{
	for (auto& value : values)
	{
		if (value.type == lhvm::DataType::Object)
		{
			value.value.uintVal = EntityInPlace(value.value.uintVal, table, counts);
		}
	}
}

void ImportStack(lhvm::VMStack& stack, const script_objects::Table& table, ImportedScripts& counts)
{
	for (size_t i = 0; i < stack.count && i < stack.values.size(); ++i)
	{
		if (stack.types.at(i) == lhvm::DataType::Object)
		{
			stack.values.at(i).uintVal = EntityInPlace(stack.values.at(i).uintVal, table, counts);
		}
	}
}

std::optional<gamesave::MapCoords> PositionOf(const gamesave::VanillaWorld& world, const gamesave::VanillaRecord& record)
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

std::string Skip(const gamesave::VanillaRecord& record, std::string_view why)
{
	return "object " + std::to_string(record.number) + " of type " + std::to_string(record.type) + ": " + std::string(why);
}

/// The player that a player record of the file is
std::optional<PlayerNames> PlayerOfRecord(const gamesave::VanillaWorld& world, uint32_t number)
{
	const auto* record = world.Record(number);
	if (record == nullptr || !record->Is(gamesave::VanillaType::Player))
	{
		return std::nullopt;
	}
	const auto player = world.Byte(*record, "number");
	if (!player.has_value() || *player >= static_cast<uint8_t>(PlayerNames::_COUNT))
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(*player);
}

} // namespace

std::optional<HighlightInfo> vanilla_script_import::HighlightRowOf(uint32_t number)
{
	constexpr auto k_Rows = static_cast<uint32_t>(HighlightInfo::Gold) + 1;
	if (number > k_FirstHighlightRowNumber || k_FirstHighlightRowNumber - number >= k_Rows)
	{
		return std::nullopt;
	}
	return static_cast<HighlightInfo>(k_FirstHighlightRowNumber - number);
}

ImportedScriptThings vanilla_script_import::ReadScriptThings(const gamesave::VanillaWorld& world)
{
	using gamesave::VanillaType;
	ImportedScriptThings things;
	for (const auto& record : world.Records())
	{
		const auto position = PositionOf(world, record);
		if (record.Is(VanillaType::ScriptMarker))
		{
			if (!position.has_value())
			{
				things.skipped.push_back(Skip(record, "the marker has no place"));
				continue;
			}
			things.markers.push_back({.number = record.number, .position = *position});
		}
		else if (record.Is(VanillaType::ScriptTimer))
		{
			const auto setTurn = world.Word(record, "timerSetTurn");
			const auto turns = world.Word(record, "timerTurns");
			if (!position.has_value() || !setTurn.has_value() || !turns.has_value())
			{
				things.skipped.push_back(Skip(record, "the timer has no place or no turns"));
				continue;
			}
			things.timers.push_back(
			    {.number = record.number, .position = *position, .setTurn = *setTurn, .turns = static_cast<int32_t>(*turns)});
		}
		else if (record.Is(VanillaType::ScriptHighlight))
		{
			const auto kind = HighlightRowOf(world.Word(record, "info").value_or(0));
			const auto active = world.Word(record, "highlightActive");
			const auto heightSet = world.Word(record, "highlightHeightSet");
			const auto height = world.Float(record, "highlightHeight");
			const auto text = world.Word(record, "highlightText");
			const auto category = world.Word(record, "highlightCategory");
			if (!position.has_value() || !kind.has_value() || !active.has_value() || !heightSet.has_value() ||
			    !height.has_value() || !text.has_value() || !category.has_value())
			{
				things.skipped.push_back(Skip(record, "the highlight has no place, no row or not all its values"));
				continue;
			}
			things.highlights.push_back({.number = record.number,
			                             .position = *position,
			                             .kind = *kind,
			                             .active = *active != 0,
			                             .drawHeight = *heightSet != 0 ? std::optional(*height) : std::nullopt,
			                             .text = *text,
			                             .category = *category});
		}
		else if (record.Is(VanillaType::InfluenceRing))
		{
			const auto player = PlayerOfRecord(world, world.Ref(record, "player").value_or(0));
			const auto radius = world.Float(record, "radius");
			const auto anti = world.Word(record, "anti");
			if (!position.has_value() || !player.has_value() || !radius.has_value() || !anti.has_value())
			{
				things.skipped.push_back(Skip(record, "the influence has no place, no player or not all its values"));
				continue;
			}
			things.rings.push_back({.number = record.number,
			                        .position = *position,
			                        .follows = world.Ref(record, "follows").value_or(0),
			                        .player = *player,
			                        .radius = *radius,
			                        .anti = *anti != 0});
		}
	}
	return things;
}

std::optional<PlayerNames> vanilla_script_import::HeartPlayer(const gamesave::VanillaWorld& world, uint32_t number)
{
	if (number == 0)
	{
		return std::nullopt;
	}
	for (const auto* citadel : world.OfType(gamesave::VanillaType::Citadel))
	{
		if (world.Ref(*citadel, "heart").value_or(0) == number)
		{
			return PlayerOfRecord(world, world.Ref(*citadel, "owner").value_or(0));
		}
	}
	return std::nullopt;
}

std::vector<vanilla_script_import::ImportedTemple> vanilla_script_import::ReadTemples(const gamesave::VanillaWorld& world)
{
	std::vector<ImportedTemple> temples;
	for (const auto* citadel : world.OfType(gamesave::VanillaType::Citadel))
	{
		const auto player = PlayerOfRecord(world, world.Ref(*citadel, "owner").value_or(0));
		if (!player.has_value())
		{
			continue;
		}
		ImportedTemple temple {.player = *player, .citadel = citadel->number};
		temple.heart = world.Ref(*citadel, "heart").value_or(0);
		if (const auto* heart = temple.heart != 0 ? world.Record(temple.heart) : nullptr;
		    heart != nullptr && heart->Is(gamesave::VanillaType::CitadelHeart))
		{
			temple.built = world.Float(*heart, "built").value_or(1.0f);
			temple.life = world.Float(*heart, "life").value_or(1.0f);
			const auto site = world.Ref(*heart, "buildingSite").value_or(0);
			if (const auto* record = site != 0 ? world.Record(site) : nullptr;
			    record != nullptr && record->Is(gamesave::VanillaType::CitadelBuildingSite))
			{
				temple.site = site;
			}
			temple.lastHitTurn = world.Word(*heart, "heartLastHitTurn").value_or(0);
			temple.town = world.Ref(*heart, "heartTown").value_or(0);
			temple.destroying = world.Byte(*heart, "heartDestroying").value_or(0) != 0;
			temple.destructionClock = world.Float(*heart, "heartDestructionClock").value_or(0.0f);
			temple.destructionBeamClock = world.Float(*heart, "heartDestructionBeamClock").value_or(0.0f);
			temple.beamTarget = world.Ref(*heart, "heartBeamTarget").value_or(0);
			temple.beamTurn = world.Word(*heart, "heartBeamTurn").value_or(0);
		}
		temples.push_back(temple);
	}
	return temples;
}

std::optional<FileTable> vanilla_script_import::ReadScriptTable(const gamesave::VanillaWorld& world)
{
	if (world.Records().empty())
	{
		return std::nullopt;
	}
	const auto& game = world.Game();
	const auto objects = world.Fields(game, "scriptObject");
	const auto made = world.Fields(game, "scriptObjectMade");
	const auto counts = world.Fields(game, "scriptObjectCount");
	const auto next = world.Word(game, "scriptNextPlace");
	// The file keeps every place but the first
	constexpr size_t k_FilePlaces = script_objects::k_Places - 1;
	if (objects.size() != k_FilePlaces || made.size() != k_FilePlaces || counts.size() != k_FilePlaces || !next.has_value())
	{
		return std::nullopt;
	}
	FileTable table;
	for (size_t i = 0; i < k_FilePlaces; ++i)
	{
		table.places.at(i + 1) = {.number = objects[i]->number,
		                          .createdByScript = ValueOf<uint32_t>(world, *made[i]) != 0,
		                          .count = ValueOf<uint8_t>(world, *counts[i])};
	}
	table.next = static_cast<uint16_t>(*next);
	return table;
}

ImportedTable vanilla_script_import::ImportTable(const FileTable& file, const EntityOfNumber& entityOf)
{
	ImportedTable imported;
	for (uint16_t place = 1; place < script_objects::k_Places; ++place)
	{
		const auto& entry = file.places.at(place);
		if (entry.number == 0)
		{
			// A place with references but no object stays taken until the references go, as in the game. A place with
			// neither is free.
			if (entry.count != 0)
			{
				imported.table.Put(place, {.object = 0, .createdByScript = false, .count = entry.count, .used = true});
				++imported.gone;
			}
			continue;
		}
		const auto entity = entityOf(entry.number);
		if (!entity.has_value())
		{
			imported.lost.push_back(entry.number);
			continue;
		}
		imported.table.Put(place,
		                   {.object = *entity, .createdByScript = entry.createdByScript, .count = entry.count, .used = true});
		++imported.kept;
	}
	imported.table.SetNext(file.next);
	return imported;
}

ImportedScripts vanilla_script_import::ImportScripts(const lhvm::LHVMFile& file, const script_objects::Table& table)
{
	ImportedScripts counts {.file = {}, .objects = 0, .lost = 0};
	auto globals = file.GetVariablesValues();
	ImportValues(globals, table, counts);
	auto stack = file.GetStack();
	ImportStack(stack, table, counts);
	auto tasks = file.GetTasks();
	for (auto& task : tasks)
	{
		ImportValues(task.localVars, table, counts);
		ImportStack(task.stack, table, counts);
	}
	counts.file =
	    lhvm::LHVMFile(file.GetVersion(), file.GetVariablesNames(), file.GetInstructions(), file.GetAutostart(),
	                   file.GetScripts(), file.GetData(), stack, globals, tasks, file.GetTicks(), file.GetCurrentLineNumber(),
	                   file.GetHighestTaskId(), file.GetHighestScriptId(), file.GetExecutedInstructions());
	return counts;
}
