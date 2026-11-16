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
#include <string_view>

#include "Commands/PlayerCommands.h"
#include "Debug/TownMiracles.h"
#include "Enums.h"

/// The acts of the Magic, the Weather and the Moon debug windows on the world of the game (debug_acts). The turn runs
/// them for a command. The windows use Do: in a network game it sends the command, and in a game of this computer alone
/// it runs the act at once for the first player.
namespace openblack::debug_acts
{

/// Runs an act of the Magic window for the player. Returns why nothing changed, none when the act occurred.
std::optional<std::string_view> RunMagicAct(PlayerNames player, const player_commands::DebugMagic& act);
/// Runs an act on the miracles of a town
debug::town_miracles::Outcome RunTownMiracle(const player_commands::DebugTownMiracle& act);
/// Runs an act of the Weather window. Returns why nothing changed, none when the act occurred.
std::optional<std::string_view> RunWeatherAct(const player_commands::DebugWeather& act);
/// Runs an act of the Moon window. A computer with no sky, such as the game server, changes only the clock.
std::optional<std::string_view> RunSkyAct(const player_commands::DebugSky& act);

/// In a network game, sends the act as a command of the turns. Else runs the act at once.
void Do(const player_commands::DebugMagic& act);
void Do(const player_commands::DebugWeather& act);
void Do(const player_commands::DebugSky& act);
/// Returns the outcome of an act that ran at once, none for an act that goes as a command. The game log gives the
/// result of a command.
std::optional<debug::town_miracles::Outcome> Do(const player_commands::DebugTownMiracle& act);

} // namespace openblack::debug_acts
