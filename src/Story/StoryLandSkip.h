/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>

/// The debug cheat of the editor that skips to a land of the story. It is not in the original game, and a normal player
/// never sees it: only a player with the editor right opens the editor.
///
/// The story has five lands. The top script of the story runs the control script of each land in turn. Between two
/// lands, it stops the scripts of the last land and loads the map of the next land. A skip to the second land or a later
/// land starts the top script again at that change of land. Thus the land loads and its scripts start as in the story.
/// The first land has no such change: a skip to it starts a new game, as the menu does.
///
/// These rules keep no state, so that the game and the tests use them both.
namespace openblack::story_land_skip
{

inline constexpr int32_t k_FirstLand = 1;
inline constexpr int32_t k_LastLand = 5;

/// The script of the story that runs the control script of each land in turn
inline constexpr std::string_view k_TopScript = "LandControlAll";

/// The name of the control script of a land
inline constexpr std::string_view k_ControlScriptPrefix = "LandControl";

/// How the skip goes to the land
enum class Path : uint8_t
{
	/// A new game starts on the first land, as the menu starts it
	NewGame,
	/// The top script of the story starts again at the change of land before the land
	TopScript,
};

/// Whether the cheat offers the land: a land of the story, 1 to 5
[[nodiscard]] constexpr bool Offers(int32_t land) noexcept
{
	return land >= k_FirstLand && land <= k_LastLand;
}

/// How the skip goes to a land that the cheat offers
[[nodiscard]] constexpr Path PathTo(int32_t land) noexcept
{
	return land == k_FirstLand ? Path::NewGame : Path::TopScript;
}

/// Whether a command of a turn takes the skip to the land. Only the skip to a land after the first goes as a command.
[[nodiscard]] constexpr bool GoesAsCommand(int32_t land) noexcept
{
	return Offers(land) && PathTo(land) == Path::TopScript;
}

/// The control script that the top script runs just before it changes to the land. Nothing for the first land and for a
/// land that the cheat does not offer.
[[nodiscard]] inline std::optional<std::string> ControlScriptBefore(int32_t land)
{
	if (!GoesAsCommand(land))
	{
		return std::nullopt;
	}
	return std::string(k_ControlScriptPrefix) + std::to_string(land - 1);
}

/// The address where the top script starts again: the instruction after the run of the control script of the land
/// before. The change of land starts at that instruction.
[[nodiscard]] constexpr uint32_t StartAfterRun(uint32_t runAddress) noexcept
{
	return runAddress + 1;
}

/// The game that the player plays now, as the cheat sees it
struct GameFacts
{
	/// A game on the network. It has no lands of the story.
	bool networkGame {false};
	/// A skirmish on a land of the Gods' Playground
	bool skirmish {false};
	/// The number of the land that the map script gave
	int32_t landNumber {0};
};

/// Whether the editor shows the cheat: only in a game of the story on this computer alone
[[nodiscard]] constexpr bool Shown(const GameFacts& game) noexcept
{
	return !game.networkGame && !game.skirmish && Offers(game.landNumber);
}

/// Whether the world does the skip of a command at a turn. A network game never does it, and the game server refuses the
/// command.
[[nodiscard]] constexpr bool WorldDoes(const GameFacts& game, int32_t land) noexcept
{
	return Shown(game) && GoesAsCommand(land);
}

} // namespace openblack::story_land_skip
