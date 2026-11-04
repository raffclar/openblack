/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <utility>

#include <SaveSlots.h>

#include "Debug/DebugGuiInterface.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameProviderShared.h"
#include "Gui/ScreenFade.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"
#include "Online/NetworkSession.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;
using namespace openblack::inspector::game_providers;

namespace
{

/// The number of the open land, which the saved games use for their land folders
uint32_t OpenLand()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0;
	}
	return static_cast<uint32_t>(std::max(Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber, 0));
}

} // namespace

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> SavesProvider()
{
	auto provider = std::make_unique<FunctionProvider>("saves");
	provider->Add(Query("slots", "The slots of the current player that have a saved game, for the land that is open", {},
	                    ResultKind::List),
	              [](const QueryContext& /*c*/) {
		              if (!Locator::saveGameSystem::has_value())
		              {
			              return QueryResult::Error("there are no saved games without a game");
		              }
		              Json items = Json::array();
		              for (const auto& slot : Locator::saveGameSystem::value().Slots())
		              {
			              items.push_back({{"slot", slot.slot},
			                               {"land", slot.land},
			                               {"date", gui::ToUtf8(slot.date)},
			                               {"name", gui::ToUtf8(slot.name)}});
		              }
		              return QueryResult::Value(std::move(items));
	              });
	const ParameterDescription slotParameter {
	    .name = "slot", .type = "integer", .description = "The slot, 0 to 20", .required = true};
	auto save =
	    Query("save",
	          "Saves the game into a slot: the whole world (every entity and its components, the clock, the random "
	          "numbers, the camera and the scripts) and every creature with its mind file and its leash. The slots 14 to 18 "
	          "save into a folder of the open land in the folder of the slot. The result gives the folder of the world "
	          "file in the folder of the player",
	          {slotParameter, {.name = "name", .type = "string", .description = "The name of the save", .required = false}});
	save.writes = true;
	provider->Add(std::move(save), [](const QueryContext& context) {
		const auto slot = NumberMember(context.params, "slot");
		if (!Locator::saveGameSystem::has_value() || !slot.has_value() || *slot < 0.0)
		{
			return QueryResult::Error("saves.save needs a game and a slot from 0 to 20");
		}
		const auto name = gui::ToUtf16(StringMember(context.params, "name").value_or(""));
		const auto number = static_cast<uint32_t>(*slot);
		const bool saved = Locator::saveGameSystem::value().Save(number, name);
		return QueryResult::Value(
		    {{"saved", saved}, {"slot", number}, {"folder", gamesave::SlotFolder(number, OpenLand()).generic_string()}});
	});
	auto load = Query("load",
	                  "Loads a slot. A save of the whole world loads its land and its world in place of the open land. An "
	                  "older save puts the creatures of the land that is open back: the values of each record, the mind of "
	                  "its mind file, the plan that the save kept and its leash. Slot 14 (the previous land) loads from the "
	                  "folder of the land before the open land, and slot 18 (the next land) from the folder of the land "
	                  "after it, as the save game room does. The result gives the folder that the load read",
	                  {slotParameter});
	load.writes = true;
	provider->Add(std::move(load), [](const QueryContext& context) {
		const auto slot = NumberMember(context.params, "slot");
		if (!Locator::saveGameSystem::has_value() || !slot.has_value() || *slot < 0.0)
		{
			return QueryResult::Error("saves.load needs a game and a slot from 0 to 20");
		}
		const auto number = static_cast<uint32_t>(*slot);
		// Find the folder before the load, because a load can open another land
		const auto folder = gamesave::LoadSlotFolder(number, OpenLand()).generic_string();
		const bool loaded = Locator::saveGameSystem::value().Load(number);
		return QueryResult::Value({{"loaded", loaded}, {"slot", number}, {"folder", folder}});
	});
	auto loadAndSave =
	    Query("load_and_save",
	          "Loads a slot and then saves the game into a second slot in the same call, with no frame and no turn between. A "
	          "save of a loaded world must have the same bytes as the save that the game loaded (tools/sim/save_diff.py "
	          "compares the two files)",
	          {slotParameter,
	           {.name = "into", .type = "integer", .description = "The slot of the second save, 0 to 20", .required = true},
	           {.name = "name", .type = "string", .description = "The name of the second save", .required = false}});
	loadAndSave.writes = true;
	provider->Add(std::move(loadAndSave), [](const QueryContext& context) {
		const auto slot = NumberMember(context.params, "slot");
		const auto into = NumberMember(context.params, "into");
		if (!Locator::saveGameSystem::has_value() || !slot.has_value() || *slot < 0.0 || !into.has_value() || *into < 0.0)
		{
			return QueryResult::Error("saves.load_and_save needs a game, a slot and an into slot from 0 to 20");
		}
		auto& saves = Locator::saveGameSystem::value();
		const bool loaded = saves.Load(static_cast<uint32_t>(*slot));
		const auto name = gui::ToUtf16(StringMember(context.params, "name").value_or(""));
		const bool saved = loaded && saves.Save(static_cast<uint32_t>(*into), name);
		return QueryResult::Value({{"loaded", loaded},
		                           {"saved", saved},
		                           {"slot", static_cast<uint32_t>(*slot)},
		                           {"into", static_cast<uint32_t>(*into)}});
	});
	auto worldStateRoundTrip =
	    Query("world_state_round_trip",
	          "Makes the world state that a game server sends, loads it at once and makes it again, with no frame and no turn "
	          "between. Writes the two states to the files first and again. The two must have the same bytes "
	          "(tools/sim/save_diff.py compares the two files)",
	          {{.name = "first", .type = "string", .description = "The file of the first state", .required = true},
	           {.name = "again", .type = "string", .description = "The file of the state after the load", .required = true}});
	worldStateRoundTrip.writes = true;
	provider->Add(std::move(worldStateRoundTrip), [](const QueryContext& context) {
		const auto firstPath = StringMember(context.params, "first");
		const auto againPath = StringMember(context.params, "again");
		if (!Locator::saveGameSystem::has_value() || !Locator::filesystem::has_value() || !firstPath.has_value() ||
		    !againPath.has_value())
		{
			return QueryResult::Error("saves.world_state_round_trip needs a game and the two files");
		}
		auto& saves = Locator::saveGameSystem::value();
		auto& files = Locator::filesystem::value();
		const auto first = saves.WorldState();
		const bool loaded = !first.empty() && saves.LoadWorldState(first);
		const auto again = loaded ? saves.WorldState() : std::vector<uint8_t> {};
		const bool written = files.WriteAll(std::filesystem::path(*firstPath), first) &&
		                     (!loaded || files.WriteAll(std::filesystem::path(*againPath), again));
		return QueryResult::Value({{"loaded", loaded},
		                           {"written", written},
		                           {"same", loaded && first == again},
		                           {"bytes", {first.size(), again.size()}}});
	});
	provider->Add(Query("vanilla_check",
	                    "After a load of a slot of the game itself: each creature of the game's file, and whether the record "
	                    "that a save writes for it now has the same bytes as the file's record",
	                    {}, ResultKind::List),
	              [](const QueryContext& /*c*/) {
		              if (!Locator::saveGameSystem::has_value())
		              {
			              return QueryResult::Error("there are no saved games without a game");
		              }
		              Json items = Json::array();
		              for (const auto& check : Locator::saveGameSystem::value().CheckVanillaRecords())
		              {
			              Json item {
			                  {"number", check.number}, {"entity", check.entity}, {"same", check.same}, {"size", check.size}};
			              if (check.firstDifference.has_value())
			              {
				              item["firstDifference"] = *check.firstDifference;
			              }
			              items.push_back(std::move(item));
		              }
		              return QueryResult::Value(std::move(items));
	              });
	return provider;
}

} // namespace openblack::inspector::game_providers
