/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>

#include "Commands/PlayerCommands.h"

/// The acts of the debug windows and of the debug menus on the world of the game (debug_acts). The turn runs them for a
/// command. The windows and the menus use Do: in a network game it sends the command, and in a game of this computer
/// alone it runs the act at once.
namespace openblack::debug_acts
{

/// Runs an act on the advisors of this computer. Returns false when the act is refused or the advisors did not load.
bool RunAdvisorAct(const player_commands::DebugAdvisor& act);
/// Runs a change of the debug menus on the world. Returns false when the change is refused.
bool RunWorldAct(const player_commands::DebugWorld& act);
/// Runs a line of land script commands. Returns the error of the line, none when the line ran.
std::optional<std::string> RunConsoleLine(const player_commands::DebugConsoleLine& line);

/// Whether the acts go as commands of the turns: true while a network game is live
[[nodiscard]] bool SendsCommands();

/// In a network game, sends the act as a command of the turns. Else runs the act at once.
void Do(const player_commands::DebugAdvisor& act);
void Do(const player_commands::DebugWorld& act);
/// Returns the error of a line that ran at once and failed. A line that goes as a command has its result in the game log.
std::optional<std::string> Do(const player_commands::DebugConsoleLine& line);

} // namespace openblack::debug_acts
