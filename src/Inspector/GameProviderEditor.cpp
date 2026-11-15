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
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <glm/vec2.hpp>

#include "Common/CreatureEditValues.h"
#include "Common/DebugCheats.h"
#include "Common/DeveloperToolsGate.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "Editor/EditorCreatureEdits.h"
#include "Editor/EditorEntities.h"
#include "Editor/EditorNetworkEdits.h"
#include "GameProviderShared.h"
#include "Locator.h"

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
	return QueryResult::Error("the editor is refused: only a player with the editor claim opens the editor in a network game");
}

} // namespace

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> EditorProvider()
{
	auto provider = std::make_unique<FunctionProvider>("editor");
	provider->Add(Query("state", "The editor of this computer: whether it is open, whether a network game is live, whether "
	                             "the player of this computer has the editor claim, and the debug cheats of that player"),
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
		                  {"editor", developer_tools::LocalPlayerHasEditorClaim()},
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
	    {{.name = "kind", .type = "string", .description = "move, turn, place or remove", .required = true},
	     {.name = "id", .type = "integer", .description = "The thing of a move, a turn or a removal", .required = false},
	     {.name = "position",
	      .type = "point",
	      .description = "[x, z] or [x, y, z] of a move or a placement",
	      .required = false},
	     {.name = "radians", .type = "number", .description = "The angle of a turn or a placement", .required = false},
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
		constexpr std::array<std::pair<std::string_view, EditorEdit::Kind>, 4> k_Kinds {{
		    {"place", EditorEdit::Kind::Place},
		    {"move", EditorEdit::Kind::Move},
		    {"turn", EditorEdit::Kind::Turn},
		    {"remove", EditorEdit::Kind::Remove},
		}};
		const auto kind =
		    std::ranges::find(k_Kinds, std::string_view(kindName), &std::pair<std::string_view, EditorEdit::Kind>::first);
		if (kind == k_Kinds.end())
		{
			return QueryResult::Error("editor.network_edit needs a kind: move, turn, place or remove");
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
	return provider;
}

} // namespace openblack::inspector::game_providers
