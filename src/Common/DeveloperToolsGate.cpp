/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DeveloperToolsGate.h"

#include <spdlog/spdlog.h>

#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "Online/NetworkSession.h"

namespace openblack::developer_tools
{

namespace
{

/// The editor right that a tool run gives to a signed-in account, or none. Only a developer build reads it.
[[nodiscard]] std::optional<bool> SimAccountEditor()
{
	if constexpr (k_Built)
	{
		if (Locator::config::has_value())
		{
			return Locator::config::value().simAccountEditor;
		}
	}
	return std::nullopt;
}

/// The debugger right that a tool run gives to a signed-in account, or none. Only a developer build reads it.
[[nodiscard]] std::optional<bool> SimAccountDebugger()
{
	if constexpr (k_Built)
	{
		if (Locator::config::has_value())
		{
			return Locator::config::value().simAccountDebugger;
		}
	}
	return std::nullopt;
}

/// Whether the rules are the rules of a developer build. A tool run can ask for the rules of a build that players get.
[[nodiscard]] bool BuiltRule()
{
	return k_Built && !SimAccountEditor().has_value() && !SimAccountDebugger().has_value();
}

/// The editor right of the account that is signed in to the lobby, or none when no account is signed in
[[nodiscard]] std::optional<bool> AccountEditor()
{
	if (const auto sim = SimAccountEditor(); sim.has_value())
	{
		return sim;
	}
	if (SimAccountDebugger().has_value())
	{
		// A tool run that gives only the debugger right has a signed-in account with no editor right
		return false;
	}
	if (!Locator::lobbyClient::has_value())
	{
		return std::nullopt;
	}
	return Locator::lobbyClient::value().GetAccountEditor();
}

/// The debugger mark of the account that is signed in to the lobby, or none when no account is signed in
[[nodiscard]] std::optional<bool> AccountDebugger()
{
	if (const auto sim = SimAccountDebugger(); sim.has_value())
	{
		return sim;
	}
	if (SimAccountEditor().has_value())
	{
		// A tool run that gives only the editor right has a signed-in account with no debugger mark
		return false;
	}
	if (!Locator::lobbyClient::has_value())
	{
		return std::nullopt;
	}
	return Locator::lobbyClient::value().GetAccountDebugger();
}

} // namespace

bool NetworkSessionLive()
{
	return Locator::networkGame::has_value() && Locator::networkGame::value().InGame();
}

bool LocalPlayerHasEditorClaim()
{
	if (!NetworkSessionLive())
	{
		return false;
	}
	const auto* session = Locator::networkGame::value().Session();
	return session != nullptr && session->GetJoined().has_value() && session->GetJoined()->editor;
}

EditorRight LocalEditorRight()
{
	return EditorRightOf(NetworkSessionLive(), LocalPlayerHasEditorClaim(), AccountEditor());
}

bool LocalPlayerHasDebuggerClaim()
{
	if (!NetworkSessionLive())
	{
		return false;
	}
	const auto* session = Locator::networkGame::value().Session();
	return session != nullptr && session->GetJoined().has_value() && session->GetJoined()->debugger;
}

DebuggerRight LocalDebuggerRight()
{
	return DebuggerRightOf(NetworkSessionLive(), LocalPlayerHasDebuggerClaim(), AccountDebugger());
}

bool EditorCanOpen()
{
	return !EditorRefusalOf(BuiltRule(), NetworkSessionLive(), LocalEditorRight()).has_value();
}

void ShowEditorRefusal()
{
	if (!ShowsRefusalLine(BuiltRule(), NetworkSessionLive(), LocalEditorRight()) ||
	    !Locator::incomingMessageSystem::has_value())
	{
		return;
	}
	const auto language = Locator::config::has_value() ? Locator::config::value().language : GameLanguage::UkEnglish;
	Locator::incomingMessageSystem::value().Add(incoming_messages::Kind::Chat, EditorRefusedLineOf(language), 0);
	// The test tools find the words "The player sees the refusal line"
	if (auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "The player sees the refusal line of the editor and the debug cheats");
	}
}

GuiMode CurrentGuiMode()
{
	return GuiModeOf(BuiltRule(), NetworkSessionLive(), LocalEditorRight(), LocalDebuggerRight());
}

bool EditorAllowed()
{
	const auto refusal = EditorRefusalOf(BuiltRule(), NetworkSessionLive(), LocalEditorRight());
	if (refusal.has_value())
	{
		if (auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_WARN(logger, "The game refuses the editor: {}", *refusal);
		}
		ShowEditorRefusal();
		return false;
	}
	return true;
}

bool Allowed(Action action)
{
	const auto refusal = RefusalOf(k_Built, NetworkSessionLive());
	if (refusal.has_value())
	{
		if (auto logger = spdlog::get("game"))
		{
			SPDLOG_LOGGER_WARN(logger, "The game refuses the {}: {}", NameOf(action), *refusal);
		}
		return false;
	}
	return true;
}

} // namespace openblack::developer_tools
