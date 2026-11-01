/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVMFile.h>
#include <VanillaWorld.h>
#include <gtest/gtest.h>

#include "ECS/VanillaScriptImport.h"

using namespace openblack;
using namespace openblack::gamesave;
namespace script_import = openblack::ecs::vanilla_script_import;
namespace script_objects = openblack::ecs::script_objects;

namespace
{

/// A game object with a table of script objects, as the reader keeps it: every place of the file, then the place where
/// the next search starts
class FakeTable
{
public:
	FakeTable() { _game.type = static_cast<uint32_t>(VanillaType::Game); }

	/// Sets the object, the made flag and the count of a place of the file (1 to 511)
	FakeTable& Place(uint16_t place, uint32_t number, bool made, uint8_t count)
	{
		_places.at(place) = {.number = number, .createdByScript = made, .count = count};
		return *this;
	}

	/// Adds a named word after the table
	FakeTable& Word(std::string_view name, uint32_t value)
	{
		_words.emplace_back(name, value);
		return *this;
	}

	[[nodiscard]] VanillaWorld Make(uint32_t next) const
	{
		std::vector<uint8_t> bytes;
		VanillaRecord game = _game;
		game.number = 1;
		const auto put = [&bytes](const void* value, size_t size) {
			const auto at = bytes.size();
			bytes.resize(at + size);
			std::memcpy(bytes.data() + at, value, size);
			return at;
		};
		for (uint16_t place = 1; place < script_objects::k_Places; ++place)
		{
			const auto& entry = _places.at(place);
			game.fields.push_back({.name = "scriptObject", .kind = VanillaValue::Reference, .number = entry.number});
			const uint32_t made = entry.createdByScript ? 1 : 0;
			game.fields.push_back({.name = "scriptObjectMade", .at = put(&made, sizeof(made)), .size = sizeof(made)});
			game.fields.push_back(
			    {.name = "scriptObjectCount", .at = put(&entry.count, sizeof(entry.count)), .size = sizeof(entry.count)});
		}
		game.fields.push_back({.name = "scriptNextPlace", .at = put(&next, sizeof(next)), .size = sizeof(next)});
		for (const auto& [name, value] : _words)
		{
			game.fields.push_back({.name = name, .at = put(&value, sizeof(value)), .size = sizeof(value)});
		}
		return {bytes, VanillaFraming {}, {game}, {}};
	}

private:
	VanillaRecord _game;
	std::array<script_import::FilePlace, script_objects::k_Places> _places {};
	std::vector<std::pair<std::string_view, uint32_t>> _words;
};

/// A script file with a global, a main stack and one task that each hold values
lhvm::LHVMFile ScriptFile(std::vector<lhvm::VMVar> globals, lhvm::VMStack stack, lhvm::VMTask task)
{
	return {lhvm::LHVMVersion::BlackAndWhite,
	        {"a", "b"},
	        {},
	        {},
	        {},
	        {},
	        stack,
	        std::move(globals),
	        {std::move(task)},
	        7,
	        3,
	        9,
	        4,
	        1000};
}

lhvm::VMStack StackOf(std::vector<std::pair<lhvm::DataType, uint32_t>> values)
{
	lhvm::VMStack stack;
	for (const auto& [type, value] : values)
	{
		stack.types.at(stack.count) = type;
		stack.values.at(stack.count) = lhvm::VMValue(value);
		++stack.count;
	}
	return stack;
}

} // namespace

// Each place of the file keeps its object, whether a script made it and its count of references, and the table keeps
// where the next search for a free place starts
TEST(VanillaScriptImport, ReadsTheTableOfTheScriptObjects)
{
	FakeTable fake;
	fake.Place(1, 40, true, 2).Place(7, 41, false, 0).Place(511, 0, false, 3);
	const auto table = script_import::ReadScriptTable(fake.Make(8));
	ASSERT_TRUE(table.has_value());
	EXPECT_EQ(table->next, 8U);
	EXPECT_EQ(table->places[1].number, 40U);
	EXPECT_TRUE(table->places[1].createdByScript);
	EXPECT_EQ(table->places[1].count, 2U);
	EXPECT_EQ(table->places[7].number, 41U);
	EXPECT_FALSE(table->places[7].createdByScript);
	EXPECT_EQ(table->places[511].number, 0U);
	EXPECT_EQ(table->places[511].count, 3U);
	EXPECT_EQ(table->places[2].number, 0U);

	// A game object with no table
	const VanillaWorld empty({}, VanillaFraming {}, {VanillaRecord {.number = 1}}, {});
	EXPECT_FALSE(script_import::ReadScriptTable(empty).has_value());
}

// The values after the table give the level of the interface, the entry to the temple and the countdown timer of the
// scripts. A game object without all of them gives no values.
TEST(VanillaScriptImport, ReadsTheValuesOfTheScriptsAfterTheTable)
{
	FakeTable fake;
	fake.Word("scriptCountdownOn", 1)
	    .Word("scriptCountdownLeft", 90)
	    .Word("scriptCountdownShown", 1)
	    .Word("scriptTempleEntry", 0)
	    .Word("scriptInterfaceLevel", 12);
	const auto values = script_import::ReadScriptValues(fake.Make(1));
	ASSERT_TRUE(values.has_value());
	EXPECT_EQ(values->interfaceLevel, 12);
	EXPECT_FALSE(values->templeEntry);
	EXPECT_TRUE(values->countdownOn);
	EXPECT_EQ(values->countdownLeft, 90U);
	EXPECT_TRUE(values->countdownShown);

	EXPECT_FALSE(script_import::ReadScriptValues(FakeTable().Make(1)).has_value());
}

// An object that openblack made takes its place as its entity. A place of an object that openblack did not make is
// free, and a place with references to an object that went stays taken.
TEST(VanillaScriptImport, PutsTheEntitiesIntoThePlacesOfTheFile)
{
	FakeTable fake;
	fake.Place(1, 40, true, 2).Place(7, 41, false, 0).Place(9, 42, false, 1).Place(20, 0, false, 3);
	const auto file = script_import::ReadScriptTable(fake.Make(12));
	ASSERT_TRUE(file.has_value());
	const auto imported = script_import::ImportTable(*file, [](uint32_t number) -> std::optional<uint32_t> {
		if (number == 42)
		{
			return std::nullopt;
		}
		return number + 100;
	});
	EXPECT_EQ(imported.kept, 2U);
	EXPECT_EQ(imported.gone, 1U);
	ASSERT_EQ(imported.lost.size(), 1U);
	EXPECT_EQ(imported.lost[0], 42U);
	EXPECT_EQ(imported.table.Next(), 12U);

	const auto& made = imported.table.At(1);
	EXPECT_TRUE(made.used);
	EXPECT_EQ(made.object, 140U);
	EXPECT_TRUE(made.createdByScript);
	EXPECT_EQ(made.count, 2U);
	EXPECT_EQ(imported.table.At(7).object, 141U);
	EXPECT_EQ(imported.table.At(7).count, 0U);
	EXPECT_FALSE(imported.table.At(9).used);
	EXPECT_TRUE(imported.table.At(20).used);
	EXPECT_EQ(imported.table.At(20).object, 0U);
	EXPECT_EQ(imported.table.At(20).count, 3U);
	EXPECT_FALSE(imported.table.At(2).used);
	// The place of an object with no references is free again after the next turn of the scripts
	EXPECT_EQ(imported.table.Unreferenced(), std::vector<uint16_t>({7}));
}

// Only the values that hold an object change: a place becomes the entity in it, and a place with no entity becomes no
// object. Numbers, floats and the other values stay.
TEST(VanillaScriptImport, ChangesTheObjectValuesOfTheScriptsIntoEntities)
{
	script_objects::Table table;
	table.Put(3, {.object = 77, .createdByScript = false, .count = 4, .used = true});
	table.Put(5, {.object = 0, .createdByScript = false, .count = 1, .used = true});

	using lhvm::DataType;
	const std::vector<lhvm::VMVar> globals = {{DataType::Float, lhvm::VMValue(0.0f), "Null variable"},
	                                          {DataType::Object, lhvm::VMValue(3U), "a"},
	                                          {DataType::Int, lhvm::VMValue(3U), "b"}};
	lhvm::VMTask task;
	task.id = 9;
	task.localVars = {{DataType::Object, lhvm::VMValue(5U), "local"}, {DataType::Object, lhvm::VMValue(0U), "none"}};
	task.stack = StackOf({{DataType::Object, 3}, {DataType::Object, 600}});
	const auto file = ScriptFile(globals, StackOf({{DataType::Float, 3}, {DataType::Object, 3}}), task);

	const auto imported = script_import::ImportScripts(file, table);
	EXPECT_EQ(imported.objects, 5U);
	EXPECT_EQ(imported.lost, 2U);
	const auto& values = imported.file.GetVariablesValues();
	ASSERT_EQ(values.size(), 3U);
	EXPECT_EQ(values[1].value.uintVal, 77U);
	EXPECT_EQ(values[2].value.uintVal, 3U);
	EXPECT_EQ(imported.file.GetStack().values[0].uintVal, 3U);
	EXPECT_EQ(imported.file.GetStack().values[1].uintVal, 77U);
	ASSERT_EQ(imported.file.GetTasks().size(), 1U);
	const auto& importedTask = imported.file.GetTasks().front();
	EXPECT_EQ(importedTask.localVars[0].value.uintVal, 0U);
	EXPECT_EQ(importedTask.localVars[1].value.uintVal, 0U);
	EXPECT_EQ(importedTask.stack.values[0].uintVal, 77U);
	EXPECT_EQ(importedTask.stack.values[1].uintVal, 0U);
	// The rest of the running state stays as it is
	EXPECT_TRUE(imported.file.HasStatus());
	EXPECT_EQ(imported.file.GetTicks(), 7U);
	EXPECT_EQ(imported.file.GetCurrentLineNumber(), 3U);
	EXPECT_EQ(imported.file.GetHighestTaskId(), 9U);
	EXPECT_EQ(imported.file.GetHighestScriptId(), 4U);
	EXPECT_EQ(imported.file.GetExecutedInstructions(), 1000U);
}

/// The table and the script file of each saved game of Land 1 in the copy of a Profiles folder that
/// OPENBLACK_VANILLA_SAVES gives. Each object value of the scripts is a place of the table that holds an object, or a
/// place whose object went.
TEST(VanillaScriptImport, SavedGamesOfLandOneHaveTheirScriptObjectsInTheTable)
{
	const char* saves = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	std::error_code error;
	if (saves == nullptr || !std::filesystem::is_directory(saves, error))
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	const auto readAll = [](const std::filesystem::path& path) {
		std::ifstream stream(path, std::ios::binary);
		return std::vector<uint8_t> {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
	};
	uint32_t files = 0;
	std::map<uint32_t, uint32_t> types;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(saves))
	{
		const auto name = entry.path().filename().string();
		if (!entry.is_regular_file() || entry.path().extension() != ".sav" || name.starts_with("Script"))
		{
			continue;
		}
		const auto read = ReadVanillaWorld(readAll(entry.path()));
		if (!read.world.has_value() || read.world->Framing().landscape.find("Land1.lnd") == std::string::npos)
		{
			continue;
		}
		const auto table = script_import::ReadScriptTable(*read.world);
		ASSERT_TRUE(table.has_value()) << entry.path();
		// No script of these saves has a countdown timer or a lower level of the interface, and the temple lets the
		// player in
		const auto scriptValues = script_import::ReadScriptValues(*read.world);
		ASSERT_TRUE(scriptValues.has_value()) << entry.path();
		EXPECT_EQ(scriptValues->interfaceLevel, 0) << entry.path();
		EXPECT_TRUE(scriptValues->templeEntry) << entry.path();
		EXPECT_FALSE(scriptValues->countdownOn) << entry.path();
		// Every object the file has goes into the table as itself
		const auto imported = script_import::ImportTable(*table, [](uint32_t number) { return std::optional(number); });
		EXPECT_TRUE(imported.lost.empty()) << entry.path();
		for (uint16_t place = 1; place < script_objects::k_Places; ++place)
		{
			const auto number = table->places.at(place).number;
			if (number == 0)
			{
				continue;
			}
			const auto* record = read.world->Record(number);
			ASSERT_NE(record, nullptr) << entry.path() << " place " << place;
			++types[record->type];
		}
		// The heart of each temple belongs to the owner of the temple
		for (const auto* heart : read.world->OfType(VanillaType::CitadelHeart))
		{
			EXPECT_TRUE(script_import::HeartPlayer(*read.world, heart->number).has_value()) << entry.path();
		}
		const auto things = script_import::ReadScriptThings(*read.world);
		EXPECT_TRUE(things.skipped.empty()) << entry.path();
		EXPECT_EQ(things.markers.size(), read.world->OfType(VanillaType::ScriptMarker).size()) << entry.path();
		EXPECT_EQ(things.highlights.size(), read.world->OfType(VanillaType::ScriptHighlight).size()) << entry.path();
		EXPECT_EQ(things.rings.size(), read.world->OfType(VanillaType::InfluenceRing).size()) << entry.path();
		const auto scriptPath = entry.path().parent_path() / ("Script_" + entry.path().filename().string());
		ASSERT_TRUE(std::filesystem::is_regular_file(scriptPath)) << scriptPath;
		lhvm::LHVMFile scripts;
		scripts.Open(readAll(scriptPath));
		ASSERT_TRUE(scripts.HasStatus()) << scriptPath;
		const auto values = script_import::ImportScripts(scripts, imported.table);
		// Each object value of the scripts finds its object in the table
		EXPECT_EQ(values.lost, 0U) << entry.path();
		EXPECT_GT(values.objects, 0U) << entry.path();
		std::cout << entry.path().generic_string() << ": " << imported.kept << " objects, " << imported.gone << " gone, "
		          << scripts.GetTasks().size() << " tasks, " << values.objects << " object values, " << values.lost
		          << " with no object" << std::endl;
		++files;
	}
	// The scripts of Land 1 hold markers, timers, highlights, influences and flocks
	for (const auto type : {VanillaType::ScriptMarker, VanillaType::ScriptTimer, VanillaType::ScriptHighlight,
	                        VanillaType::InfluenceRing, VanillaType::Flock})
	{
		EXPECT_GT(types[static_cast<uint32_t>(type)], 0U);
	}
	EXPECT_GT(files, 0U);
}
