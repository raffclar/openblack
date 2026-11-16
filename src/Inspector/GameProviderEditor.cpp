/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <bit>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <GameSocket.h>
#include <LHVM.h>
#include <MindFile.h>
#include <fmt/format.h>
#include <glm/vec2.hpp>

#include "Common/CreatureEditValues.h"
#include "Common/DebugCheats.h"
#include "Common/DeveloperToolsGate.h"
#include "Creature/CreatureMindFileBody.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "Editor/EditorActions.h"
#include "Editor/EditorCreatureActs.h"
#include "Editor/EditorCreatureEdits.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorLandEdits.h"
#include "Editor/EditorMindLoads.h"
#include "Editor/EditorNetworkEdits.h"
#include "Editor/EditorScriptActs.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameProviderShared.h"
#include "Locator.h"
#include "Story/StoryLandSkip.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::inspector::game_providers;
using player_commands::EditorEdit;

namespace
{

/// The value of a boolean parameter, or the fallback when the request does not give it
[[nodiscard]] bool BoolParam(const Json& params, std::string_view name, bool fallback)
{
	const auto found = params.find(std::string(name));
	return found != params.end() && found->is_boolean() ? found->get<bool>() : fallback;
}

/// The value of a number parameter, or none
[[nodiscard]] std::optional<float> NumberParam(const Json& params, std::string_view name)
{
	const auto found = params.find(std::string(name));
	return found != params.end() && found->is_number() ? std::optional(found->get<float>()) : std::nullopt;
}

/// A tool can send a command that the editor of this computer would not send, so that a test sees the refusal of the
/// game server. Else the editor's own rule decides.
[[nodiscard]] std::optional<QueryResult> Refused(const Json& params)
{
	if (BoolParam(params, "force", false) || developer_tools::EditorAllowed())
	{
		return std::nullopt;
	}
	return QueryResult::Error("the editor is refused: only a player with the editor right opens the editor");
}

} // namespace

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> EditorProvider()
{
	auto provider = std::make_unique<FunctionProvider>("editor");
	provider->Add(Query("state", "The editor of this computer: whether it is open, whether a network game is live, whether "
	                             "the player of this computer has the editor right (from the join ticket in a network game, "
	                             "else from the signed-in account: granted, refused or unknown), what the debug windows "
	                             "show, and the debug cheats of that player"),
	              [](const QueryContext& /*context*/) {
		              Json cheats = Json::array();
		              const auto own = editor::OwnDebugCheats();
		              for (size_t index = 0; index < debug_cheats::k_CheatCount; ++index)
		              {
			              const auto cheat = static_cast<debug_cheats::Cheat>(index);
			              if (debug_cheats::Has(own, cheat))
			              {
				              cheats.push_back(debug_cheats::NameOf(cheat));
			              }
		              }
		              return QueryResult::Value({
		                  {"open", Locator::editorSystem::has_value() && Locator::editorSystem::value().IsOpen()},
		                  {"network", developer_tools::NetworkSessionLive()},
		                  {"editor", developer_tools::LocalEditorRight() == developer_tools::EditorRight::Granted},
		                  {"right", std::string(developer_tools::NameOf(developer_tools::LocalEditorRight()))},
		                  {"mode", std::string(developer_tools::NameOf(developer_tools::CurrentGuiMode()))},
		                  {"cheats", std::move(cheats)},
		              });
	              });

	provider->Add(Query("show_tab",
	                    "Opens the editor of this computer on a tab (Palette, Cheats, Log, ...), as the player does. It "
	                    "changes only the windows of this computer, and the editor opens only when its rule lets it.",
	                    {{.name = "tab", .type = "string", .description = "The name of the tab", .required = true}}),
	              [](const QueryContext& context) {
		              const auto tab = context.params.find("tab");
		              if (tab == context.params.end() || !tab->is_string())
		              {
			              return QueryResult::Error("editor.show_tab needs the name of a tab");
		              }
		              if (!Locator::debugGui::has_value())
		              {
			              return QueryResult::Error("there are no debug windows");
		              }
		              Locator::debugGui::value().OpenWindow(std::string(debug::gui::k_EditorWindow) + "/" +
		                                                    tab->get<std::string>());
		              return QueryResult::Value(
		                  {{"open", Locator::editorSystem::has_value() && Locator::editorSystem::value().IsOpen()}});
	              });

	auto edit = Query(
	    "network_edit",
	    "Sends an edit of the editor as a command of the turns, as the editor of a network game sends it: each computer "
	    "does it at the same turn, and the game server takes it only from a player with the editor claim. force sends it also "
	    "when the "
	    "editor of this computer is refused, so that a test sees the refusal of the server.",
	    {{.name = "kind", .type = "string", .description = "move, turn, scale, place or remove", .required = true},
	     {.name = "id",
	      .type = "integer",
	      .description = "The thing of a move, a turn, a scale or a removal",
	      .required = false},
	     {.name = "position",
	      .type = "point",
	      .description = "[x, z] or [x, y, z] of a move or a placement",
	      .required = false},
	     {.name = "radians", .type = "number", .description = "The angle of a turn or a placement", .required = false},
	     {.name = "scale", .type = "number", .description = "The new scale of the first axis (scale)", .required = false},
	     {.name = "place_kind", .type = "integer", .description = "The kind of the palette item (place)", .required = false},
	     {.name = "place_type", .type = "integer", .description = "The type of the palette item (place)", .required = false},
	     {.name = "copies",
	      .type = "integer",
	      .description = "Placements in a row from the position, all in one frame (place; 1 when not given)",
	      .required = false},
	     {.name = "step", .type = "point", .description = "[x, z] from each placement to the next (place)", .required = false},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	edit.writes = true;
	provider->Add(std::move(edit), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		if (!Locator::playerCommandSystem::has_value())
		{
			return QueryResult::Error("there is no game to send the edit to");
		}
		const auto kindParam = context.params.find("kind");
		const auto kindName =
		    kindParam != context.params.end() && kindParam->is_string() ? kindParam->get<std::string>() : std::string();
		constexpr std::array<std::pair<std::string_view, EditorEdit::Kind>, 5> k_Kinds {{
		    {"place", EditorEdit::Kind::Place},
		    {"move", EditorEdit::Kind::Move},
		    {"turn", EditorEdit::Kind::Turn},
		    {"remove", EditorEdit::Kind::Remove},
		    {"scale", EditorEdit::Kind::Scale},
		}};
		const auto kind =
		    std::ranges::find(k_Kinds, std::string_view(kindName), &std::pair<std::string_view, EditorEdit::Kind>::first);
		if (kind == k_Kinds.end())
		{
			return QueryResult::Error("editor.network_edit needs a kind: move, turn, scale, place or remove");
		}
		EditorEdit command {.kind = kind->second};
		if (command.kind != EditorEdit::Kind::Place)
		{
			const auto entity = EntityParam(context.params);
			if (!entity.has_value())
			{
				return QueryResult::Error("editor.network_edit needs the id of a thing");
			}
			command.object = *entity;
		}
		if (const auto position = PositionParam(context.params))
		{
			command.position = *position;
			// A point of [x, z] stands on the land, as the editor puts a thing that it moves
			if (const auto found = context.params.find("position"); found->is_array() && found->size() == 2)
			{
				command.position.y = editor::LandHeight({position->x, position->z});
			}
		}
		command.radians = NumberParam(context.params, "radians").value_or(0.0f);
		command.scale = NumberParam(context.params, "scale").value_or(1.0f);
		command.placeKind = static_cast<uint8_t>(NumberParam(context.params, "place_kind").value_or(0.0f));
		command.placeType = static_cast<int32_t>(NumberParam(context.params, "place_type").value_or(0.0f));
		if (const auto refusal = editor::EditRefusal(command))
		{
			return QueryResult::Error(std::string(*refusal));
		}
		// A tool sends a row of placements in one frame, so that they go in the same turn
		const auto count = command.kind == EditorEdit::Kind::Place
		                       ? std::clamp(static_cast<int>(NumberParam(context.params, "copies").value_or(1.0f)), 1, 256)
		                       : 1;
		glm::vec2 step {0.0f};
		if (const auto found = context.params.find("step"); found != context.params.end() && found->is_array() &&
		                                                    found->size() == 2 && (*found)[0].is_number() &&
		                                                    (*found)[1].is_number())
		{
			step = {(*found)[0].get<float>(), (*found)[1].get<float>()};
		}
		const auto start = command.position;
		for (int index = 0; index < count; ++index)
		{
			if (index > 0)
			{
				const glm::vec2 point {start.x + step.x * static_cast<float>(index),
				                       start.z + step.y * static_cast<float>(index)};
				command.position = {point.x, editor::LandHeight(point), point.y};
			}
			Locator::playerCommandSystem::value().Submit(command);
		}
		return QueryResult::Value({{"sent", count}});
	});

	auto act = Query(
	    "act",
	    "Does an action of the editor on things, as the tool bar and the keys of the editor do it, with the history of the "
	    "editor: delete, duplicate, place, move, turn, undo or redo. In a network game each change goes as a command of the "
	    "turns, and the history learns it from the answer at the turn. force acts also when the editor of this computer is "
	    "refused.",
	    {{.name = "act",
	      .type = "string",
	      .description = "delete, duplicate, place, move, turn, undo or redo",
	      .required = true},
	     {.name = "ids",
	      .type = "array",
	      .description = "The things to pick before the act (delete, duplicate, move, turn)",
	      .required = false},
	     {.name = "offset", .type = "point", .description = "[x, z] that a move adds to each thing", .required = false},
	     {.name = "radians", .type = "number", .description = "The angle of a turn", .required = false},
	     {.name = "position", .type = "point", .description = "[x, z] of a placement", .required = false},
	     {.name = "place_kind", .type = "integer", .description = "The kind of the palette item (place)", .required = false},
	     {.name = "place_type", .type = "integer", .description = "The type of the palette item (place)", .required = false},
	     {.name = "force", .type = "boolean", .description = "Act also when the editor is refused", .required = false}});
	act.writes = true;
	provider->Add(std::move(act), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		if (!Locator::editorSystem::has_value() || !Locator::entitiesRegistry::has_value())
		{
			return QueryResult::Error("there is no editor");
		}
		const auto actParam = context.params.find("act");
		const auto name = actParam != context.params.end() && actParam->is_string() ? actParam->get<std::string>() : "";
		auto& system = Locator::editorSystem::value();
		auto& registry = Locator::entitiesRegistry::value();
		// The act uses writes of its own, as the editor window does: a network game sends commands
		std::unique_ptr<editor::EditorWrites> writes;
		if (developer_tools::NetworkSessionLive())
		{
			writes = editor::MakeNetworkWrites();
		}
		else
		{
			writes = std::make_unique<editor::LocalWrites>();
		}
		const editor::EditorActs acts {.system = system, .registry = registry, .writes = *writes};
		std::vector<entt::entity> picks;
		if (const auto ids = context.params.find("ids"); ids != context.params.end() && ids->is_array())
		{
			for (const auto& id : *ids)
			{
				if (id.is_number_integer() && registry.Valid(static_cast<entt::entity>(id.get<uint32_t>())))
				{
					picks.push_back(static_cast<entt::entity>(id.get<uint32_t>()));
				}
			}
			system.GetSelection().SelectAll(picks);
		}
		std::string note;
		if (name == "delete")
		{
			editor::DeletePicked(acts);
		}
		else if (name == "duplicate")
		{
			editor::DuplicatePicked(acts);
		}
		else if (name == "undo")
		{
			note = editor::UndoStep(acts);
		}
		else if (name == "redo")
		{
			note = editor::RedoStep(acts);
		}
		else if (name == "move" || name == "turn")
		{
			editor::PoseEdit edit;
			edit.Begin(registry, picks);
			glm::vec2 offset {0.0f};
			if (const auto found = context.params.find("offset"); found != context.params.end() && found->is_array() &&
			                                                      found->size() == 2 && (*found)[0].is_number() &&
			                                                      (*found)[1].is_number())
			{
				offset = {(*found)[0].get<float>(), (*found)[1].get<float>()};
			}
			const auto radians = NumberParam(context.params, "radians").value_or(0.0f);
			for (const auto entity : picks)
			{
				const auto pose = editor::PoseOf(registry, entity);
				if (!pose.has_value())
				{
					continue;
				}
				if (name == "move")
				{
					const glm::vec2 point {pose->position.x + offset.x, pose->position.z + offset.y};
					writes->MoveTo(entity, {point.x, editor::LandHeight(point), point.y});
				}
				else
				{
					writes->Turn(entity, radians);
				}
			}
			edit.Finish(acts, name == "move" ? "Move" : "Turn");
		}
		else if (name == "place")
		{
			const auto position = PositionParam(context.params);
			if (!position.has_value())
			{
				return QueryResult::Error("editor.act place needs a position");
			}
			const editor::PlaceItem item {
			    .kind = static_cast<editor::PlaceKind>(NumberParam(context.params, "place_kind").value_or(0.0f)),
			    .type = static_cast<int32_t>(NumberParam(context.params, "place_type").value_or(0.0f))};
			const glm::vec3 point {position->x, editor::LandHeight({position->x, position->z}), position->z};
			if (const auto made = writes->Place(item, point, 0.0f))
			{
				editor::RecordMade(acts, *made, "Place");
			}
			else
			{
				const std::array requests {writes->LastMakeRequest()};
				editor::RecordAllSent(acts, requests, "Place");
			}
		}
		else
		{
			return QueryResult::Error("editor.act needs an act: delete, duplicate, place, move, turn, undo or redo");
		}
		const auto& history = system.GetHistory();
		auto selected = Json::array();
		for (const auto entity : system.GetSelection().All())
		{
			selected.push_back(Id(entity));
		}
		return QueryResult::Value({{"note", note},
		                           {"undo_steps", history.UndoCount()},
		                           {"redo_steps", history.RedoCount()},
		                           {"undo_label", history.UndoLabel().value_or("")},
		                           {"redo_label", history.RedoLabel().value_or("")},
		                           {"waiting", history.Waiting()},
		                           {"selected", std::move(selected)}});
	});

	auto creatureEdit = Query(
	    "creature_edit",
	    "Sends a change of a value of a creature as a command of the turns, as the creature section of the editor sends it "
	    "in a network game: each computer does it at the same turn, and the game server takes it only from a player with the "
	    "editor claim. force sends it also when the editor of this computer is refused.",
	    {{.name = "id", .type = "integer", .description = "The creature", .required = true},
	     {.name = "value",
	      .type = "string",
	      .description = "alignment, fatness, strength, size, energy, exhaustion, thirst, poo, life, warmth, itchiness or "
	                     "grown up",
	      .required = true},
	     {.name = "amount", .type = "number", .description = "The new amount of the value", .required = true},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	creatureEdit.writes = true;
	provider->Add(std::move(creatureEdit), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		const auto entity = EntityParam(context.params);
		const auto nameParam = context.params.find("value");
		const auto name =
		    nameParam != context.params.end() && nameParam->is_string() ? nameParam->get<std::string>() : std::string();
		const auto value = creature_edit_values::ValueNamed(name);
		const auto amount = NumberParam(context.params, "amount");
		if (!entity.has_value() || !value.has_value() || !amount.has_value())
		{
			return QueryResult::Error("editor.creature_edit needs the id of a creature, the name of a value and an amount");
		}
		editor::SendCreatureEdit(*entity, *value, *amount);
		return QueryResult::Value({{"sent", true}});
	});
	auto villagerEdit = Query(
	    "villager_edit",
	    "Sends a change of a field of a villager as a command of the turns, as the villager section of the editor sends it in "
	    "a network game: each computer does it at the same turn, and the game server takes it only from a player with the "
	    "editor claim. force sends it also when the editor of this computer is refused.",
	    {{.name = "id", .type = "integer", .description = "The villager", .required = true},
	     {.name = "field",
	      .type = "string",
	      .description = "life, age, food, life stage, sex, tribe, role or state",
	      .required = true},
	     {.name = "number",
	      .type = "integer",
	      .description = "The age, the life stage, the sex, the tribe, the role or the state",
	      .required = false},
	     {.name = "amount", .type = "number", .description = "The life or the food", .required = false},
	     {.name = "index", .type = "integer", .description = "The slot of states of a state, 0 to 2", .required = false},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	villagerEdit.writes = true;
	provider->Add(std::move(villagerEdit), [](const QueryContext& context) {
		using Field = player_commands::EditorVillagerEdit::Field;
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		constexpr std::array<std::pair<std::string_view, Field>, 8> k_Fields {{
		    {"life", Field::Life},
		    {"age", Field::Age},
		    {"food", Field::Food},
		    {"life stage", Field::LifeStage},
		    {"sex", Field::Sex},
		    {"tribe", Field::Tribe},
		    {"role", Field::Role},
		    {"state", Field::State},
		}};
		const auto fieldParam = context.params.find("field");
		const auto fieldName =
		    fieldParam != context.params.end() && fieldParam->is_string() ? fieldParam->get<std::string>() : std::string();
		const auto field = std::ranges::find(k_Fields, std::string_view(fieldName), &std::pair<std::string_view, Field>::first);
		const auto entity = EntityParam(context.params);
		if (!entity.has_value() || field == k_Fields.end())
		{
			return QueryResult::Error("editor.villager_edit needs the id of a villager and a field");
		}
		const player_commands::EditorVillagerEdit edit {
		    .villager = *entity,
		    .field = field->second,
		    .index = static_cast<uint8_t>(NumberParam(context.params, "index").value_or(0.0f)),
		    .number = static_cast<uint32_t>(NumberParam(context.params, "number").value_or(0.0f)),
		    .amount = NumberParam(context.params, "amount").value_or(0.0f)};
		if (const auto refusal = editor::VillagerEditRefusal(edit))
		{
			return QueryResult::Error(std::string(*refusal));
		}
		editor::SendVillagerEdit(edit);
		return QueryResult::Value({{"sent", true}});
	});
	auto creatureSpawn = Query(
	    "creature_spawn",
	    "Sends a new creature as a command of the turns, as the creature tools send it in a network game: each computer makes "
	    "it at the same turn, and the game server takes it only from a player with the editor claim. force sends it also "
	    "when the editor of this computer is refused.",
	    {{.name = "species", .type = "integer", .description = "The species (CreatureType, from 1)", .required = true},
	     {.name = "position", .type = "point", .description = "[x, z] or [x, y, z] of the creature", .required = true},
	     {.name = "owner", .type = "integer", .description = "The player that owns it (0 when not given)", .required = false},
	     {.name = "radians", .type = "number", .description = "The angle that it faces", .required = false},
	     {.name = "scale", .type = "number", .description = "Its size (1 when not given)", .required = false},
	     {.name = "alignment", .type = "number", .description = "-1 to 1 (0 when not given)", .required = false},
	     {.name = "fatness", .type = "number", .description = "0 to 1 (0.5 when not given)", .required = false},
	     {.name = "strength", .type = "number", .description = "0 to 1 (0.5 when not given)", .required = false},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	creatureSpawn.writes = true;
	provider->Add(std::move(creatureSpawn), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		const auto species = NumberParam(context.params, "species");
		auto position = PositionParam(context.params);
		if (!species.has_value() || !position.has_value())
		{
			return QueryResult::Error("editor.creature_spawn needs a species and a position");
		}
		// A point of [x, z] stands on the land, as the creature tools put a creature
		if (const auto found = context.params.find("position"); found->is_array() && found->size() == 2)
		{
			position->y = editor::LandHeight({position->x, position->z});
		}
		const player_commands::EditorCreatureSpawn spawn {
		    .species = static_cast<uint8_t>(*species),
		    .owner = static_cast<uint8_t>(NumberParam(context.params, "owner").value_or(0.0f)),
		    .position = *position,
		    .radians = NumberParam(context.params, "radians").value_or(0.0f),
		    .scale = NumberParam(context.params, "scale").value_or(1.0f),
		    .alignment = NumberParam(context.params, "alignment").value_or(0.0f),
		    .fatness = NumberParam(context.params, "fatness").value_or(0.5f),
		    .strength = NumberParam(context.params, "strength").value_or(0.5f)};
		if (const auto refusal = editor::CreatureSpawnRefusal(spawn))
		{
			return QueryResult::Error(std::string(*refusal));
		}
		editor::SendCreatureSpawn(spawn);
		return QueryResult::Value({{"sent", true}});
	});
	auto mindLoad = Query(
	    "mind_load",
	    "Sends a mind file of this computer and its command of the turns, as the creature tools send them in a network game: "
	    "the file goes to the game server, which sends it to every computer, and each computer loads it at the same turn. "
	    "With an id, the creature takes the mind. With a position, a new creature with the body of the file takes it. force "
	    "sends it also when the editor of this computer is refused.",
	    {{.name = "path", .type = "string", .description = "The mind file on this computer", .required = true},
	     {.name = "id", .type = "integer", .description = "The creature that takes the mind", .required = false},
	     {.name = "position",
	      .type = "point",
	      .description = "[x, z] or [x, y, z] of a new creature from the file",
	      .required = false},
	     {.name = "owner", .type = "integer", .description = "The player that owns a new creature", .required = false},
	     {.name = "radians", .type = "number", .description = "The angle that a new creature faces", .required = false},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	mindLoad.writes = true;
	provider->Add(std::move(mindLoad), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		const auto pathParam = context.params.find("path");
		if (pathParam == context.params.end() || !pathParam->is_string())
		{
			return QueryResult::Error("editor.mind_load needs the path of a mind file");
		}
		const auto bytes = creaturemind::ReadFileBytes(pathParam->get<std::string>());
		const auto mind = bytes.has_value() ? editor::mind_loads::ReadMind(*bytes) : nullptr;
		if (mind == nullptr)
		{
			return QueryResult::Error("the mind file does not read");
		}
		if (bytes->size() > game_socket::k_MostEditorMindFileBytes)
		{
			return QueryResult::Error(fmt::format("a network game takes a mind file of at most {} KB",
			                                      game_socket::k_MostEditorMindFileBytes / 1024));
		}
		player_commands::EditorMindLoad load;
		auto position = PositionParam(context.params);
		if (const auto entity = EntityParam(context.params); entity.has_value())
		{
			load.use = player_commands::EditorMindLoad::Use::LoadInto;
			load.creature = *entity;
		}
		else if (position.has_value())
		{
			// The new creature has the body of the file, as the creature tools make it
			const auto body = creature_mind_body::FromMindFile(*mind);
			if (!body.has_value())
			{
				return QueryResult::Error("the species of the mind file is not one that the game knows");
			}
			if (const auto found = context.params.find("position"); found->is_array() && found->size() == 2)
			{
				position->y = editor::LandHeight({position->x, position->z});
			}
			load.use = player_commands::EditorMindLoad::Use::Spawn;
			load.spawn = {.species = static_cast<uint8_t>(body->species),
			              .owner = static_cast<uint8_t>(NumberParam(context.params, "owner").value_or(0.0f)),
			              .position = *position,
			              .radians = NumberParam(context.params, "radians").value_or(0.0f),
			              .scale = body->size.value_or(1.0f),
			              .alignment = body->alignment.value_or(0.0f),
			              .fatness = body->fatness,
			              .strength = body->strength,
			              .hasTattoos = body->tattoos.has_value(),
			              .tattoos = body->tattoos.value_or(creature_tattoo::Slots {})};
		}
		else
		{
			return QueryResult::Error("editor.mind_load needs the id of a creature or a position");
		}
		if (const auto refusal = editor::mind_loads::MindLoadRefusal(load, true))
		{
			return QueryResult::Error(std::string(*refusal));
		}
		if (!editor::mind_loads::SendMindLoad(*bytes, load))
		{
			return QueryResult::Error("the game socket is not open: the mind file did not go");
		}
		return QueryResult::Value({{"sent", true},
		                           {"bytes", bytes->size()},
		                           {"hash", fmt::format("{:016x}", editor::mind_loads::HashOf(*bytes))},
		                           {"use", load.use == player_commands::EditorMindLoad::Use::Spawn ? "spawn" : "load"},
		                           {"species", static_cast<int>(load.spawn.species)}});
	});
	auto scriptAct = Query(
	    "script",
	    "Does an act of the Scripts tab on the script machine, as the tab does it: at once in a game alone, or as a command "
	    "of the turns in a network game, which each computer does at the same turn and the game server takes only from a "
	    "player with the editor claim. Acts: start, stop_script, hold_all, continue_all, step, continue, hold, stop, "
	    "breakpoint, remove_breakpoints, set_global, set_local. force sends it also when the editor of this computer is "
	    "refused.",
	    {{.name = "act", .type = "string", .description = "The act", .required = true},
	     {.name = "id", .type = "integer", .description = "The script, the task or the global of the act", .required = false},
	     {.name = "script",
	      .type = "string",
	      .description = "The name of the script of a start or a stop_script, in place of the id",
	      .required = false},
	     {.name = "address", .type = "integer", .description = "The address of a breakpoint", .required = false},
	     {.name = "index",
	      .type = "integer",
	      .description = "The local of set_local, or 1 (on) or 0 (off) for a breakpoint",
	      .required = false},
	     {.name = "value", .type = "number", .description = "The new value of a variable, as a number", .required = false},
	     {.name = "parameters", .type = "array", .description = "The numbers that a start gives", .required = false},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	scriptAct.writes = true;
	provider->Add(std::move(scriptAct), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		using Act = player_commands::EditorScript::Act;
		constexpr std::array<std::pair<std::string_view, Act>, 12> k_Acts {{
		    {"start", Act::Start},
		    {"stop_script", Act::StopScript},
		    {"hold_all", Act::HoldAll},
		    {"continue_all", Act::ContinueAll},
		    {"step", Act::StepTask},
		    {"continue", Act::ContinueTask},
		    {"hold", Act::HoldTask},
		    {"stop", Act::StopTask},
		    {"breakpoint", Act::Breakpoint},
		    {"remove_breakpoints", Act::RemoveBreakpoints},
		    {"set_global", Act::SetGlobal},
		    {"set_local", Act::SetTaskVariable},
		}};
		const auto actParam = context.params.find("act");
		const auto found =
		    actParam != context.params.end() && actParam->is_string()
		        ? std::ranges::find(k_Acts, actParam->get<std::string>(), &std::pair<std::string_view, Act>::first)
		        : k_Acts.end();
		if (found == k_Acts.end())
		{
			return QueryResult::Error("editor.script needs an act that the Scripts tab knows");
		}
		player_commands::EditorScript act {.act = found->second};
		act.id = static_cast<uint32_t>(NumberParam(context.params, "id").value_or(0.0f));
		if (const auto name = context.params.find("script"); name != context.params.end() && name->is_string())
		{
			if (!Locator::vm::has_value())
			{
				return QueryResult::Error("the game has no script machine");
			}
			const auto& scripts = Locator::vm::value().GetScripts();
			const auto script = std::ranges::find(scripts, name->get<std::string>(), &lhvm::VMScript::name);
			if (script == scripts.end())
			{
				return QueryResult::Error("the program has no script with the name");
			}
			act.id = script->scriptId;
		}
		if (const auto address = context.params.find("address"); address != context.params.end() && address->is_number())
		{
			act.address = address->get<uint32_t>();
		}
		if (const auto index = context.params.find("index"); index != context.params.end() && index->is_number())
		{
			act.index = index->get<uint16_t>();
		}
		if (const auto value = NumberParam(context.params, "value"))
		{
			act.value = std::bit_cast<uint32_t>(*value);
		}
		if (const auto parameters = context.params.find("parameters");
		    parameters != context.params.end() && parameters->is_array())
		{
			for (const auto& parameter : *parameters)
			{
				if (!parameter.is_number())
				{
					return QueryResult::Error("each parameter is a number");
				}
				act.parameters.push_back(parameter.get<float>());
			}
		}
		if (const auto refusal = editor::script_acts::ScriptActRefusal(act))
		{
			return QueryResult::Error(std::string(*refusal));
		}
		if (developer_tools::NetworkSessionLive())
		{
			if (!Locator::playerCommandSystem::has_value())
			{
				return QueryResult::Error("the game has no commands");
			}
			Locator::playerCommandSystem::value().Submit(act);
			return QueryResult::Value({{"sent", true}});
		}
		const auto result = editor::script_acts::RunScriptAct(act);
		return QueryResult::Value({{"sent", false}, {"done", result.done}, {"refusal", result.refusal}, {"task", result.task}});
	});
	auto landScriptAct =
	    Query("land_script",
	          "Does an act of the Land script tab on its stepper, as the tab does it: at once in a game alone, or as a command "
	          "of the "
	          "turns in a network game. Acts: loaded_land, open (with a path: the text goes to every computer first), step, "
	          "continue, run_to, skip, restart, breakpoint. force sends it also when the editor of this computer is refused.",
	          {{.name = "act", .type = "string", .description = "The act", .required = true},
	           {.name = "command", .type = "integer", .description = "The command of run_to and breakpoint", .required = false},
	           {.name = "path", .type = "string", .description = "The land script file of an open", .required = false},
	           {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	landScriptAct.writes = true;
	provider->Add(std::move(landScriptAct), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		using Act = player_commands::EditorLandScript::Act;
		constexpr std::array<std::pair<std::string_view, Act>, 8> k_Acts {{
		    {"loaded_land", Act::LoadedLand},
		    {"open", Act::Open},
		    {"step", Act::Step},
		    {"continue", Act::Continue},
		    {"run_to", Act::RunTo},
		    {"skip", Act::Skip},
		    {"restart", Act::Restart},
		    {"breakpoint", Act::Breakpoint},
		}};
		const auto actParam = context.params.find("act");
		const auto found =
		    actParam != context.params.end() && actParam->is_string()
		        ? std::ranges::find(k_Acts, actParam->get<std::string>(), &std::pair<std::string_view, Act>::first)
		        : k_Acts.end();
		if (found == k_Acts.end() || !Locator::editorSystem::has_value())
		{
			return QueryResult::Error("editor.land_script needs an act that the Land script tab knows");
		}
		player_commands::EditorLandScript act {.act = found->second};
		act.command = static_cast<uint32_t>(NumberParam(context.params, "command").value_or(0.0f));
		std::string text;
		if (act.act == Act::Open)
		{
			const auto path = context.params.find("path");
			const auto read = path != context.params.end() && path->is_string()
			                      ? editor::script_acts::LandScriptText(path->get<std::string>())
			                      : std::nullopt;
			if (!read.has_value())
			{
				return QueryResult::Error("editor.land_script open needs the path of a land script that reads");
			}
			text = *read;
		}
		const std::span bytes(reinterpret_cast<const uint8_t*>(text.data()), text.size());
		if (developer_tools::NetworkSessionLive())
		{
			if (!Locator::playerCommandSystem::has_value())
			{
				return QueryResult::Error("the game has no commands");
			}
			if (act.act == Act::Open)
			{
				const auto hash = editor::script_acts::SendFile(game_socket::EditorScriptFile::Kind::LandScript, bytes);
				if (!hash.has_value())
				{
					return QueryResult::Error("the game socket is not open: the land script did not go");
				}
				act.file = *hash;
			}
			Locator::playerCommandSystem::value().Submit(act);
			return QueryResult::Value({{"sent", true}, {"hash", fmt::format("{:016x}", act.file)}});
		}
		auto& run = Locator::editorSystem::value().GetLandScriptRun();
		const auto result =
		    act.act == Act::Open
		        ? (editor::script_acts::ReadLandScript(run, "the file of the inspector", text),
		           editor::script_acts::LandScriptResult {.done = true})
		        : editor::script_acts::ApplyLandScript(act, {}, run, editor::script_acts::LocatorLandScriptWorld());
		return QueryResult::Value(
		    {{"sent", false}, {"done", result.done}, {"refusal", result.refusal}, {"commands", result.commands}});
	});
	auto scriptProgram = Query(
	    "script_program",
	    "Changes the program of the script machine, as the Scripts tab does it: compile (the text of a source file into the "
	    "script), open (a compiled .chl) or games (the program of the game again). In a network game the file goes to every "
	    "computer first, and each computer does the change at the same turn. force sends it also when the editor of this "
	    "computer is refused.",
	    {{.name = "use", .type = "string", .description = "compile, open or games", .required = true},
	     {.name = "path", .type = "string", .description = "The source file or the program file", .required = false},
	     {.name = "script", .type = "string", .description = "The name of the script of a compile", .required = false},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	scriptProgram.writes = true;
	provider->Add(std::move(scriptProgram), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		using Use = player_commands::EditorScriptProgram::Use;
		const auto useParam = context.params.find("use");
		const auto useName = useParam != context.params.end() && useParam->is_string() ? useParam->get<std::string>() : "";
		std::optional<Use> use;
		if (useName == "compile")
		{
			use = Use::Compile;
		}
		else if (useName == "open")
		{
			use = Use::Open;
		}
		else if (useName == "games")
		{
			use = Use::GamesProgram;
		}
		if (!use.has_value() || !Locator::vm::has_value() || !Locator::filesystem::has_value())
		{
			return QueryResult::Error("editor.script_program needs a use: compile, open or games");
		}
		player_commands::EditorScriptProgram change {.use = *use};
		std::vector<uint8_t> file;
		if (*use != Use::GamesProgram)
		{
			const auto path = context.params.find("path");
			if (path == context.params.end() || !path->is_string())
			{
				return QueryResult::Error("editor.script_program needs the path of a file");
			}
			try
			{
				file = Locator::filesystem::value().ReadAll(path->get<std::string>());
			}
			catch (const std::exception& error)
			{
				return QueryResult::Error(fmt::format("the file does not read: {}", error.what()));
			}
			if (*use == Use::Open && !editor::script_acts::ReadsProgram(file))
			{
				return QueryResult::Error("the program does not read");
			}
		}
		if (*use == Use::Compile)
		{
			const auto name = context.params.find("script");
			const auto& scripts = Locator::vm::value().GetScripts();
			const auto script = name != context.params.end() && name->is_string()
			                        ? std::ranges::find(scripts, name->get<std::string>(), &lhvm::VMScript::name)
			                        : scripts.end();
			if (script == scripts.end())
			{
				return QueryResult::Error("editor.script_program compile needs the name of a script of the program");
			}
			change.script = script->scriptId;
		}
		if (developer_tools::NetworkSessionLive())
		{
			if (!Locator::playerCommandSystem::has_value())
			{
				return QueryResult::Error("the game has no commands");
			}
			if (const auto kind = editor::script_acts::FileKindOf(change))
			{
				const auto hash = editor::script_acts::SendFile(*kind, file);
				if (!hash.has_value())
				{
					return QueryResult::Error("the game socket is not open, or the file is too large: the file did not go");
				}
				change.file = *hash;
			}
			Locator::playerCommandSystem::value().Submit(change);
			return QueryResult::Value({{"sent", true}, {"hash", fmt::format("{:016x}", change.file)}, {"bytes", file.size()}});
		}
		const auto result = editor::script_acts::RunProgram(change, file);
		return QueryResult::Value({{"sent", false}, {"done", result.done}, {"refusal", result.refusal}});
	});
	provider->Add(Query("script_state",
	                    "What the script tabs of the editor show and every computer of a network game must have the same: the "
	                    "tasks with their scripts and whether they are held, the breakpoints, the count of the instructions, "
	                    "and the land script of the stepper with its next command"),
	              [](const QueryContext& /*context*/) {
		              auto tasks = Json::array();
		              auto breakpoints = Json::array();
		              size_t instructions = 0;
		              if (Locator::vm::has_value())
		              {
			              const auto& vm = Locator::vm::value();
			              for (const auto& [id, task] : vm.GetTasks())
			              {
				              tasks.push_back({{"id", id}, {"script", task.name}, {"held", vm.IsTaskHeld(id)}});
			              }
			              for (const auto address : vm.GetBreakpoints())
			              {
				              breakpoints.push_back(address);
			              }
			              instructions = vm.GetInstructions().size();
		              }
		              Json land = nullptr;
		              if (Locator::editorSystem::has_value())
		              {
			              const auto& run = Locator::editorSystem::value().GetLandScriptRun();
			              const auto next = run.stepper.Next();
			              land = {{"name", run.name},
			                      {"commands", run.read.commands.size()},
			                      {"next", next.has_value() ? Json(*next) : Json(nullptr)}};
		              }
		              return QueryResult::Value({{"tasks", std::move(tasks)},
		                                         {"breakpoints", std::move(breakpoints)},
		                                         {"instructions", instructions},
		                                         {"land_script", std::move(land)}});
	              });
	provider->Add(
	    Query("script_reports",
	          "What the Scripts tab of this computer shows of its acts at the turns of a network game: the last act on "
	          "the script machine and the last change of the program, each with its turn, whether the machine did it, "
	          "the new task of a start and the line of the tab. Only the computer that sent the act has the report."),
	    [](const QueryContext& /*context*/) {
		    const auto toJson = [](const std::optional<editor::script_acts::ActReport>& report) -> Json {
			    if (!report.has_value())
			    {
				    return nullptr;
			    }
			    return {{"turn", report->turn},
			            {"done", report->done},
			            {"task", report->task},
			            {"text", report->text},
			            {"number", report->number}};
		    };
		    if (!Locator::editorSystem::has_value())
		    {
			    return QueryResult::Value({{"script", nullptr}, {"program", nullptr}, {"changes", 0}});
		    }
		    const auto& reports = Locator::editorSystem::value().GetScriptActReports();
		    return QueryResult::Value(
		        {{"script", toJson(reports.script)}, {"program", toJson(reports.program)}, {"changes", reports.changes}});
	    });
	auto landStroke = Query(
	    "land_stroke",
	    "Sends a stroke of the land brush as a command of the turns, as the land tools send it in a network game: each "
	    "computer raises, lowers or paints the land at the same turn, and the game server takes it only from a player with "
	    "the editor claim. force sends it also when the editor of this computer is refused.",
	    {{.name = "mode", .type = "string", .description = "raise, lower or paint", .required = true},
	     {.name = "position", .type = "point", .description = "[x, z] of the middle of the stroke", .required = true},
	     {.name = "inner",
	      .type = "number",
	      .description = "The inner radius in metres (10 when not given)",
	      .required = false},
	     {.name = "outer",
	      .type = "number",
	      .description = "The outer radius in metres (30 when not given)",
	      .required = false},
	     {.name = "step",
	      .type = "integer",
	      .description = "The altitude steps, 1 to 64 (4 when not given)",
	      .required = false},
	     {.name = "country", .type = "integer", .description = "The country of a paint (0 when not given)", .required = false},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	landStroke.writes = true;
	provider->Add(std::move(landStroke), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		constexpr std::array<std::pair<std::string_view, editor::land_brush::Mode>, 3> k_Modes {{
		    {"raise", editor::land_brush::Mode::Raise},
		    {"lower", editor::land_brush::Mode::Lower},
		    {"paint", editor::land_brush::Mode::Paint},
		}};
		const auto modeParam = context.params.find("mode");
		const auto modeName =
		    modeParam != context.params.end() && modeParam->is_string() ? modeParam->get<std::string>() : std::string();
		const auto mode = std::ranges::find(k_Modes, std::string_view(modeName), &decltype(k_Modes)::value_type::first);
		const auto position = PositionParam(context.params);
		if (mode == k_Modes.end() || !position.has_value())
		{
			return QueryResult::Error("editor.land_stroke needs a mode (raise, lower or paint) and a position");
		}
		const editor::land_brush::Brush brush {.mode = mode->second,
		                                       .innerRadius = NumberParam(context.params, "inner").value_or(10.0f),
		                                       .outerRadius = NumberParam(context.params, "outer").value_or(30.0f),
		                                       .step = static_cast<int>(NumberParam(context.params, "step").value_or(4.0f)),
		                                       .country =
		                                           static_cast<uint8_t>(NumberParam(context.params, "country").value_or(0.0f))};
		const glm::vec2 centre {position->x, position->z};
		const auto stroke = editor::land_edits::StrokeOf(brush, centre);
		if (const auto refusal = editor::land_edits::LandStrokeRefusal(stroke, std::nullopt))
		{
			return QueryResult::Error(std::string(*refusal));
		}
		editor::land_edits::SendLandStroke(stroke);
		return QueryResult::Value({{"sent", true}});
	});
	auto path = Query(
	    "path",
	    "Sends a footpath or a stream as a command of the turns, as the paths of the land tools send it in a network game: "
	    "each computer makes it at the same turn, and the game server takes it only from a player with the editor claim. "
	    "force sends it also when the editor of this computer is refused.",
	    {{.name = "kind", .type = "string", .description = "footpath or stream", .required = true},
	     {.name = "points", .type = "array", .description = "The points [[x, z], ...], 2 to 256", .required = true},
	     {.name = "first", .type = "integer", .description = "The building at the first point", .required = false},
	     {.name = "last", .type = "integer", .description = "The building at the last point", .required = false},
	     {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	path.writes = true;
	provider->Add(std::move(path), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		const auto kindParam = context.params.find("kind");
		const auto kindName =
		    kindParam != context.params.end() && kindParam->is_string() ? kindParam->get<std::string>() : std::string();
		const auto points = context.params.find("points");
		if ((kindName != "footpath" && kindName != "stream") || points == context.params.end() || !points->is_array())
		{
			return QueryResult::Error("editor.path needs a kind (footpath or stream) and the points");
		}
		player_commands::EditorPath command {
		    .kind = static_cast<uint8_t>(kindName == "stream" ? editor::paths::Kind::Stream : editor::paths::Kind::Footpath)};
		for (const auto& point : *points)
		{
			if (!point.is_array() || point.size() != 2 || !point[0].is_number() || !point[1].is_number())
			{
				return QueryResult::Error("editor.path needs each point as [x, z]");
			}
			command.points.emplace_back(point[0].get<float>(), point[1].get<float>());
		}
		const auto holder = [&context](std::string_view name) {
			const auto number = NumberParam(context.params, name);
			return number.has_value() ? static_cast<entt::entity>(static_cast<uint32_t>(*number)) : entt::entity {entt::null};
		};
		command.firstHolder = holder("first");
		command.lastHolder = holder("last");
		if (const auto refusal = editor::land_edits::PathRefusal(command))
		{
			return QueryResult::Error(std::string(*refusal));
		}
		editor::land_edits::SendPath(command);
		return QueryResult::Value({{"sent", true}});
	});
	auto creatureAct =
	    Query("creature_act",
	          "Sends an act of the creature tools as a command of the turns, as the tabs of the creature section send it in a "
	          "network game: each computer does it at the same turn, and the game server takes it only from a player with the "
	          "editor claim. force sends it also when the editor of this computer is refused.",
	          {{.name = "id",
	            .type = "integer",
	            .description = "The creature (none for an act on every creature)",
	            .required = false},
	           {.name = "act", .type = "string", .description = "The name of the act, such as \"sit down\"", .required = true},
	           {.name = "other", .type = "integer", .description = "The other thing of the act", .required = false},
	           {.name = "position", .type = "array", .description = "The place of the act [x, y, z]", .required = false},
	           {.name = "amount", .type = "number", .description = "The amount of the act", .required = false},
	           {.name = "index", .type = "integer", .description = "The index of the act in its list", .required = false},
	           {.name = "args", .type = "array", .description = "Up to six small numbers of the act", .required = false},
	           {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	creatureAct.writes = true;
	provider->Add(std::move(creatureAct), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		const auto nameParam = context.params.find("act");
		const auto name =
		    nameParam != context.params.end() && nameParam->is_string() ? nameParam->get<std::string>() : std::string();
		const auto kind = creature_edit_acts::ActNamed(name);
		if (!kind.has_value())
		{
			return QueryResult::Error("editor.creature_act needs the name of an act");
		}
		player_commands::EditorCreatureAct act {.creature = EntityParam(context.params).value_or(entt::null),
		                                        .act = static_cast<uint8_t>(*kind)};
		if (const auto other = context.params.find("other"); other != context.params.end() && other->is_number_integer())
		{
			act.other = static_cast<entt::entity>(other->get<uint32_t>());
		}
		if (const auto position = context.params.find("position");
		    position != context.params.end() && position->is_array() && position->size() == 3)
		{
			act.position = {(*position)[0].get<float>(), (*position)[1].get<float>(), (*position)[2].get<float>()};
		}
		act.amount = NumberParam(context.params, "amount").value_or(0.0f);
		if (const auto index = context.params.find("index"); index != context.params.end() && index->is_number_integer())
		{
			act.index = index->get<uint16_t>();
		}
		if (const auto args = context.params.find("args"); args != context.params.end() && args->is_array())
		{
			for (size_t i = 0; i < std::min(args->size(), act.args.size()); ++i)
			{
				act.args.at(i) = (*args)[i].get<uint8_t>();
			}
		}
		if (const auto refusal = editor::ActRefusal(act))
		{
			return QueryResult::Error(fmt::format("editor.creature_act refuses the act: {}", *refusal));
		}
		if (Locator::playerCommandSystem::has_value())
		{
			Locator::playerCommandSystem::value().Submit(act);
		}
		return QueryResult::Value({{"sent", true}});
	});
	auto cheat =
	    Query("cheat",
	          "Sends a debug cheat of the player of this computer as a command of the turns, as the cheats tab "
	          "of the editor does. The game server takes it only from a player with the editor claim. force sends it also when "
	          "the editor of this computer is refused.",
	          {{.name = "cheat", .type = "string", .description = "The name of the cheat", .required = true},
	           {.name = "on", .type = "boolean", .description = "On (the default) or off", .required = false},
	           {.name = "force", .type = "boolean", .description = "Send also when the editor is refused", .required = false}});
	cheat.writes = true;
	provider->Add(std::move(cheat), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		const auto nameParam = context.params.find("cheat");
		const auto name =
		    nameParam != context.params.end() && nameParam->is_string() ? nameParam->get<std::string>() : std::string();
		const auto found = std::ranges::find(debug_cheats::k_CheatNames, std::string_view(name));
		if (found == debug_cheats::k_CheatNames.end())
		{
			return QueryResult::Error("editor.cheat needs the name of a cheat");
		}
		const auto cheatOf = static_cast<debug_cheats::Cheat>(found - debug_cheats::k_CheatNames.begin());
		editor::SendDebugCheat(cheatOf, BoolParam(context.params, "on", true));
		return QueryResult::Value({{"sent", true}});
	});
	auto landSkip =
	    Query("land_skip",
	          "Sends the skip to a land of the story, 2 to 5, as a command of the turns, as the cheats tab of the "
	          "editor does. The story changes the land as at the end of the land before. Only a game of the story "
	          "on this computer alone does it. The skip to land 1 starts a new game, so only the editor does it.",
	          {{.name = "land", .type = "integer", .description = "The land of the story, 2 to 5", .required = true}});
	landSkip.writes = true;
	provider->Add(std::move(landSkip), [](const QueryContext& context) {
		if (auto refused = Refused(context.params))
		{
			return *refused;
		}
		const auto landParam = context.params.find("land");
		if (landParam == context.params.end() || !landParam->is_number_integer())
		{
			return QueryResult::Error("editor.land_skip needs the land, 2 to 5");
		}
		const auto land = landParam->get<int32_t>();
		if (!story_land_skip::GoesAsCommand(land))
		{
			return QueryResult::Error("editor.land_skip takes only the lands 2 to 5");
		}
		if (!editor::SendStoryLandSkip(land))
		{
			return QueryResult::Error("editor.land_skip: this game is not a game of the story on this computer alone");
		}
		return QueryResult::Value({{"sent", true}, {"land", land}});
	});
	return provider;
}

} // namespace openblack::inspector::game_providers
