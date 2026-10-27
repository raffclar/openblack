/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <LHVMTypes.h>

/// The kinds of script that the game starts by name. A game for one player starts all kinds. A network game starts only
/// the multiplayer scripts and the multiplayer help scripts. Thus no story script and no world script starts by name in
/// a network game, and each computer runs only its own help.
namespace openblack::script::start
{

/// The kinds of script that a game for one player starts by name: all kinds
inline constexpr auto k_SinglePlayerScripts =
    lhvm::ScriptType::Script | lhvm::ScriptType::Help | lhvm::ScriptType::ChallengeHelp | lhvm::ScriptType::TempleHelp |
    lhvm::ScriptType::TempleSpecial | lhvm::ScriptType::Multiplayer | lhvm::ScriptType::MultiplayerHelp;

/// The kinds of script that a network game starts by name
inline constexpr auto k_NetworkGameScripts = lhvm::ScriptType::Multiplayer | lhvm::ScriptType::MultiplayerHelp;

/// The kinds of script that the game starts by name, in a network game or in a game for one player
[[nodiscard]] constexpr lhvm::ScriptType StartableScripts(bool networkGame) noexcept
{
	return networkGame ? k_NetworkGameScripts : k_SinglePlayerScripts;
}

/// The kinds of script that the game starts by name now, from the clock's network game flag
[[nodiscard]] lhvm::ScriptType StartableScripts();

} // namespace openblack::script::start
