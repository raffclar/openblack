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
#include <utility>
#include <vector>

#include <GameSocket.h>

/// The facts that decide whether the worlds of the server and of a player can be the same: the build, the game data,
/// the land and its file, the map of the lobby and the seed. When the worlds part, the server compares the player's
/// facts with its own and names the first fact that differs. When no fact differs, the cause is in the game's logic.
namespace openblack::online::desync_facts
{

/// One fact that differs
struct FactDifference
{
	/// The name of the fact, such as "build"
	std::string fact;
	std::string server;
	std::string player;
	/// The difference in words, such as "the player loaded another land"
	std::string words;

	[[nodiscard]] bool operator==(const FactDifference&) const = default;
};

/// The facts as names and values, for the logs and the reports
[[nodiscard]] std::vector<std::pair<std::string, std::string>> Listed(const game_socket::SyncFacts& facts);

/// The first fact that differs, in this order: the build, the game data, the land, the file of the land, the map of the
/// lobby and the seed. A hash or a file that one side does not know does not count. The turn, the patch and the
/// language only tell more, and do not count.
[[nodiscard]] std::optional<FactDifference> FirstDifferent(const game_socket::SyncFacts& server,
                                                           const game_socket::SyncFacts& player);

} // namespace openblack::online::desync_facts
