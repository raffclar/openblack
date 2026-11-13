/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "Common/DebugCheats.h"
#include "Common/DeveloperToolsGate.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
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
	return QueryResult::Error("the editor is refused: only a staff player opens the editor in a network game");
}

} // namespace

namespace openblack::inspector::game_providers
{

std::unique_ptr<ProviderInterface> EditorProvider()
{
	auto provider = std::make_unique<FunctionProvider>("editor");
	provider->Add(Query("state", "The editor of this computer: whether it is open, whether a network game is live, whether "
	                             "the player of this computer is staff, and the debug cheats of that player"),
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
		                  {"staff", developer_tools::LocalPlayerIsStaff()},
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
	    "does it at the same turn, and the game server takes it only from a staff player. force sends it also when the "
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
		Locator::playerCommandSystem::value().Submit(command);
		return QueryResult::Value({{"sent", true}});
	});

	auto cheat =
	    Query("cheat",
	          "Sends a debug cheat of the player of this computer as a command of the turns, as the cheats tab "
	          "of the editor does. The game server takes it only from a staff player. force sends it also when "
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
