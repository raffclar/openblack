/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "DeveloperTools.h"

/// The gate of the developer tools in the running game. It reads the network game of this computer and the account of
/// the lobby from the Locator.
namespace openblack::developer_tools
{

/// True from the join of a game server until the network session ends
[[nodiscard]] bool NetworkSessionLive();

/// True while a network session is live and the join ticket of the player of this computer has the editor claim
[[nodiscard]] bool LocalPlayerHasEditorClaim();

/// The editor right of the player of this computer (see EditorRightOf): the editor claim of the join ticket in a network
/// game, else the editor right of the account that is signed in to the lobby
[[nodiscard]] EditorRight LocalEditorRight();

/// True while a network session is live and the join ticket of the player of this computer has the debugger claim
[[nodiscard]] bool LocalPlayerHasDebuggerClaim();

/// The debugger right of the player of this computer (see DebuggerRightOf): the debugger claim of the join ticket in a
/// network game, else the debugger mark of the account that is signed in to the lobby
[[nodiscard]] DebuggerRight LocalDebuggerRight();

/// Whether the editor can open now (see EditorRefusalOf). It does not log and does not show the refusal line, thus the
/// game can call it in each frame.
[[nodiscard]] bool EditorCanOpen();

/// Shows the refusal line of the editor and the debug cheats to the player, in the incoming messages (see
/// ShowsRefusalLine). The game calls it for a refusal of this computer and for a refusal of the game server.
void ShowEditorRefusal();

/// What the debug windows show now (see GuiModeOf)
[[nodiscard]] GuiMode CurrentGuiMode();

/// What the Land generator tab of the editor shows now (see LandGeneratorTabOf). It does not log, thus the game can
/// call it in each frame.
[[nodiscard]] LandGeneratorTab CurrentLandGeneratorTab();

/// Whether the editor can open now (see EditorRefusalOf). The log tells each refusal. When the join ticket or the
/// signed-in account does not have the editor right, the player sees the refusal line. Thus call it only for an event.
[[nodiscard]] bool EditorAllowed();

/// Whether the developer action can run now (see RefusalOf). The log tells each refusal, thus call it only for an
/// event such as a key press or a request, not in each frame.
[[nodiscard]] bool Allowed(Action action);

} // namespace openblack::developer_tools
